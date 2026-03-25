#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

void app_init(void);
void app_task(void);

/* Exposed for testing only */
bool uart_send_frame(uint8_t msg_id, const uint8_t *payload, uint8_t length);
extern volatile bool uart_message_ready;
extern uint8_t       uart_msg_id;
extern uint8_t       uart_msg_len;
extern uint8_t       uart_payload[];

#ifdef __cplusplus
}
#endif

#endif