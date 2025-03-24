/*   main.c
*
*   Created on: 2/22/2025
*   Created by: Noa Johnson
*   
*   Description: This is the main application file for the Temp Sense Service. 
*   This file will initialize the Temp Sense Service and read the temperature 
*   from the sensor.
*/


#include "Temp_Acc_Sense_Service.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void){
    if(SENS_INIT() != 0){
        LOG_ERR("Failed to initialize Temp Sense Service");
        return -1;
    }

    while(1){
        double temp = ReadTemperature();
        if(temp < 0){
            LOG_ERR("Failed to read temperature");
            return -1;
        }
        LOG_INF("Temperature: %f F", temp);


        struct axis_data *accel_data;
        if (ReadAcceleration(accel_data) != 0) {
            LOG_ERR("Failed to acceleration temperature");
            return -1;
        }
        LOG_INF("Acceleration (g): ");
        LOG_INF("X=%f", accel_data->x);
        LOG_INF("Y=%f", accel_data->y);
        LOG_INF("Z=%f", accel_data->z);

        k_sleep(K_MSEC(1000));
    }
    return 0;
}