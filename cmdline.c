#include <time.h>
#include "cmd_uart.h"
#include "stdlib.h"
#include "FreeRTOS.h"
#include "motor.h"

#ifdef USED_CMDLINE

#define TIP "\r\n#"

#define PARAM_VALID_NUM(n)\
do {\
    if(*(uint8_t *)arg != n) {\
        printf("\r\nparam num is false,need %d, but %d\r\n", n, *(uint8_t *)arg);\
        return;\
    }\
} while(0)

struct cmd_line_uart {
    void (*write)(uint8_t *data, uint16_t size);
    uint16_t(*read)(uint8_t *data);
};

typedef struct {
    struct cmdline_list cmdline;
    list_head_t list;
}ST_CMDLINE_NODE;

static void cmd_help(void *arg, void *args[]);
static void moto_ctrl(void *arg, void *args[]);
static void cmd_free(void *arg, void *args[]);
static void reboot(void *arg, void *args[]);
static void baseinfo(void *arg, void *args[]);
static void cmd_adc(void *arg, void *args[]);
static void cmd_temp(void *arg, void *args[]);
static void cmd_sense(void *arg, void *args[]);
static void cmd_dev(void *arg, void *args[]);
static void cmd_mlx(void *arg, void *args[]);


static list_head_t s_cmd_list;
struct cmd_line_uart g_cmd_uart;

static struct cmdline_list g_cmd_list[] = {
    {"help",  cmd_help,  "list all commond!"},
    {"moto",  moto_ctrl, "moto control! example:moto X direction speed"},
    {"free",  cmd_free,  "list heap, min ever heap, cdmline_thread stack"},
    {"reboot",reboot,    "reboot the bord"},
    {"info",  baseinfo,  "get base infomation"},
    {"adc",   cmd_adc,   "get adc value"},
    {"sense", cmd_sense, "get switch state"},
    {"temp",  cmd_temp,  "get resistor temperature"},
    {"dev",   cmd_dev,   "control device state"},
    {"mlx",   cmd_mlx,   "mlx90614 test"},
};

//将16进制字符串格式化为16进制数
static uint32_t str2hex(uint8_t *data)
{
    int num = 0;

    //判断是否为16进制字符串
    if(*data != '0' || (*(data+1) != 'x' && *(data+1) != 'X')) {
        printf("\r\nyou have input string \"%s\", please input right hex number!\r\n",data);
        return 0;
    }

    sscanf((const char*)data, "%x", &num);
    return num;
}

/******************************commond start***********************************/
static void cmd_help(void *arg, void *args[])
{
    PARAM_VALID_NUM(0);

    printf("\r\nCMD                 DESCRIPTION\r\n");
    // for (uint8_t i = 0;i < sizeof(g_cmd_list) / sizeof(struct cmdline_list); i++) {
    //     if (g_cmd_list[i].cmd_string != NULL && g_cmd_list[i].cmd != NULL) {
    //         //第二列打印对齐
    //         printf("%-*s", 20 - strlen(g_cmd_list[i].cmd_string), g_cmd_list[i].cmd_string);
    //         printf("\"%s\"\r\n", g_cmd_list[i].description);
    //     }
    // }

    ST_CMDLINE_NODE *node;
    list_head_t *pos;
    list_for_each(pos, &s_cmd_list)
    {
        node = list_entry(pos, ST_CMDLINE_NODE, list);
        if (node->cmdline.cmd_string != NULL && node->cmdline.cmd != NULL) {
            //第二列打印对齐
           //printf("%-*s", 20 - strlen(node->cmdline.cmd_string), node->cmdline.cmd_string);
            printf("%-*s", 20, node->cmdline.cmd_string);
            printf("\"%s\"\r\n", node->cmdline.description);
        }
    }
}

static void reboot(void *arg, void *args[])
{
    PARAM_VALID_NUM(0);

    __set_FAULTMASK(1);//关闭总中断
    NVIC_SystemReset();//请求单片机重启,不允许被打断，所以关总中断
}

static void cmd_free(void *arg, void *args[])
{
    PARAM_VALID_NUM(0);

    printf("\r\nheap:%d,minheap:%d,cmdstack:%d\r\n", xPortGetFreeHeapSize(),
           xPortGetMinimumEverFreeHeapSize(), (int)uxTaskGetStackHighWaterMark(NULL));
}

static void baseinfo(void *arg, void *args[])
{
    PARAM_VALID_NUM(0);

    uint32_t now = get_process_msec();
    time_t rawtime = now / 1000;
    struct tm *info = localtime(&rawtime);

    printf("\r\nbuild time:%s %s\r\n", __DATE__, __TIME__);
    printf("gcc version:%s\r\n", __VERSION__);
    printf("run time:%ums, %02d-%02d:%02d:%02d\r\n", (unsigned int)now, \
        info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
}

static void moto_ctrl(void *arg, void *args[])
{
    PARAM_VALID_NUM(3);

    MOTOR_HANDLE moto = atoi((const char *)args[0]);
    uint16_t in1 = atoi((const char *)args[1]);
    uint16_t in2 = atoi((const char *)args[2]);
    //motorX_control(moto,in1,in2);
}

static void led_ctrl(void *arg, void *args[])
{
    PARAM_VALID_NUM(1);

    uint8_t onoff = atoi((const char *)args[0]);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, onoff);
}
/***************************commond end*****************************************/

