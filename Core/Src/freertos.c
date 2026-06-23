/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "lcd.h"
#include "tui.h"
#include "usbh_def.h"
#include "usbh_hid.h"
#include "usbh_hid_keybd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern uint8_t ESP8266_SendCmd(char *cmd, char *expected_resp, uint32_t timeout);

extern char esp8266_rx_buffer[];

extern volatile uint8_t tcp_receiving;
extern volatile uint8_t tcp_done;
extern char tcp_rx_buffer[];
extern volatile uint16_t tcp_rx_len;
extern volatile int http_content_length;
extern volatile int http_body_received;
extern volatile uint8_t http_header_complete;

extern UART_HandleTypeDef huart3;

/* 键盘事件队列:USBH_HID_EventCallback 把按键 ASCII 推入,UsbKbTask 阻塞读取处理。
 * 拆成 ISR/USB 任务上下文中产事件 + 普通任务中消费事件,避免在 USB Host 任务的
 * 512B 小栈上调用 tui_printf(其内部有 256B 缓冲区 + vsnprintf)导致栈溢出。*/
static QueueHandle_t keybd_queue = NULL;

char user_input_buffer[256];
uint16_t user_input_index = 0;
/* USER CODE END Variables */
osThreadId LlmTaskHandle;
osThreadId UsbKbTaskHandle;
osSemaphoreId LlmGenSemHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartLlmTask(void const *argument);

void StartUsbKbTask(void const *argument);

extern void MX_USB_HOST_Init(void);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize);

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
    *ppxIdleTaskStackBuffer = &xIdleStack[0];
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
    /* place for user code */
}

/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* Create the semaphores(s) */
    /* definition and creation of LlmGenSem */
    osSemaphoreDef(LlmGenSem);
    LlmGenSemHandle = osSemaphoreCreate(osSemaphore(LlmGenSem), 1);

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* 键盘事件队列:容纳 32 个 ASCII 字节,事件回调入队,UsbKbTask 出队 */
    keybd_queue = xQueueCreate(32, sizeof(uint8_t));
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* definition and creation of LlmTask */
    osThreadDef(LlmTask, StartLlmTask, osPriorityNormal, 0, 2048);
    LlmTaskHandle = osThreadCreate(osThread(LlmTask), NULL);

    /* definition and creation of UsbKbTask */
    osThreadDef(UsbKbTask, StartUsbKbTask, osPriorityAboveNormal, 0, 512);
    UsbKbTaskHandle = osThreadCreate(osThread(UsbKbTask), NULL);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    /* USER CODE END RTOS_THREADS */
}

/* USER CODE BEGIN Header_StartLlmTask */
/**
  * @brief  Function implementing the LlmTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartLlmTask */
