#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include "types.h"

#define SECTOR_SIZE 512

bool ata_init(void);
bool ata_read(uint32_t lba, uint32_t count, void *buf);
const char *ata_model(void);
uint32_t ata_sectors(void);

#endif