static uint8_t cmd_str_analyse(uint8_t *data, uint8_t **cmd, uint8_t **param)
{
    uint8_t *str = NULL;
    uint8_t i = 0;

    //找到第一个不为空格的字符
    str = data;
    for( ; ; ) {
        if(*str != ' ') {
            *cmd = str;
            break;
        }
        str++;
    }

    //解析命令后面的参数，保存在指针*param下
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
    uint8_t i = 0;

    /* data格式：命令[空格]param0[空格]param[1][空格]param[2].....*/
    /* 例：moto 1 99 10 */
    param_num = cmd_str_analyse(data, &cmd, param);

    // for (i = 0;i < sizeof(g_cmd_list) / sizeof(struct cmdline_list); i++) {
    //     if (g_cmd_list[i].cmd_string != NULL && g_cmd_list[i].cmd != NULL) {
    //         if (strcmp((const char *)cmd, g_cmd_list[i].cmd_string) == 0) {
    //             g_cmd_list[i].cmd(&param_num, (void **)param);
    //             return;
    //         }
    //     }
    // }

    // if (i >= sizeof(g_cmd_list) / sizeof(struct cmdline_list) && *cmd != '\0') {
    //     printf("\r\nnot found commond \"%s\", please try again or input \"help\"!\r\n", cmd);
    // }


    ST_CMDLINE_NODE *node;
    list_head_t *pos;
    list_for_each(pos, &s_cmd_list)
    {
        node = list_entry(pos, ST_CMDLINE_NODE, list);
        if (node->cmdline.cmd_string != NULL && node->cmdline.cmd != NULL) {
            if (strcmp((const char *)cmd, node->cmdline.cmd_string) == 0) {
                LogDebug("\r\n");
                node->cmdline.cmd(&param_num, (void **)param);
                return;
            }
        }
    }

    if (*cmd != '\0') {
        LogDebug("\r\nnot found commond \"%s\", please try again or input \"help\"!\r\n", cmd);
    }
}

static int32_t cmdline_rx(void *data, int32_t len)
{
    static uint8_t buf_cnt = 0;
    static uint8_t cmd_buf[UART1_LOOP_BUFFER_SIZE];
    int32_t i = 0;
    uint8_t *tmp = (uint8_t *)data;

    for (; i < len; i++) {
        if (tmp[i] != '\r') {
            g_cmd_uart.write(&tmp[i], 1);
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
            g_cmd_uart.write((uint8_t *)TIP, strlen(TIP));
        }
    }

    return 0;
}

static THREAD_VOID cmdline_process(void *args)
{
    uint8_t buffer[UART1_LOOP_BUFFER_SIZE] = { 0 };
    uint8_t len = 0;

    while (1) {
        len = g_cmd_uart.read(buffer);
        if (len > 0) {
            //printf("recv len(%d) and data is:%s\r\n", len, buffer);
            cmdline_rx(buffer, len);
        }
        sleep_ms(100);
    }
}

int cmdline_init()
{
    thread_t procTaskHandle = NULL;

    INIT_LIST_HEAD(&s_cmd_list);

    for (uint8_t i = 0; i < UBOUND(g_cmd_list); ++i) {
        reg_cmdline(g_cmd_list[i].cmd_string, g_cmd_list[i].cmd, g_cmd_list[i].description);
    }

    cmd_uart_init();

    //#ifdef USED_UART1
    //    g_cmd_uart.write = uart1.write;
    //    g_cmd_uart.read = uart1.read;
    //#endif
#ifdef USED_UART4
    g_cmd_uart.write = uart4.write;
    g_cmd_uart.read = uart4.read;
#endif

    if (THREAD_OK != create_thread(cmdline_process,
        "cmdline_thread",
        1024 + 1024 + 256,
        NULL,
        OS_PRIO_MEDIUM + 1,
        &procTaskHandle,
        NULL)) {
        LogErrorPrefix("%s failed!\r\n", __FUNCTION__);
        return 0;
    }
    LogDebugPrefix("cmdline_init finished!\r\n");
    return 0;
}

void reg_cmdline(char *cmd_string, cmdline_cb cmd, char *description)
{
    ST_CMDLINE_NODE *node = (ST_CMDLINE_NODE *)malloc(sizeof(ST_CMDLINE_NODE));
    node->cmdline.cmd_string = cmd_string;
    node->cmdline.cmd = cmd;
    node->cmdline.description = description;
    list_add_tail(&(node->list), &s_cmd_list);
}

#else
int cmdline_init()
{
    return 0;
}
#endif /*USED_CMDLINE*/
