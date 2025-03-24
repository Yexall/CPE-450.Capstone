#ifndef Temp_Acc_Sense_Service_h
#define Temp_Acc_Sense_Service_h

#include "General_Includes.h"

// Structure definition
Global struct axis_data {
    double x;
    double y;
    double z;
};

Global int SENS_INIT(void);
Global double ReadTemperature(void);
Global int ReadAcceleration(struct axis_data* data);



#endif