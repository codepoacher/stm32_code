
#include "uart.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;

/*串口中断回调函数*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        loop_buffer_put(uart1.loop_buffer, uart1.buff, 1);
        uart1.recv_status = HAL_UART_Receive_IT(huart, uart1.buff, 1);
    } else if(huart->Instance == USART2) {
        //loop_buffer_put(uart2.loop_buffer, uart2.buff, 1);
        //uart2.recv_status = HAL_UART_Receive_IT(huart, uart2.buff, 1);
    } else if(huart->Instance == USART3) {
        //loop_buffer_put(uart3.loop_buffer, uart3.buff, 1);
        //uart3.recv_status = HAL_UART_Receive_IT(huart, uart3.buff, 1);
    } else if(huart->Instance == UART4) {
        //loop_buffer_put(uart4.loop_buffer, uart4.buff, 1);
        //uart4.recv_status = HAL_UART_Receive_IT(huart, uart4.buff, 1);
    }
}

//防止不能进入接收中断,如果用轮询方式接收，则不调用此函数
static HAL_StatusTypeDef huart_write(struct comm_uart *uart, uint8_t *pData, uint16_t Size)
{
    uint32_t ret;

    xSemaphoreTake(uart->lock,pdMS_TO_TICKS(portMAX_DELAY));
    while(1) {
        //ret = HAL_UART_Transmit_IT(uart->huart, pData, Size);
        ret = HAL_UART_Transmit(uart->huart, pData, Size, 0xffff);
        if(ret == HAL_OK) {
            break;
        }
    }

    while(uart->recv_status != HAL_OK) {
        uart->huart->RxState = HAL_UART_STATE_READY;
        uart->recv_status = HAL_UART_Receive_IT(uart->huart, uart->buff, 1);
    }
    xSemaphoreGive(uart->lock);

    return ret;
}

//注册串口读写接口
UART_READ_WRITE_REGSTER(uart1);
//UART_READ_WRITE_REGSTER(uart2);
//UART_READ_WRITE_REGSTER(uart3);
//UART_READ_WRITE_REGSTER(uart4);

//串口初始化
void init_uart()
{
    UART_INIT_REGISTER(uart1, huart1, UART1_LOOP_BUFFER_SIZE);
    //UART_INIT_REGISTER(uart2, huart2, UART2_LOOP_BUFFER_SIZE);
    //UART_INIT_REGISTER(uart3, huart3, UART3_LOOP_BUFFER_SIZE);
    //UART_INIT_REGISTER(uart4, huart4, UART4_LOOP_BUFFER_SIZE);
}
