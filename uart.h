#ifndef __HAL_DRIVER_UART_H
#define __HAL_DRIVER_UART_H

#include "stm32f1xx_hal.h"
#include "loopbuf.h"
#include "platform.h"

#define UART1_LOOP_BUFFER_SIZE 128
#define UART2_LOOP_BUFFER_SIZE 1024
#define UART3_LOOP_BUFFER_SIZE 1024
#define UART4_LOOP_BUFFER_SIZE 1024

/*串口读写接口注册*/
#define UART_READ_WRITE_REGSTER(u) \
static void u##_write(uint8_t *data, uint16_t size) \
{\
    if(data == NULL || size <= 0) {\
        LogErrorPrefix("%s,line=%d\r\n",__FUNCTION__,__LINE__);\
        return;\
    }\
    huart_write(&u, data, size);\
}\
static uint16_t u##_read(uint8_t *data)\
{\
    uint16_t len = 0;\
    if(data == NULL) {\
        LogErrorPrefix("%s,line=%d\r\n",__FUNCTION__,__LINE__);\
        return 0;\
    }\
    xSemaphoreTake(u.loop_buffer->lock,pdMS_TO_TICKS(portMAX_DELAY));\
    len = loop_buffer_use(u.loop_buffer);\
    if(len > 0) {\
        loop_buffer_get(u.loop_buffer, data, len);\
    }\
    xSemaphoreGive(u.loop_buffer->lock);\
    return len;\
}

/*串口初始化注册*/
#define UART_INIT_REGISTER(u, h, s)\
do{\
    u.huart = &h;\
    u.loop_buffer = init_loop_buffer(s);\
    u.lock = xSemaphoreCreateMutex();\
    u.write = u##_write;\
    u.read = u##_read;\
    u.recv_status = HAL_UART_Receive_IT(u.huart, u.buff, 1);\
}while(0)

/*通用串口结构体*/
struct comm_uart{
    UART_HandleTypeDef *huart;
    struct loop_buffer *loop_buffer;
    uint8_t buff[1];
    QueueHandle_t lock;
    HAL_StatusTypeDef recv_status;
    void (*write)(uint8_t *data, uint16_t size);
    uint16_t (*read)(uint8_t *data);
};

/*对外模块提供的串口接口*/
struct comm_uart uart1;
//struct comm_uart uart2;
//struct comm_uart uart3;
//struct comm_uart uart4;

void init_uart();
int cmdline_init();

#endif /*__HAL_DRIVER_UART_H*/