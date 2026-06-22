#ifndef __TUI_H
#define __TUI_H

#include "stdint.h"

/**
 * @brief  初始化 TUI 输出流:复位虚拟光标并创建 FreeRTOS 互斥锁.
 * @note   必须在 lcd_init() 之后调用,因为内部依赖 lcddev.width / lcddev.height.
 *         调度器未启动前调用是安全的(锁会延后到 printf 时再获取).
 */
void tui_init(void);

/**
 * @brief  设置 TUI 的字符显示字号.
 * @param  size: 仅接受 12 / 16 / 24,其它值被忽略.
 */
void tui_set_font(uint8_t size);

/**
 * @brief  清屏并把光标复位到 (0,0).
 */
void tui_clear(void);

/**
 * @brief  线程安全的格式化打印接口,行为类似 stdout 的 printf:
 *           - '\n'  换行(列归零,行下移一行)
 *           - '\r'  回车(列归零,行不变)
 *           - 同行写满后自动换行到下一行
 *           - 触底后整屏清空,光标回到左上角
 *         默认使用白色画笔(WHITE).
 * @param  format: 标准 printf 格式串
 * @retval 实际写入的字符数 (vsnprintf 返回值)
 */
int tui_printf(const char *format, ...);

/**
 * @brief  与 tui_printf 行为一致,但本次打印使用指定颜色,结束后恢复 g_point_color.
 * @param  color:  本次打印的画笔颜色(与 g_point_color 同类型,例如 RED / GREEN / BLUE)
 * @param  format: 标准 printf 格式串
 * @retval 实际写入的字符数 (vsnprintf 返回值)
 */
int tui_printf_color(uint32_t color, const char *format, ...);

#endif /* __TUI_H */