#ifndef _TIME_SYNC_H_
#define _TIME_SYNC_H_

typedef struct {
    char country[50];
    char city[50];
    char timezone[50];
    char latitude[10];
    char longitude[10];
} tTimeSync_localizationInfo;

void timeSync_init( void );

#endif /* _TIME_SYNC_H_ */