void StartLlmTask(void const *argument) {
    /* init code for USB_HOST */
    MX_USB_HOST_Init();
    /* USER CODE BEGIN StartLlmTask */

    osDelay(3000);
    printf("\r\n>> Initiating System Self-Test with AI...\r\n");
    tui_printf_color(YELLOW, "Initiating LLM API...\n");

    strcpy(user_input_buffer, "hello");
    user_input_index = strlen("hello");

    /* Infinite loop */
    for (;;) {
        if (osSemaphoreWait(LlmGenSemHandle, osWaitForever) == osOK) {
            printf("\r\n=== Sending User Input to LLM API ===\r\n");
            printf("Input: %s\r\n", user_input_buffer);
            tui_printf_color(YELLOW, "Connecting to API...\n");

            if (ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"" LLM_HOST "\",8080\r\n", "CONNECT", 3000)) {
                printf("<< TCP connect success\r\n");

                // url encode
                char encoded_input[512] = {0};
                char *src = user_input_buffer;
                char *dst = encoded_input;
                while (*src) {
                    if (*src == ' ') {
                        *dst++ = '%';
                        *dst++ = '2';
                        *dst++ = '0';
                    } else {
                        *dst++ = *src;
                    }
                    src++;
                }

                // 构造动态 HTTP GET 报文
                char http_req[1024];
                snprintf(http_req, sizeof(http_req),
                         "GET /api/llm/generate?user_input=%s HTTP/1.1\r\n"
                         "Host: " LLM_HOST ":8080\r\n"
                         "Connection: close\r\n\r\n",
                         encoded_input);

                char send_cmd[32];
                sprintf(send_cmd, "AT+CIPSEND=%d\r\n", strlen(http_req));

                // 1. 先发 CIPSEND 并等 > 提示符（仍在 AT 模式）
                if (ESP8266_SendCmd(send_cmd, ">", 2000)) {
                    printf(">> Sending HTTP request...\r\n");

                    // 2. 切换到 TCP 接收模式
                    tcp_rx_len = 0;
                    tcp_rx_buffer[0] = '\0';
                    tcp_done = 0;
                    http_content_length = -1;
                    http_body_received = 0;
                    http_header_complete = 0;
                    tcp_receiving = 1;

                    // 3. 直接用 HAL_UART_Transmit 发 HTTP（不走 SendCmd）
                    HAL_UART_Transmit(&huart3, (uint8_t *) http_req, strlen(http_req), HAL_MAX_DELAY);

                    // 4. 等待 TCP 真正结束（CLOSED）
                    uint32_t start = HAL_GetTick();
                    while (!tcp_done) {
                        if (HAL_GetTick() - start > 15000) break;
                        osDelay(10);
                    }

                    // 5. 此时 tcp_rx_buffer 已经是完整的 HTTP 响应
                    if (tcp_done) {
                        printf("<< Response Received! Parsing...\r\n");

                        // 【改进 1】先在缓冲区中寻找真正的 HTTP 协议头起始位置，避开 "SEND OK"
                        char *http_start = strstr(tcp_rx_buffer, "HTTP/1.");
                        if (http_start != NULL) {
                            // 【改进 2】从 HTTP 协议头之后，寻找真正的 Header 和 Body 的分界线
                            char *body = strstr(http_start, "\r\n\r\n");
                            if (body != NULL) {
                                body += 4; // 跳过 \r\n\r\n

                                // 清理：原位抹掉 body 中残留的 +IPD,N: 前缀和 CLOSED
                                char *out = body;
                                char *p = body;
                                while (*p) {
                                    if (strncmp(p, "+IPD,", 5) == 0) {
                                        while (*p && *p != ':') p++;
                                        if (*p == ':') p++;
                                        continue;
                                    }
                                    if (strncmp(p, "CLOSED", 6) == 0) break;
                                    *out++ = *p++;
                                }
                                *out = '\0';

                                // 此时串口打印出来的就是纯净的、去掉了 HTTP 头的 Body 内容
                                printf("--- LLM Cleaned Body ---\r\n%s\r\n------------------\r\n", body);

                                // 清屏并准备打印到 LCD
                                // lcd_clear(BLACK);
                                tui_printf_color(GREEN, "AI Reply:\n");

                                // 【改进 3】LCD 友好型安全字符流打印，防止连续大量空格导致屏幕跑飞
                                char *read_ptr = body;
                                int continuous_space = 0;
                                while (*read_ptr) {
                                    // 压缩连续的多余空格，防止坐标溢出
                                    if (*read_ptr == ' ' || *read_ptr == '\t') {
                                        continuous_space++;
                                        if (continuous_space > 1) {
                                            read_ptr++;
                                            continue;
                                        }
                                    } else {
                                        continuous_space = 0;
                                    }

                                    // 过滤掉 \r 符号，统一只用 \n 换行
                                    if (*read_ptr == '\r') {
                                        read_ptr++;
                                        continue;
                                    }

                                    // 逐个字符安全输出到 LCD
                                    char t_buf[2] = {*read_ptr, '\0'};
                                    tui_printf("%s", t_buf);
                                    read_ptr++;
                                }

                                tui_printf("[Done]\n");
                            } else {
                                printf("<< Error: body delimiter not found after HTTP header\r\n");
                            }
                        } else {
                            printf("<< Error: 'HTTP/1.' protocol header not found\r\n");
                        }
                    } else {
                        printf("<< TCP receive timeout\r\n");
                        tui_printf_color(RED, "API Timeout!\n");
                    }
                }
            } else {
                printf("<< TCP connect failed\r\n");
                tui_printf_color(RED, "API Connect Failed!\n");
            }

            // =====================================
            // 4. 请求结束，重置缓冲区，准备迎接真实的物理键盘输入
            user_input_index = 0;
            memset(user_input_buffer, 0, sizeof(user_input_buffer));

            printf("\r\n==========================================\r\n");
            printf(">> Talk to AI (type on USB Keyboard and press Enter):\r\n");
            printf("==========================================\r\n");
        }

        osDelay(10000);
    }
    /* USER CODE END StartLlmTask */
}

