#ifndef APP_H 
#define APP_H




/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif

/* One-time initialization for this module */
void App_Init(void);

/* Handle incoming frames and actions */
void App_Task(void);

   
#ifdef __cplusplus
}
#endif

#endif
