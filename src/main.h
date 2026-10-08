#ifndef _MAIN_H
#define _MAIN_H

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "hardware/regs/usb.h"
#include "hardware/structs/usb.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/spi.h"

#include "usb_common.h"
#include "dev_lowlevel.h"

#include "board.h"
#include "tools.h"
#include "handler.h"
#include "flash.h"

#define usb_hw_set ((usb_hw_t *)hw_set_alias_untyped(usb_hw))
#define usb_hw_clear ((usb_hw_t *)hw_clear_alias_untyped(usb_hw))

void ep0_in_handler(uint8_t *buf, uint16_t len);
void ep0_out_handler(uint8_t *buf, uint16_t len);
void ep1_out_handler(uint8_t *buf, uint16_t len);
void ep2_in_handler(uint8_t *buf, uint16_t len);

#endif
