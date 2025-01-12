#ifndef _LVGL_WRAPPER_H_
#define _LVGL_WRAPPER_H_

void lvglWrapper_init( void );

void lvglWrapper_displayInit( void );

void lvglWrapper_createClockScreen( void );
void lvglWrapper_updateClockLabel( const char *timeBuffer );
void lvglWrapper_updateDateLabel( const char *dateBuffer );
void lvglWrapper_updateLocationLabel( const char *locationBuffer );

#endif /* _LVGL_WRAPPER_H_ */