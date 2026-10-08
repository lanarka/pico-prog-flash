# pico-prog-flash

A simple USB SPI-flash programmer for the Raspberry Pi Pico (RP2040), written with
the pico-sdk. The firmware talks to the PC over USB bulk endpoints (custom protocol,
no drivers needed)

Targets 25-series SPI NOR flash chips (up to 8 MB, e.g. W25Q64) using 4 KB sector
erase, 256-byte page program and the standard `0x03` read command.

## Wiring

| Signal | RP2040 pin | Notes                         |
|--------|------------|-------------------------------|
| MISO   | GPIO 12    | SPI1 RX                       |
| CS     | GPIO 7     |                               |
| SCK    | GPIO 14    | SPI1 SCK, 20 MHz              |
| MOSI   | GPIO 15    | SPI1 TX                       |
| Button | GPIO 22    | active low, pull-up           |
| LEDs   | GPIO 5 / 25| external / on-board LED       |


## Build and flash the firmware

Requires the pico-sdk (`PICO_SDK_PATH` set), CMake and the ARM toolchain.

```sh
./clean      # fresh build/ directory + cmake + make
./compile    # incremental build
./flash      # picotool load -v -x build/thing.uf2 -f
```

**Chip erase on boot:** hold the button (GPIO 22) while powering up. The device
erases the whole flash, the LED stays on during the erase, and the firmware then
idles.

## Host (read/write flash)

```sh
pip3 install pyusb
cd host
sudo python3 run.py info
sudo python3 run.py erase
sudo python3 run.py write data.bin [address]
sudo python3 run.py read dump.bin <size> [address]
sudo python3 run.py verify data.bin [address]
```

Addresses and sizes accept decimal or `0x` hex. All operations show a progress bar;
`write` verifies automatically after programming.

**Important:** flash can only be programmed from 1 to 0. Always `erase` first,
otherwise the result is `old AND new`. A full 8 MB erase takes from tens of seconds
to a few minutes; the device reports BUSY for any flash command until it finishes,
and `erase` waits for it.

## Protocol

Packets are 64 bytes. Host to device: `0xC0, cmd, ...`; reply: `0xA0, 0xA1 (ACK) | 0xA2 (BUSY), ...`.

| Cmd  | Name           | Payload                                  |
|------|----------------|------------------------------------------|
| 0x01 | LED_ON         |                                          |
| 0x02 | LED_OFF        |                                          |
| 0x03 | FLASH_INFO     | reply: JEDEC manufacturer, type, capacity|
| 0x04 | FLASH_ERASE    | starts chip erase, ACK is immediate      |
| 0x05 | FLASH_WRITE_SEG| addr (4 B LE), len, data (max 57 B)      |
| 0x06 | FLASH_READ_SEG | addr (4 B LE), len (max 62 B); data at reply[2:] |

Writes are completed before the ACK is sent, and the firmware splits them at
256-byte page boundaries, so any address and length are safe.

## Project layout

```
src/    firmware (main.c: USB, handler.c: protocol, flash.c: SPI flash driver)
host/   usbprog.py (device library), run.py (command-line tool)
```
