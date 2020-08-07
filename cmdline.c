
#include "uart.h"
#include "stdlib.h"
#include "FreeRTOS.h"
#include "motor.h"

#define TIP "\r\n#"

struct commond_ctrl{
    char *cmd_string;
    void (*cmd)(void *arg, void *args[]);
    char *description;
};

static void cmd_help(void *arg, void *args[]);
static void moto_ctrl(void *arg, void *args[]);
static void led_ctrl(void *arg, void *args[]);
static void cmd_free(void *arg, void *args[]);
static void reboot(void *arg, void *args[]);

struct commond_ctrl g_cmd_list[] = {
    {"help",  cmd_help,  "list all commond!"},
    {"moto",  moto_ctrl, "moto control! example:moto X in1 in2"},
    {"led" ,  led_ctrl,  "example:led 1"},
    {"free",  cmd_free,  "list heap, min ever heap, cdmline_thread stack"},
    {"reboot",reboot,    "reboot the bord"},
};

static void cmd_help(void *arg, void *args[])
{
    printf("\r\nCMD             DESCRIPTION\r\n");
    for(uint8_t i = 0 ;i < sizeof(g_cmd_list)/sizeof(struct commond_ctrl); i++) {
        if(g_cmd_list[i].cmd_string != NULL && g_cmd_list[i].cmd != NULL){
            //第二列打印对齐
            printf("%-*s",20-strlen(g_cmd_list[i].cmd_string), g_cmd_list[i].cmd_string);
            printf("\"%s\"\r\n", g_cmd_list[i].description);
        }
    }
}

static void moto_ctrl(void *arg, void *args[])
{
    if(*(uint8_t *)arg != 3) {
        printf("\r\nparam num is false,need 3, but %d\r\n",*(uint8_t *)arg);
        return;
    }

    MOTOR_HANDLE moto = atoi((const char *)args[0]);
    uint16_t in1 = atoi((const char *)args[1]);
    uint16_t in2 = atoi((const char *)args[2]);
    //motorX_control(moto,in1,in2);
}
static void led_ctrl(void *arg, void *args[])
{
    uint8_t onoff = atoi((const char *)args[0]);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, onoff);
}

static void cmd_free(void *arg, void *args[])
{
    printf("\r\nheap:%d,minheap:%d,cmdstack:%d\r\n", xPortGetFreeHeapSize(),
           xPortGetMinimumEverFreeHeapSize(), (int)uxTaskGetStackHighWaterMark(NULL));
}

static void reboot(void *arg, void *args[])
{
    __set_FAULTMASK(1);//关闭总中断
    NVIC_SystemReset();//请求单片机重启,不允许被打断，所以关总中断
}

static uint8_t cmd_str_analyse(uint8_t *data, uint8_t **cmd, uint8_t **param)
{
    uint8_t *str = NULL;
    uint8_t i = 0;

    *cmd = data;
    str = data;

    for( ; ; ) {
        if (*str != '\0') {
            if(*str == ' ') {
                *str = '\0';
                str++;
                if(*str != ' ' && *str != '\0') {
                    param[i++] = str;
                }
            } else {
                str++;
            }
        } else {
            break;
        }
    }

    return i;
}

static void cmdline_dispatcher(uint8_t *data)
{
    uint8_t *param[20] = {NULL};
    uint8_t *cmd = NULL;
    uint8_t param_num = 0;

    /* data格式：命令[空格]param0[空格]param[1][空格]param[2].....*/
    /* 例：moto 1 99 10 */
    param_num = cmd_str_analyse(data, &cmd, param);

    for(uint8_t i = 0 ;i < sizeof(g_cmd_list)/sizeof(struct commond_ctrl); i++) {
        if(g_cmd_list[i].cmd_string != NULL && g_cmd_list[i].cmd != NULL){
            if(strcmp((const char *)cmd, g_cmd_list[i].cmd_string) == 0) {
                g_cmd_list[i].cmd(&param_num, (void **)param);
                return;
            }
        }
    }

    printf("\r\nnot found cmd \"%s\",please try again or input \"help\"\r\n", cmd);

}

static int32_t cmdline_rx(void *data, int32_t len)
{
    static uint8_t buf_cnt = 0;
    static uint8_t cmd_buf[UART1_LOOP_BUFFER_SIZE];
    int32_t i = 0;
    uint8_t *tmp = (uint8_t *)data;

    for (; i < len; i++) {
        if (tmp[i] != '\r') {
            uart1.write(&tmp[i], 1);
            /* 退格处理 */
            if (0x7f == tmp[i]) {
                cmd_buf[--buf_cnt] = 0;
                continue;
            }

            if (tmp[i] != '\n') {
                cmd_buf[buf_cnt] = tmp[i];
                buf_cnt++;
                if (buf_cnt >= 128) {
                    buf_cnt = 0;
                    memset(cmd_buf, 0, sizeof(cmd_buf));
                }
            }
        } else {
            cmdline_dispatcher(cmd_buf);
            memset(cmd_buf, 0, buf_cnt);
            buf_cnt = 0;
            uart1.write((uint8_t *)TIP, strlen(TIP));
        }
    }

    return 0;
}

static THREAD_VOID cmdline_process(void *args)
{
    uint8_t buffer[UART1_LOOP_BUFFER_SIZE] = {0};
    uint8_t len = 0;

    while(1) {
        len = uart1.read(buffer);
        if(len > 0) {
            //printf("recv len(%d) and data is:%s\r\n", len, buffer);
            cmdline_rx(buffer, len);
        }
        sleep_ms(10);
    }
}

int cmdline_init()
{
    thread_t procTaskHandle = NULL;
    init_uart();

    if (THREAD_OK != create_thread(cmdline_process,
                                   "cmdline_thread",
                                   1024+1024,
                                   NULL,
                                   OS_PRIO_MEDIUM+1,
                                   &procTaskHandle,
                                   NULL)) {
        LogErrorPrefix("%s failed!\r\n", __FUNCTION__);
        return 0;
    }

    return 0;
}
