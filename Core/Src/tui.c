#include "tui.h"
#include "lcd.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"

static uint16_t cursor_x = 0;
static uint16_t cursor_y = 0;
static uint8_t  font_size = 16;

static osMutexId tui_mutex = NULL;
osMutexDef(TuiMutex);

static inline int scheduler_running(void)
{
    return (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
}

static void tui_lock(void)
{
    if (tui_mutex != NULL && scheduler_running())
    {
        osMutexWait(tui_mutex, osWaitForever);
    }
}

static void tui_unlock(void)
{
    if (tui_mutex != NULL && scheduler_running())
    {
        osMutexRelease(tui_mutex);
    }
}

void tui_init(void)
{
    cursor_x = 0;
    cursor_y = 0;

    lcd_init();
    lcd_clear(BLACK);

    // if (tui_mutex == NULL)
    // {
    //     tui_mutex = osMutexCreate(osMutex(TuiMutex));
    // }
}

void tui_set_font(uint8_t size)
{
    if (size == 12 || size == 16 || size == 24)
    {
        font_size = size;
    }
}

void tui_clear(void)
{
    tui_lock();
    lcd_clear(g_back_color);
    cursor_x = 0;
    cursor_y = 0;
    tui_unlock();
}

static void tui_putc(char ch)
{
    /* 正点原子 ASCII 字库:字宽 = 字高 / 2 (12->6, 16->8, 24->12). */
    const uint8_t c_width  = (uint8_t)(font_size / 2);
    const uint8_t c_height = font_size;

    if (ch == '\n')
    {
        cursor_x = 0;
        cursor_y = (uint16_t)(cursor_y + c_height);
    }
    else if (ch == '\r')
    {
        cursor_x = 0;
    }
    else if (ch >= 32 && ch <= 126)
    {
        /* 横向撞墙 -> 强制换行. */
        if (cursor_x + c_width > lcddev.width)
        {
            cursor_x = 0;
            cursor_y = (uint16_t)(cursor_y + c_height);
        }

        /* 纵向撞墙 -> 清屏并回到原点. */
        if (cursor_y + c_height > lcddev.height)
        {
            lcd_clear(g_back_color);
            cursor_x = 0;
            cursor_y = 0;
        }

        /* mode=0:空白点用背景色覆盖,自然擦除该位置的旧字符. */
        lcd_show_char(cursor_x, cursor_y, ch, font_size, 0, g_point_color);

        cursor_x = (uint16_t)(cursor_x + c_width);
    }
}

static int tui_vprintf(uint32_t color, const char *format, va_list args)
{
    char     buffer[256];
    int      len;
    uint32_t saved_color = g_point_color;

    len = vsnprintf(buffer, sizeof(buffer), format, args);

    if (len <= 0)
    {
        return len;
    }

    /* vsnprintf 返回的是"原本想写入的长度",可能 > sizeof(buffer).
     * 这种情况按实际写入 sizeof(buffer)-1 个字符处理即可. */
    if (len >= (int)sizeof(buffer))
    {
        len = (int)sizeof(buffer) - 1;
    }

    g_point_color = color;

    tui_lock();
    for (int i = 0; i < len; i++)
    {
        tui_putc(buffer[i]);
    }
    tui_unlock();

    g_point_color = saved_color;

    return len;
}

int tui_printf_color(uint32_t color, const char *format, ...)
{
    va_list args;
    int     ret;

    va_start(args, format);
    ret = tui_vprintf(color, format, args);
    va_end(args);

    return ret;
}

int tui_printf(const char *format, ...)
{
    va_list args;
    int     ret;

    va_start(args, format);
    ret = tui_vprintf(WHITE, format, args);
    va_end(args);

    return ret;
}