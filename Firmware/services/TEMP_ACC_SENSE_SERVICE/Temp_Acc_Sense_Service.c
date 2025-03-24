/*
*   Temp_Sense_Service.c
*
*   Created on: 2/22/2025
*   Created by: Noa Johnson
*   
*   Description: This is the file that contains all the functions relating
*   to the Temp Sense Service. This file will initialize the Temp Sense Service
*   and read the temperature from the sensor.
*/

#include "Temp_Acc_Sense_Service.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tass, LOG_LEVEL_DBG);

/*          Macros          */

// Node Labels
#define SPI1_NODE       DT_NODELABEL(spi1)
#define GPIO_NODE       DT_NODELABEL(gpiod)

//  Initilization Register Addresses
#define IIS3DWB_FIFO_CTRL3  0x09
#define IIS3DWB_FIFO_CTRL4  0x0A
#define IIS3DWB_WHO_AM_I    0x0F 
#define IIS3DWB_CTRL1_XL    0x10
#define IIS3DWB_CTRL3_C     0x12
#define IIS3DWB_CTRL4_C     0x13  
#define IIS3DWB_CTRL6_C     0x15

//  Data Register Addresses
#define IIS3DWB_OUT_TEMP_L  0x20    //  Temperature Low Data Register
#define IIS3DWB_OUT_TEMP_H  0x21    //  Temperature High Data Register    
#define IIS3DWB_OUTX_L_XL   0x28    //  Accelerometer X Axis Low Data Register
#define IIS3DWB_OUTX_H_XL   0x29    //  Accelerometer X Axis High Data Register
#define IIS3DWB_OUTY_L_XL   0x2A    //  Accelerometer Y Axis Low Data Register
#define IIS3DWB_OUTY_H_XL   0x2B    //  Accelerometer Y Axis High Data Register
#define IIS3DWB_OUTZ_L_XL   0x2C    //  Accelerometer Z Axis Low Data Register
#define IIS3DWB_OUTZ_H_XL   0x2D    //  Accelerometer Z Axis High Data Register

//  Data Register Write Values
#define IIS3DWB_RESET_SET       0x01
#define IIS3DWB_BDU_SET         0x40
#define IIS3DWB_ACC_SET         0xAC
#define IIS3DWB_FS_SET          0x03
#define IIS3DWB_CTRL1_XL_SET    0xA0

// Constant Values
#define IIS3DWB_READ_BIT    0x80
#define IIS3DWB_ID          0x7B
#define SPI_BUS_FREQ        1000000



/*          Static        */


// Config for SPI Procotol
static const struct spi_config spi_cfg = {
    .frequency = SPI_BUS_FREQ,
    .operation = (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8)),  
    .cs = NULL
};

// Device pointer for the spi1 node
static const struct device *spi1_dev = DEVICE_DT_GET(SPI1_NODE);


static int read_register(const struct device *dev, uint8_t reg, uint8_t *value) {
    uint8_t tx_buf_data[] = {reg | IIS3DWB_READ_BIT, 0};
    uint8_t rx_buf_data[2];
    struct spi_buf tx_buf = {.buf = tx_buf_data, .len = sizeof(tx_buf_data)};
    struct spi_buf rx_buf = {.buf = rx_buf_data, .len = sizeof(rx_buf_data)};
    struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

    if (spi_transceive(dev, &spi_cfg, &tx, &rx) != 0) {
        return -1;
    }
    *value = rx_buf_data[1];
    return 0;
}

/*
*   Function: verify_register
*   -------------------------

*   This function will verify that the register has been set correctly.
*   Note: This function is only used for initialization.
*   
*   Parameters:
*   ---------------
*   dev: The device pointer
*   reg: The register to verify 
*   value: The value to verify
*   expected_value: The expected value of the register
*   mask: The mask to apply to the register
*
*/
static int verify_register(const struct device *dev, uint8_t reg, uint8_t value, uint8_t expected_value, uint8_t mask) {
    uint8_t reg_value;
    if (read_register(dev, reg, &reg_value) != 0) {
        LOG_ERR("Failed to read register 0x%02x", reg);
        return -1;
    }
    if ((reg_value & mask) != (expected_value & mask)) {
        LOG_ERR("Register 0x%02x verification failed. Expected: 0x%02x, Got: 0x%02x (mask: 0x%02x)",
                reg, expected_value, reg_value, mask);
        return -1;
    }
    
    LOG_INF("Register 0x%02x verified: 0x%02x", reg, reg_value);
    return 0;
}

