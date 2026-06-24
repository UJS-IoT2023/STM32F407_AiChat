/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include "usb_host.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "tui.h"
#include "usart.h"
#include "lcd.h"
#include "usb_host.h"
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

/* USER CODE BEGIN PV */
#define RX_BUFFER_SIZE 4096
uint8_t rx_data;
char esp8266_rx_buffer[RX_BUFFER_SIZE];
uint16_t rx_index = 0;

#define TCP_RX_BUFFER_SIZE 16384
volatile uint8_t tcp_receiving = 0;
volatile uint8_t tcp_done = 0;
char tcp_rx_buffer[TCP_RX_BUFFER_SIZE];
volatile uint16_t tcp_rx_len = 0;
volatile int http_content_length = -1;
volatile int http_body_received = 0;
volatile uint8_t http_header_complete = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

void MX_FREERTOS_Init(void);

/* USER CODE BEGIN PFP */
uint8_t ESP8266_SendCmd(char *cmd, char *expected_resp, uint32_t timeout);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Redirect prinft function
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart1, (uint8_t *) ptr, len, HAL_MAX_DELAY);
    return len;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    // Clear USART buffer
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_FSMC_Init();
    MX_USART1_UART_Init();
    MX_USART3_UART_Init();
    /* USER CODE BEGIN 2 */

    // Banner
    printf("\n\r===================================\n\r");
    printf("+        Program started          +\n\r");
    printf("===================================\n\r");

    // Initialize LCD

    g_point_color = WHITE;
    g_back_color = BLACK;

    tui_init();

    tui_printf_color(GREEN, "[OK] LCD initialized\n");

    // Initialize ESP8266
    HAL_UART_Receive_IT(&huart3, &rx_data, 1);

    ESP8266_SendCmd("AT\r\n", "OK", 1000);
    ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 1000);
    tui_printf_color(YELLOW, "Connecting WiFi " WIFI_SSID "\n");
    if (ESP8266_SendCmd("AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWD "\"\r\n", "WIFI GOT IP", 15000)) {
        tui_printf_color(GREEN, "[OK] WiFi Connected!\n");

        ESP8266_SendCmd("AT+CIFSR\r\n", "OK", 2000);
    } else {
        tui_printf_color(RED,   "[FAIL] WiFi Failed!\n");

        // Exception
    }
    /* USER CODE END 2 */

    /* Call init function for freertos objects (in cmsis_os2.c) */
    MX_FREERTOS_Init();

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
    */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        char c = (char) rx_data;

        if (tcp_receiving) {
            if (tcp_rx_len < TCP_RX_BUFFER_SIZE - 1) {
                tcp_rx_buffer[tcp_rx_len++] = c;
                tcp_rx_buffer[tcp_rx_len] = '\0';
            }

            // 1. 只有看到 \r\n\r\n 之后才认为 header 完整
            if (!http_header_complete) {
                char *h_end = strstr(tcp_rx_buffer, "\r\n\r\n");
                if (h_end) {
                    http_header_complete = 1;

                    // header 完整后才允许解析 Content-Length
                    char *cl = strstr(tcp_rx_buffer, "Content-Length:");
                    if (cl) {
                        cl += strlen("Content-Length:");
                        while (*cl == ' ') cl++;
                        http_content_length = atoi(cl);
                    }
                }
            }

            // 2. 只有 header 完整后才判断 body 是否收完
            if (http_header_complete && http_content_length >= 0) {
                char *body = strstr(tcp_rx_buffer, "\r\n\r\n");
                if (body) {
                    body += 4;
                    int current_body_len = (int) tcp_rx_len - (int) (body - tcp_rx_buffer);
                    if (current_body_len >= http_content_length) {
                        tcp_done = 1;
                        tcp_receiving = 0;
                    }
                }
            }

            // 3. 兜底：CLOSED 仍然作为备用结束条件
            if (strstr(tcp_rx_buffer, "CLOSED")) {
                tcp_done = 1;
                tcp_receiving = 0;
            }
        } else {
            esp8266_rx_buffer[rx_index++] = c;
            if (rx_index >= RX_BUFFER_SIZE - 1) {
                rx_index = 0;
            }
        }

        HAL_UART_Receive_IT(&huart3, &rx_data, 1);
    }
}

uint8_t ESP8266_SendCmd(char *cmd, char *expected_resp, uint32_t timeout) {
    // 1. 清空接收缓冲区
    memset(esp8266_rx_buffer, 0, RX_BUFFER_SIZE);
    rx_index = 0;

    // 2. 打印我们正在发送的指令到 USART1 进行调试
    printf(">> Send: %s", cmd);

    // 3. 通过 USART3 发送指令给 ESP8266
    HAL_UART_Transmit(&huart3, (uint8_t *) cmd, strlen(cmd), HAL_MAX_DELAY);

    // 4. 等待响应 (结合 FreeRTOS 的 osDelay 让出 CPU)
    uint32_t start_time = HAL_GetTick();
    while ((HAL_GetTick() - start_time) < timeout) {
        if (strstr(esp8266_rx_buffer, expected_resp) != NULL) {
            printf("<< Response:\r\n%s\r\n", esp8266_rx_buffer);
            return 1;
        }
        if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
            HAL_Delay(10);
        } else {
            osDelay(10);
        }
    }

    // 打印超时时的缓冲区状态，方便排错
    printf("<< Timeout!: \r\n%s\r\n", esp8266_rx_buffer);
    return 0; // 失败
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    /* USER CODE BEGIN Callback 0 */

    /* USER CODE END Callback 0 */
    if (htim->Instance == TIM1) {
        HAL_IncTick();
    }
    /* USER CODE BEGIN Callback 1 */

    /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
