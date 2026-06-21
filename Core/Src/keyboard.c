/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usb_keyboard.c
  * @brief          : USB HID keyboard -> UART1 echo handler
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 UJS Embedded.
  * Licensed under terms in the project LICENSE file.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#include "usbh_hid.h"
#include "usbh_hid_keybd.h"
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart1;

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
    if (USBH_HID_GetDeviceType(phost) != HID_KEYBOARD) return;

    HID_KEYBD_Info_TypeDef *info = USBH_HID_GetKeybdInfo(phost);
    if (info == NULL) return;

    char c = (char)USBH_HID_GetASCIICode(info);
    if (c == 0) return;

    HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}