/*          Global          */

Global int SENS_INIT(void)
{
    // Variable that holds temporary register values
    uint8_t reg_value;

    // Initial Check for SPI1 device
    if (!device_is_ready(spi1_dev)) {
        LOG_ERR("SPI1 device not ready");
        return -1;
    }

    // Initial Communication Check to verify device
    uint8_t whoami = 0;
    // Checks for issues with SPI communication protocol
    if (read_register(spi1_dev, IIS3DWB_WHO_AM_I, &whoami) != 0) {
        LOG_ERR("Failed to read WHO_AM_I");
        return -1;
    }
    // Check for incorrect device ID
    if (whoami != IIS3DWB_ID) {
        LOG_ERR("Wrong device ID: expected 0x%02x, got 0x%02x", IIS3DWB_ID, whoami);
        return -1;
    }
    LOG_INF("Correct WHO_AM_I: 0x%02x", whoami);

    // Creating Buffers for resetting the device
    uint8_t tx_reset[] = {IIS3DWB_CTRL3_C, IIS3DWB_RESET_SET};  // Set RESET bit
    struct spi_buf tx_buf = {.buf = tx_reset, .len = sizeof(tx_reset)};
    struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    LOG_INF("Initializing IIS3DWB sensor");

    // Reset device
    tx_buf.buf = tx_reset;
    tx_buf.len = sizeof(tx_reset);
    if (spi_write(spi1_dev, &spi_cfg, &tx) != 0) {
        LOG_ERR("Failed to reset device");
        return -1;
    }

    k_msleep(10);  // Wait for reset

    // Set FIFO batch rate 
    uint8_t tx_fifo[] = {IIS3DWB_FIFO_CTRL3, 0x0A};
    tx_buf.buf = tx_fifo;
    tx_buf.len = sizeof(tx_fifo);
    if (spi_write(spi1_dev, &spi_cfg, &tx) != 0){
        LOG_ERR("Failed to set/verify FIFO bypass mode. Read: 0x%02x", reg_value);
        return -1;
    }

    /* Enable Block Data Update (BDU)
        Determines if the output registers are updated
        continuously or only after the entire output
        register has been read.
        * 0: Continuous update
        * 1: Output registers not updated until MSB and LSB have been read
    */
    uint8_t tx_bdu[] = {IIS3DWB_CTRL3_C, IIS3DWB_BDU_SET}; 
    tx_buf.buf = tx_bdu;
    tx_buf.len = sizeof(tx_bdu);
    if (spi_write(spi1_dev, &spi_cfg, &tx) != 0 ||
        read_register(spi1_dev, IIS3DWB_CTRL3_C, &reg_value) != 0 ||
        (reg_value & IIS3DWB_BDU_SET) != IIS3DWB_BDU_SET) {
        LOG_ERR("Failed to set/verify BDU. Read: 0x%02x", reg_value);
        return -1;
    }
    LOG_DBG("BDU mode verified: 0x%02x", reg_value);


    // Set full scale (±2g)
    uint8_t tx_fs[] = {IIS3DWB_CTRL1_XL, IIS3DWB_CTRL1_XL_SET};  // Set FS bits
    tx_buf.buf = tx_fs;
    tx_buf.len = sizeof(tx_fs);
    if (spi_write(spi1_dev, &spi_cfg, &tx) != 0) {
        LOG_ERR("Failed to set full scale");
        return -1;
    }
    
    /* Verify CTRL1_XL register
        Determines if the CTRL1_XL peripheral registers
        is set correctly on the sensor.
    */
    read_register(spi1_dev, IIS3DWB_CTRL1_XL, &reg_value);
    LOG_INF("Register 0x%02x set to 0x%02x", IIS3DWB_CTRL1_XL, reg_value);


    LOG_INF("IIS3DWB initialization complete");
    return 0;
}

