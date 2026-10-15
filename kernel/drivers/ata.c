/* Driver de disco ATA (IDE) em modo PIO, canal primario, disco mestre.
 * "PIO" = a CPU le cada palavra de 16 bits pela porta de dados (sem DMA).
 * Usamos polling (esperar o bit de status) em vez da IRQ14. */
#include "drivers/ata.h"
#include "arch/io.h"

#define ATA_DATA     0x1F0
#define ATA_ERROR    0x1F1
#define ATA_COUNT    0x1F2
#define ATA_LBA0     0x1F3
#define ATA_LBA1     0x1F4
#define ATA_LBA2     0x1F5
#define ATA_DRIVE    0x1F6
#define ATA_STATUS   0x1F7      /* leitura */
#define ATA_COMMAND  0x1F7      /* escrita */
#define ATA_CONTROL  0x3F6

#define ST_ERR  0x01
#define ST_DRQ  0x08            /* dados prontos para transferir */
#define ST_DF   0x20
#define ST_BSY  0x80

#define CMD_READ_SECTORS 0x20
#define CMD_IDENTIFY     0xEC

static char model[41];
static uint32_t total_sectors;

static void delay400ns(void)
{
    for (int i = 0; i < 4; i++)
        inb(ATA_CONTROL);
}

static bool wait_not_busy(void)
{
    for (int i = 0; i < 10000000; i++)
        if (!(inb(ATA_STATUS) & ST_BSY))
            return true;
    return false;
}

static bool wait_data(void)
{
    for (int i = 0; i < 10000000; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & (ST_ERR | ST_DF))
            return false;
        if (!(s & ST_BSY) && (s & ST_DRQ))
            return true;
    }
    return false;
}

bool ata_init(void)
{
    uint16_t id[256];

    outb(ATA_CONTROL, 0x02);            /* nIEN: sem IRQ, usamos polling */
    outb(ATA_DRIVE, 0xA0);              /* seleciona o disco mestre */
    delay400ns();
    outb(ATA_COUNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_COMMAND, CMD_IDENTIFY);

    if (inb(ATA_STATUS) == 0)           /* nenhum disco ligado */
        return false;
    if (!wait_not_busy())
        return false;
    if (inb(ATA_LBA1) || inb(ATA_LBA2)) /* nao e ATA (ex.: CD-ROM ATAPI) */
        return false;
    if (!wait_data())
        return false;
    insw(ATA_DATA, id, 256);

    /* nome do modelo: palavras 27-46, com os bytes de cada palavra trocados */
    for (int i = 0; i < 20; i++) {
        model[2 * i] = (char)(id[27 + i] >> 8);
        model[2 * i + 1] = (char)(id[27 + i] & 0xFF);
    }
    model[40] = '\0';
    for (int i = 39; i >= 0 && model[i] == ' '; i--)
        model[i] = '\0';

    total_sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    return true;
}

bool ata_read(uint32_t lba, uint32_t count, void *buf)
{
    uint8_t *p = buf;

    while (count) {
        uint32_t n = count > 128 ? 128 : count;

        if (!wait_not_busy())
            return false;
        outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));  /* mestre, modo LBA */
        delay400ns();
        outb(ATA_COUNT, (uint8_t)n);
        outb(ATA_LBA0, lba & 0xFF);
        outb(ATA_LBA1, (lba >> 8) & 0xFF);
        outb(ATA_LBA2, (lba >> 16) & 0xFF);
        outb(ATA_COMMAND, CMD_READ_SECTORS);

        for (uint32_t i = 0; i < n; i++) {
            delay400ns();
            if (!wait_data())
                return false;
            insw(ATA_DATA, p, 256);
            p += SECTOR_SIZE;
        }
        lba += n;
        count -= n;
    }
    return true;
}

const char *ata_model(void)   { return model; }
uint32_t ata_sectors(void)    { return total_sectors; }
