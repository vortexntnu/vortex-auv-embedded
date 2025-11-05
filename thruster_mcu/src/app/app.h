#ifndef APP_H 
#define APP_H




/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif

/* One-time initialization for this module */
void app_init(void);

/* Handle incoming frames and actions */
void app_task(void);

   
#ifdef __cplusplus
}
#endif

#endif
