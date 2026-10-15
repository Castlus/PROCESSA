/* Driver de video. Dois modos, com a mesma grade de 80x25 caracteres:
 *  - boot por BIOS: modo texto VGA. A placa de video mostra na tela o
 *    conteudo da memoria em 0xB8000: cada celula tem 2 bytes (caractere +
 *    cor). Escrever ali = desenhar na tela.
 *  - boot por UEFI: nao existe modo texto, so uma tela grafica (GOP). O
 *    kernel guarda a grade numa copia propria e desenha cada caractere
 *    pixel a pixel com a fonte 8x16 da VGA (drivers/font8x16.c). */
#include "drivers/console.h"
#include "drivers/serial.h"
#include "arch/io.h"
#include "boot.h"
#include "mm/vmm.h"

#define VGA_TEXT ((volatile uint16_t *)0xB8000)

extern const uint8_t font8x16[256 * 16];

static int log_row = LOG_TOP;
static int log_col;
static uint8_t utf8_lead;      /* 1o byte de um caractere UTF-8 pendente */

/* ---- tela grafica (boot UEFI) ---- */
static bool fb_on;
static volatile uint32_t *fb;
static uint32_t fb_stride, fb_scale, fb_x0, fb_y0;
static uint32_t fb_palette[16];
static uint16_t grid[VGA_ROWS * VGA_COLS];     /* o que deve estar na tela */
static uint16_t shown[VGA_ROWS * VGA_COLS];    /* o que ja foi desenhado   */
static int cursor_row = -1, cursor_col;

/* as 16 cores da VGA em RGB */
static const uint32_t vga_rgb[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static inline uint16_t cell(uint8_t ch, uint8_t color)
{
    return (uint16_t)ch | ((uint16_t)color << 8);
}

static uint32_t pack_component(uint32_t value, uint32_t mask)
{
    int shift = 0;
    if (!mask)
        return 0;
    while (!(mask & 1)) {
        mask >>= 1;
        shift++;
    }
    return ((value * mask) / 255) << shift;
}

/* Converte RGB para o formato de pixel que a placa usa. */
static uint32_t pack_color(uint32_t rgb, const struct uefi_bootinfo *u)
{
    uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    switch (u->fb_format) {
    case 0:  return r | (g << 8) | (b << 16);      /* bytes na memoria: R G B */
    case 1:  return b | (g << 8) | (r << 16);      /* bytes na memoria: B G R */
    default: return pack_component(r, u->mask_r) | pack_component(g, u->mask_g) |
                    pack_component(b, u->mask_b);
    }
}

/* Desenha uma celula da grade: 8x16 pixels da fonte, ampliados fb_scale vezes. */
static void fb_draw(int row, int col)
{
    int i = row * VGA_COLS + col;
    uint16_t c = grid[i];
    const uint8_t *glyph = &font8x16[(c & 0xFF) * 16];
    uint32_t fg = fb_palette[(c >> 8) & 0xF], bg = fb_palette[(c >> 12) & 0xF];
    bool cursor = row == cursor_row && col == cursor_col;
    uint32_t x0 = fb_x0 + (uint32_t)col * 8 * fb_scale;
    uint32_t y0 = fb_y0 + (uint32_t)row * 16 * fb_scale;

    for (int y = 0; y < 16; y++) {
        uint8_t bits = (cursor && y >= 13 && y <= 14) ? 0xFF : glyph[y];
        for (uint32_t sy = 0; sy < fb_scale; sy++) {
            volatile uint32_t *p = fb + (uint64_t)(y0 + y * fb_scale + sy) * fb_stride + x0;
            for (int x = 0; x < 8; x++) {
                uint32_t color = (bits & (0x80 >> x)) ? fg : bg;
                for (uint32_t sx = 0; sx < fb_scale; sx++)
                    *p++ = color;
            }
        }
    }
    shown[i] = c;
}

static void put_cell(int i, uint16_t v)
{
    if (!fb_on) {
        VGA_TEXT[i] = v;
        return;
    }
    grid[i] = v;
    if (shown[i] != v)                  /* so redesenha o que mudou */
        fb_draw(i / VGA_COLS, i % VGA_COLS);
}

static uint16_t get_cell(int i)
{
    return fb_on ? grid[i] : VGA_TEXT[i];
}

void vga_put(int row, int col, uint8_t ch, uint8_t color)
{
    if (row < 0 || row >= VGA_ROWS || col < 0 || col >= VGA_COLS)
        return;
    put_cell(row * VGA_COLS + col, cell(ch, color));
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
    if (fb_on) {                        /* tela grafica: desenhamos o cursor */
        int old_row = cursor_row, old_col = cursor_col;
        cursor_row = row;
        cursor_col = col;
        if (old_row >= 0)
            fb_draw(old_row, old_col);
        fb_draw(row, col);
        return;
    }
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
            put_cell(r * VGA_COLS + c, get_cell((r + 1) * VGA_COLS + c));
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

/* Boot UEFI: prepara a tela grafica. A grade de 80x25 e ampliada o maximo
 * que couber (num notebook Full HD, 2x) e centralizada. */
static void fb_init(const struct uefi_bootinfo *u)
{
    if (!u->fb_addr || u->fb_width < VGA_COLS * 8 || u->fb_height < VGA_ROWS * 16)
        return;
    fb = vmm_map_mmio(u->fb_addr, (uint64_t)u->fb_stride * u->fb_height * 4, false);
    if (!fb)
        return;

    uint32_t sx = u->fb_width / (VGA_COLS * 8), sy = u->fb_height / (VGA_ROWS * 16);
    fb_scale = sx < sy ? sx : sy;
    fb_stride = u->fb_stride;
    fb_x0 = (u->fb_width - VGA_COLS * 8 * fb_scale) / 2;
    fb_y0 = (u->fb_height - VGA_ROWS * 16 * fb_scale) / 2;
    for (int i = 0; i < 16; i++)
        fb_palette[i] = pack_color(vga_rgb[i], u);

    for (uint32_t y = 0; y < u->fb_height; y++)       /* fundo preto */
        for (uint32_t x = 0; x < u->fb_width; x++)
            fb[(uint64_t)y * fb_stride + x] = fb_palette[0];
    for (int i = 0; i < VGA_ROWS * VGA_COLS; i++)
        shown[i] = 0xFFFF;                             /* forca o 1o desenho */
    fb_on = true;
}

void console_init(void)
{
    if (boot.kind == BOOT_UEFI)
        fb_init(boot.uefi);
    for (int r = 0; r < VGA_ROWS; r++)
        vga_fill_row(r, ' ', COLOR_KERNEL);
    log_row = LOG_TOP;
    log_col = 0;
}

bool console_graphic(void)
{
    return fb_on;
}