/* USER CODE BEGIN Header_StartUsbKbTask */
/**
* @brief Function implementing the UsbKbTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUsbKbTask */
void StartUsbKbTask(void const *argument) {
    /* USER CODE BEGIN StartUsbKbTask */

    uint8_t ascii = 0;

    printf(">> USB Keyboard Task Started, waiting for device...\r\n");

    /* Infinite loop:阻塞等待 USBH_HID_EventCallback 推入的按键事件,
     * 不再轮询 USBH_HID_GetKeybdInfo(那会在两个上下文里重复消费 FIFO 并跑出错的句柄)。*/
    for (;;) {
        if (keybd_queue == NULL) {
            osDelay(50);
            continue;
        }

        /* 永久阻塞,直到回调推入一个 ASCII。CPU 让给 LlmTask / USB Host 任务。*/
        if (xQueueReceive(keybd_queue, &ascii, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // A. 处理退格键 (Backspace / Delete)
        if (ascii == '\b' || ascii == 0x7F) {
            if (user_input_index > 0) {
                user_input_index--;
                user_input_buffer[user_input_index] = '\0';

                // 实时 LCD 擦除回显：由于 tui.c 是流式覆盖打印，
                // 最佳的做法是清屏，重新打印固定标头和用户当前已输入的整行缓冲区
                tui_clear();
                tui_printf_color(WHITE, "LLM Chat Box\n");
                tui_printf("Input: %s", user_input_buffer);

                // 串口同步打印退格效果
                printf("\b \b");
            }
        }
        // B. 处理回车键 (Enter)，触发大模型生成任务
        else if (ascii == '\r' || ascii == '\n') {
            if (user_input_index > 0) { // 确保用户输入了内容

                // 在 LCD 和串口输出提示
                tui_printf("\n[Sending...]\n");
                printf("\r\n>> Prompt Committed. Releasing Semaphore...\r\n");

                // 释放信号量，唤醒 StartLlmTask
                osSemaphoreRelease(LlmGenSemHandle);
            }
        }
        // C. 处理普通可见字符（空格到波浪号）
        else if (ascii >= 32 && ascii <= 126) {
            if (user_input_index < sizeof(user_input_buffer) - 1) {

                // 存入 user_input_buffer
                user_input_buffer[user_input_index++] = (char)ascii;
                user_input_buffer[user_input_index] = '\0';

                // 1. 实时显示在 LCD 屏幕上
                // 你的 tui_putc 已经自带了撞墙换行和纵向换行清屏机制，非常安全
                tui_printf("%c", ascii);

                // 2. 串口回显，方便电脑调试
                printf("%c", ascii);
            }
        }
    }
    /* USER CODE END StartUsbKbTask */
}

/* USB Host 库的弱回调:每次 HID 收到新数据(中断端点回报)时,USB Host 任务
 * 会用真正的 phost(= &hUsbHostFS)调到这里。我们在此读出 ASCII,只做最小工作
 * (压入队列),把耗时的 LCD/串口 IO 留给 UsbKbTask。这样既能拿到正确的句柄,
 * 又不在 USB Host 任务的 512B 小栈上做大动作。
 *
 * 这是 keyboard.c 原本占用的回调位置 —— 它已被删除,所以这里没有重复定义冲突。*/
void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
    if (USBH_HID_GetDeviceType(phost) != HID_KEYBOARD) {
        return;
    }

    HID_KEYBD_Info_TypeDef *info = USBH_HID_GetKeybdInfo(phost);
    if (info == NULL) {
        return;
    }

    uint8_t ascii = USBH_HID_GetASCIICode(info);
    if (ascii == 0) {
        return;
    }

    if (keybd_queue != NULL) {
        /* 0 超时:队列满就丢弃,绝不阻塞 USB Host 任务 */
        (void)xQueueSend(keybd_queue, &ascii, 0);
    }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
