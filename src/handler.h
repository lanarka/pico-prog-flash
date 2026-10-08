#ifndef _HANDLER_H
#define _HANDLER_H

#include "flash.h"

void handler_init(void);
void prog_erase(void);
void main_handler(uint8_t *buf, uint16_t len);
void flash_worker_loop(void);

#endif
