#ifndef _SEN55_H_
#define _SEN55_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10;
    float humidity;
    float temperature;
    float vocIndex;
    float noxIndex;
} tSen55_data;

void sen55_init( void );
float sen55_calculateAQI( uint16_t pm25, uint16_t pm10 );
bool sen55_getSensorData( tSen55_data *data );

#endif /* _SEN55_H_ */