#ifndef _FLASH_H
#define _FLASH_H

typedef struct {
    spi_inst_t *spi;
    uint cs;
} FLASH;

FLASH flash_init(spi_inst_t* spi, uint miso, uint mosi, uint sck, uint cs);
void flash_info(FLASH* f, uint8_t* i1, uint8_t* i2, uint8_t* i3);
void flash_erase(FLASH* f);

void flash_read(FLASH* f, uint32_t addr, uint8_t* buf, size_t len);
void flash_write(FLASH* f, uint32_t addr, const uint8_t* buf, size_t len);

#endif