Global double ReadTemperature(void)
{
    uint8_t tx_temp_l[] = {IIS3DWB_OUT_TEMP_L | IIS3DWB_READ_BIT, 0};
    uint8_t tx_temp_h[] = {IIS3DWB_OUT_TEMP_H | IIS3DWB_READ_BIT, 0};
    uint8_t rx_temp_l[sizeof(tx_temp_l)];
    uint8_t rx_temp_h[sizeof(tx_temp_h)];
    struct spi_buf tx_buf[] = {
        {.buf = tx_temp_l, .len = sizeof(tx_temp_l)},
        {.buf = tx_temp_h, .len = sizeof(tx_temp_h)}
    };
    struct spi_buf rx_buf[] = {
        {.buf = rx_temp_l, .len = sizeof(rx_temp_l)},
        {.buf = rx_temp_h, .len = sizeof(rx_temp_h)}
    };
    struct spi_buf_set tx = {.buffers = tx_buf, .count = 1};
    struct spi_buf_set rx = {.buffers = rx_buf, .count = 1};

    // Read low byte
    if (spi_transceive(spi1_dev, &spi_cfg, &tx, &rx) != 0) {
        LOG_ERR("Failed to read temperature low byte");
        return -1.0;
    }

    // Read high byte
    tx.buffers = &tx_buf[1];
    rx.buffers = &rx_buf[1];
    if (spi_transceive(spi1_dev, &spi_cfg, &tx, &rx) != 0) {
        LOG_ERR("Failed to read temperature high byte");
        return -1.0;
    }

    // Combine bytes and convert to temperature
    int16_t raw_temp = (rx_temp_h[1] << 8) | rx_temp_l[1];
    
    double temperature = (double)raw_temp / 256.0 + 25.0;
    // Celsius to Fahrenheit Conversion
    temperature = 1.8 * temperature + 32.0;

    return temperature;
}

Global int ReadAcceleration(struct axis_data* data) {
    uint8_t tx_buf_data[14] = {
        IIS3DWB_OUTX_L_XL | IIS3DWB_READ_BIT,  // Register address + read bit
        0, 0, 0, 0, 0, 0                        // Dummy bytes for reading 6 bytes of data
    };
    uint8_t rx_buf_data[7];  // 1 byte address + 6 bytes data
    
    struct spi_buf tx_buf = {
        .buf = tx_buf_data,
        .len = sizeof(rx_buf_data)
    };
    struct spi_buf rx_buf = {
        .buf = rx_buf_data,
        .len = sizeof(rx_buf_data)
    };
    
    struct spi_buf_set tx = {
        .buffers = &tx_buf,
        .count = 1
    };
    struct spi_buf_set rx = {
        .buffers = &rx_buf,
        .count = 1
    };

    // Read all acceleration registers in one transfer
    if (spi_transceive(spi1_dev, &spi_cfg, &tx, &rx) != 0) {
        LOG_ERR("Failed to read acceleration data");
        return -1;
    }

    // Combine bytes into 16-bit values (little endian)
    int16_t raw_x = (rx_buf_data[2] << 8) | rx_buf_data[1];
    int16_t raw_y = (rx_buf_data[4] << 8) | rx_buf_data[3];
    int16_t raw_z = (rx_buf_data[6] << 8) | rx_buf_data[5];

    // Convert to g's (±2g scale)
    // Sensitivity is typically 0.061 mg/LSB for ±2g range
    const double sensitivity = 0.061 / 1000.0;  // Convert mg to g
    data->x = (double)raw_x * sensitivity;
    data->y = (double)raw_y * sensitivity;
    data->z = (double)raw_z * sensitivity;


    return 0;
}