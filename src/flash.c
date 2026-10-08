#include "main.h"

#define FLASH_SPI_FREQ (20*1000*1000)
#define FLASH_SIZE     (8*1024*1024)
#define SECTOR_SIZE    4096
#define PAGE_SIZE      256

static void flash_select(FLASH* f) {
    gpio_put(f->cs, 0);
}

static void flash_deselect(FLASH* f) {
    gpio_put(f->cs, 1);
}

static void flash_write_enable(FLASH* f) {
    uint8_t cmd = 0x06;
    flash_select(f);
    spi_write_blocking(f->spi, &cmd, 1);
    flash_deselect(f);
}

static void flash_wait(FLASH* f) {
    uint8_t status;
    uint8_t cmd = 0x05;
    flash_select(f);
    spi_write_blocking(f->spi, &cmd, 1);
    do {
        spi_read_blocking(f->spi, 0, &status, 1);
    } while (status & 1);
    flash_deselect(f);
}

void flash_read(FLASH* f, uint32_t addr, uint8_t* buf, size_t len) {
    flash_wait(f);
    uint8_t cmd[4] = {0x03, (addr>>16)&0xFF, (addr>>8)&0xFF, addr&0xFF};
    flash_select(f);
    spi_write_blocking(f->spi, cmd, 4);
    spi_read_blocking(f->spi, 0, buf, len);
    flash_deselect(f);
}

void flash_write(FLASH* f, uint32_t addr, const uint8_t* buf, size_t len) {
    while (len) {
        size_t n = PAGE_SIZE - (addr & (PAGE_SIZE - 1));
        if (n > len) n = len;

        flash_wait(f);
        flash_write_enable(f);
        uint8_t cmd[4] = {0x02, (addr>>16)&0xFF, (addr>>8)&0xFF, addr&0xFF};
        flash_select(f);
        spi_write_blocking(f->spi, cmd, 4);
        spi_write_blocking(f->spi, buf, n);
        flash_deselect(f);
        flash_wait(f);

        addr += n;
        buf  += n;
        len  -= n;
    }
}

static void flash_erase_sector(FLASH* f, uint32_t addr) {
    flash_wait(f);
    flash_write_enable(f);
    uint8_t cmd[4] = {0x20, (addr>>16)&0xFF, (addr>>8)&0xFF, addr&0xFF};
    flash_select(f);
    spi_write_blocking(f->spi, cmd, 4);
    flash_deselect(f);
    flash_wait(f);
}

FLASH flash_init(spi_inst_t* spi, uint miso, uint mosi, uint sck, uint cs) {
    spi_init(spi, FLASH_SPI_FREQ);
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);
    gpio_set_function(sck, GPIO_FUNC_SPI);
    FLASH f;
    f.spi = spi;
    f.cs = cs;
    gpio_init(cs);
    gpio_put(cs, 1);
    gpio_set_dir(cs, GPIO_OUT);
    printf("flash_init: done\n");
    return f;
}

void flash_info(FLASH* f, uint8_t* i1, uint8_t* i2, uint8_t* i3) {
    uint8_t cmd = 0x9F;
    uint8_t resp[3];
    flash_wait(f);
    flash_select(f);
    spi_write_blocking(f->spi, &cmd, 1);
    spi_read_blocking(f->spi, 0, resp, 3);
    flash_deselect(f);
    *i1 = resp[0];
    *i2 = resp[1];
    *i3 = resp[2];
}

void flash_erase(FLASH* f) {
    for (uint32_t offset = 0; offset < FLASH_SIZE; offset += SECTOR_SIZE) {
        flash_erase_sector(f, offset);
    }
}
