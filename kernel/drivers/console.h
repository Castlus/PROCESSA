#ifndef DRIVERS_CONSOLE_H
#define DRIVERS_CONSOLE_H

#include "types.h"

/* Tela de texto VGA 80x25 dividida em areas:
 *   linha 0       barra de status
 *   linhas 1-8    tabela de processos (PCBs)
 *   linha 9       separador
 *   linhas 10-23  log do kernel e saida dos processos (rola)
 *   linha 24      prompt de comandos */
#define VGA_COLS   80
#define VGA_ROWS   25
#define LOG_TOP    10
#define LOG_BOTTOM 23
#define PROMPT_ROW 24

enum vga_color {
    VGA_BLACK, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA,
    VGA_BROWN, VGA_LGRAY, VGA_DGRAY, VGA_LBLUE, VGA_LGREEN, VGA_LCYAN,
    VGA_LRED, VGA_LMAGENTA, VGA_YELLOW, VGA_WHITE,
};

#define VGA_COLOR(fg, bg) ((uint8_t)(((bg) << 4) | (fg)))

#define COLOR_KERNEL  VGA_COLOR(VGA_LGRAY, VGA_BLACK)
#define COLOR_TITLE   VGA_COLOR(VGA_WHITE, VGA_BLACK)
#define COLOR_ERROR   VGA_COLOR(VGA_LRED, VGA_BLACK)
#define COLOR_STATUS  VGA_COLOR(VGA_WHITE, VGA_BLUE)
#define COLOR_HEADER  VGA_COLOR(VGA_BLACK, VGA_LGRAY)
#define COLOR_RUNNING VGA_COLOR(VGA_WHITE, VGA_GREEN)
#define COLOR_SEP     VGA_COLOR(VGA_DGRAY, VGA_BLACK)
#define COLOR_PROMPT  VGA_COLOR(VGA_YELLOW, VGA_BLACK)
#define COLOR_INPUT   VGA_COLOR(VGA_WHITE, VGA_BLACK)
#define COLOR_TRACE   VGA_COLOR(VGA_WHITE, VGA_BLUE)

void console_init(void);
void console_write(const char *s, size_t n, uint8_t color);   /* area de log */
void console_write_serial(const char *s, size_t n);           /* so na serial */
void console_clear_log(void);

void vga_put(int row, int col, uint8_t ch, uint8_t color);
void vga_fill_row(int row, uint8_t ch, uint8_t color);
void vga_write_at(int row, int col, const char *s, uint8_t color, int width);
void vga_set_cursor(int row, int col);

#endif
