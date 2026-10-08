#include "main.h"

#define PROT_COMMAND        0xC0
#define PROT_ANSWER         0xA0
#define RESP_ACK            0xA1
#define RESP_BUSY           0xA2

#define CMD_LED_ON          0x01
#define CMD_LED_OFF         0x02
#define CMD_FLASH_INFO      0x03
#define CMD_FLASH_ERASE     0x04
#define CMD_FLASH_WRITE_SEG 0x05
#define CMD_FLASH_READ_SEG  0x06

#define USB_PKT_SIZE        64
#define WRITE_HDR           7   // C0, cmd, addr[4], len
#define READ_HDR            2   // A0, A1
#define MAX_WRITE_DATA      (USB_PKT_SIZE - WRITE_HDR)  // 57
#define MAX_READ_DATA       (USB_PKT_SIZE - READ_HDR)   // 62

volatile bool erasing = false;
static FLASH flash;

void handler_init(void) {
    flash = flash_init(PORT_FLASH_SPI, PIN_FLASH_MISO, PIN_FLASH_MOSI,
                       PIN_FLASH_SCK, PIN_FLASH_CS);
}

void prog_erase(void) {
    gpio_put(PIN_LED_EXT, 1);
    flash_erase(&flash);
    gpio_put(PIN_LED_EXT, 0);
    printf("erase: finished\n");
}

void main_handler(uint8_t *buf, uint16_t len) {
    if (len < 2 || buf[0] != PROT_COMMAND) return;

    uint8_t cmd = buf[1];
    uint32_t address;
    uint8_t n;

    if (erasing && cmd != CMD_LED_ON && cmd != CMD_LED_OFF) {
        buf[0] = PROT_ANSWER;
        buf[1] = RESP_BUSY;
        return;
    }

    switch (cmd) {

        case CMD_FLASH_ERASE:
            erasing = true;
            buf[0] = PROT_ANSWER;
            buf[1] = RESP_ACK;
            break;

        case CMD_FLASH_INFO:
            flash_info(&flash, &buf[2], &buf[3], &buf[4]);
            buf[0] = PROT_ANSWER;
            buf[1] = RESP_ACK;
            break;

        case CMD_LED_ON:
            gpio_put(PIN_LED_EXT, 1);
            buf[0] = PROT_ANSWER;
            buf[1] = RESP_ACK;
            break;

        case CMD_LED_OFF:
            gpio_put(PIN_LED_EXT, 0);
            buf[0] = PROT_ANSWER;
            buf[1] = RESP_ACK;
            break;

        case CMD_FLASH_WRITE_SEG:
            if (len < WRITE_HDR) return;
            address = buf[2] |
                (buf[3] << 8) |
                (buf[4] << 16) |
                ((uint32_t)buf[5] << 24);
            n = buf[6];
            if (n > MAX_WRITE_DATA) n = MAX_WRITE_DATA;
            if (n > len - WRITE_HDR) n = len - WRITE_HDR;
            flash_write(&flash, address, &buf[WRITE_HDR], n);
            buf[0] = PROT_ANSWER;
            buf[1] = RESP_ACK;
            break;

        case CMD_FLASH_READ_SEG:
            if (len < WRITE_HDR) return;
            address = buf[2] |
                (buf[3] << 8) |
                (buf[4] << 16) |
                ((uint32_t)buf[5] << 24);
            n = buf[6];
            if (n > MAX_READ_DATA) n = MAX_READ_DATA;
            flash_read(&flash, address, &buf[READ_HDR], n);
            buf[0] = PROT_ANSWER;
            buf[1] = RESP_ACK;
            break;
    }
}

void flash_worker_loop(void) {
    if (erasing) {
        prog_erase();
        erasing = false;
    }
}
