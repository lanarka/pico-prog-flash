"""
    run.py

    usage:
        run.py info
        run.py erase
        run.py write <filename> [address]      (flash must be erased!)
        run.py read  <filename> <size> [address]
        run.py verify <filename> [address]
        run.py test
"""
import os
import sys
import time
from usbprog import uProgDevice, MAX_DATA

FLASH_START_ADDR = 0x00000000
FLASH_CHUNK_SIZE = MAX_DATA


def printProgressBar(iteration, total, prefix='', suffix='', decimals=1,
                     length=40, fill='█', printEnd="\r"):
    if total <= 0:
        return
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    filledLength = int(length * iteration // total)
    bar = fill * filledLength + '-' * (length - filledLength)
    print(f'\r{prefix} |{bar}| {percent}% {suffix}', end=printEnd, flush=True)
    if iteration == total:
        print()


def flashfile(device, filename, start_addr=FLASH_START_ADDR):
    total = os.path.getsize(filename)
    addr = start_addr
    done = 0
    printProgressBar(0, total, prefix='Write ', suffix=f'0/{total} B')
    with open(filename, "rb") as f:
        while True:
            chunk = f.read(FLASH_CHUNK_SIZE)
            if not chunk:
                break
            device.flash_write_seg(list(chunk), addr)
            addr += len(chunk)
            done += len(chunk)
            printProgressBar(done, total, prefix='Write ',
                             suffix=f'{done}/{total} B')
    return done


def readflash(device, size, start_addr=FLASH_START_ADDR, prefix='Read  '):
    out = bytearray()
    addr = start_addr
    remaining = size
    printProgressBar(0, size, prefix=prefix, suffix=f'0/{size} B')
    while remaining > 0:
        chunk_size = min(FLASH_CHUNK_SIZE, remaining)
        out += bytes(device.flash_read_seg(addr, chunk_size))
        addr += chunk_size
        remaining -= chunk_size
        printProgressBar(len(out), size, prefix=prefix,
                         suffix=f'{len(out)}/{size} B')
    return bytes(out)


def readflashfile(device, filename, size, start_addr=FLASH_START_ADDR):
    data = readflash(device, size, start_addr)
    with open(filename, "wb") as f:
        f.write(data)


def verifyfile(device, filename, start_addr=FLASH_START_ADDR):
    with open(filename, "rb") as f:
        expected = f.read()
    got = readflash(device, len(expected), start_addr, prefix='Verify')
    if got == expected:
        print(f"VERIFY OK ({len(expected)} bytes)")
        return True
    for i, (a, b) in enumerate(zip(expected, got)):
        if a != b:
            print(f"VERIFY FAILED: first difference at offset 0x{i:X} "
                  f"(addr 0x{start_addr + i:X}): expected 0x{a:02X}, got 0x{b:02X}")
            break
    return False


def erase_chip(device):
    spinner = "|/-\\"
    state = {"i": 0}

    def on_wait(elapsed):
        state["i"] += 1
        print(f"\rErase  {spinner[state['i'] % 4]} {elapsed:5.0f} s",
              end="", flush=True)

    t0 = time.monotonic()
    device.flash_erase(on_wait=on_wait)
    print(f"\rErase  done in {time.monotonic() - t0:.0f} s        ")


def big_test(device, address=0x00000001):
    test = list(range(0, MAX_DATA))
    device.flash_write_seg(test, address)
    ret = device.flash_read_seg(address, MAX_DATA)
    print(test)
    print(ret)
    assert ret == test
    print('PASS')


def big_test2(device, address=0x00010000):
    big_test(device, address)


def main(argv):
    device = uProgDevice()
    device.led_on()
    device.led_off()

    if len(argv) < 2:
        print(__doc__)
        return 1
    cmd = argv[1]

    if cmd == "info":
        print("flash-info:", device.flash_info())
    elif cmd == "erase":
        erase_chip(device)
    elif cmd == "write":
        addr = int(argv[3], 0) if len(argv) > 3 else FLASH_START_ADDR
        n = flashfile(device, argv[2], addr)
        print(f"written {n} bytes")
        verifyfile(device, argv[2], addr)
    elif cmd == "read":
        addr = int(argv[4], 0) if len(argv) > 4 else FLASH_START_ADDR
        readflashfile(device, argv[2], int(argv[3], 0), addr)
    elif cmd == "verify":
        addr = int(argv[3], 0) if len(argv) > 3 else FLASH_START_ADDR
        return 0 if verifyfile(device, argv[2], addr) else 2
    elif cmd == "test":
        big_test(device)
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
