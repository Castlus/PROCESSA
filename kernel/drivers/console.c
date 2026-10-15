/* Driver de video em modo texto VGA. A placa de video mostra na tela o
 * conteudo da memoria em 0xB8000: cada celula tem 2 bytes (caractere +
 * cor). Escrever ali = desenhar na tela. */
#include "drivers/console.h"
#include "drivers/serial.h"
#include "arch/io.h"

#define VGA_MEM ((volatile uint16_t *)0xB8000)

static int log_row = LOG_TOP;
static int log_col;
static uint8_t utf8_lead;      /* 1o byte de um caractere UTF-8 pendente */

static inline uint16_t cell(uint8_t ch, uint8_t color)
{
    return (uint16_t)ch | ((uint16_t)color << 8);
}

void vga_put(int row, int col, uint8_t ch, uint8_t color)
{
    if (row < 0 || row >= VGA_ROWS || col < 0 || col >= VGA_COLS)
        return;
    VGA_MEM[row * VGA_COLS + col] = cell(ch, color);
}

void vga_fill_row(int row, uint8_t ch, uint8_t color)
{
    for (int c = 0; c < VGA_COLS; c++)
        vga_put(row, c, ch, color);
}

/* Escreve texto ASCII; se width > 0, completa com espacos ate width. */
void vga_write_at(int row, int col, const char *s, uint8_t color, int width)
{
    int n = 0;
    for (; s[n] && col + n < VGA_COLS && (width <= 0 || n < width); n++)
        vga_put(row, col + n, (uint8_t)s[n], color);
    for (; width > 0 && n < width && col + n < VGA_COLS; n++)
        vga_put(row, col + n, ' ', color);
}

void vga_set_cursor(int row, int col)
{
    uint16_t pos = (uint16_t)(row * VGA_COLS + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, (pos >> 8) & 0xFF);
}

/* O codigo-fonte esta em UTF-8, mas a VGA usa a tabela "CP437".
 * Converte as letras acentuadas do portugues que existem nela. */
static uint8_t latin1_to_cp437(uint8_t c)
{
    switch (c) {
    case 0xC0: case 0xC1: case 0xC2: case 0xC3: return 'A';
    case 0xC7: return 0x80;     /* Ç */
    case 0xC9: return 0x90;     /* É */
    case 0xCA: return 'E';
    case 0xCD: return 'I';
    case 0xD3: case 0xD4: case 0xD5: return 'O';
    case 0xDA: return 'U';
    case 0xE0: return 0x85;     /* à */
    case 0xE1: return 0xA0;     /* á */
    case 0xE2: return 0x83;     /* â */
    case 0xE3: return 'a';      /* ã nao existe na CP437 */
    case 0xE7: return 0x87;     /* ç */
    case 0xE9: return 0x82;     /* é */
    case 0xEA: return 0x88;     /* ê */
    case 0xED: return 0xA1;     /* í */
    case 0xF3: return 0xA2;     /* ó */
    case 0xF4: return 0x93;     /* ô */
    case 0xF5: return 'o';      /* õ nao existe na CP437 */
    case 0xFA: return 0xA3;     /* ú */
    case 0xFC: return 0x81;     /* ü */
    case 0xAA: return 0xA6;     /* ª */
    case 0xBA: return 0xA7;     /* º */
    case 0xB0: return 0xF8;     /* ° */
    default:   return '?';
    }
}

static void log_scroll(void)
{
    for (int r = LOG_TOP; r < LOG_BOTTOM; r++)
        for (int c = 0; c < VGA_COLS; c++)
            VGA_MEM[r * VGA_COLS + c] = VGA_MEM[(r + 1) * VGA_COLS + c];
    vga_fill_row(LOG_BOTTOM, ' ', COLOR_KERNEL);
}

static void log_newline(void)
{
    log_col = 0;
    if (log_row == LOG_BOTTOM)
        log_scroll();
    else
        log_row++;
}

static void log_putc(uint8_t ch, uint8_t color)
{
    if (ch == '\n') {
        log_newline();
        return;
    }
    if (ch == '\t') {
        do
            log_putc(' ', color);
        while (log_col % 4);
        return;
    }
    if (log_col >= VGA_COLS)
        log_newline();
    vga_put(log_row, log_col++, ch, color);
}

void console_write(const char *s, size_t n, uint8_t color)
{
    for (size_t i = 0; i < n; i++) {
        uint8_t c = (uint8_t)s[i];
        serial_putc((char)c);

        if (utf8_lead) {
            uint8_t lead = utf8_lead;
            utf8_lead = 0;
            if ((c & 0xC0) == 0x80) {
                log_putc(latin1_to_cp437((uint8_t)(((lead & 0x03) << 6) | (c & 0x3F))), color);
                continue;
            }
        }
        if (c == 0xC2 || c == 0xC3) {
            utf8_lead = c;
            continue;
        }
        if (c >= 0x80) {
            if ((c & 0xC0) != 0x80)     /* outros caracteres multibyte */
                log_putc('?', color);
            continue;
        }
        log_putc(c, color);
    }
}

void console_write_serial(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        serial_putc(s[i]);
}

void console_clear_log(void)
{
    for (int r = LOG_TOP; r <= LOG_BOTTOM; r++)
        vga_fill_row(r, ' ', COLOR_KERNEL);
    log_row = LOG_TOP;
    log_col = 0;
}

void console_init(void)
{
    for (int r = 0; r < VGA_ROWS; r++)
        vga_fill_row(r, ' ', COLOR_KERNEL);
    log_row = LOG_TOP;
    log_col = 0;
}
