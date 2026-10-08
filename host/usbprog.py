import time

USB_VENDOR_ID = 0x0000
USB_PRODUCT_ID = 0x0001

PROT_COMMAND = 0xC0
PROT_ANSWER = 0xA0

CMD_LED_ON = 0x01
CMD_LED_OFF = 0x02
CMD_FLASH_INFO = 0x03
CMD_FLASH_ERASE = 0x04
CMD_FLASH_WRITE_SEG = 0x05
CMD_FLASH_READ_SEG = 0x06

RESP_ACK = 0xA1
RESP_BUSY = 0xA2
MAX_DATA = 57

JEDEC_INFO = {
    (0x00, 0x00, 0x00): "No flash detected",
    (0xEF, 0x40, 0x16): "Winbond W25Q32 (32Mbit/4MB)",
    (0xEF, 0x40, 0x17): "Winbond W25Q64 (64Mbit/8MB)",
    (0xEF, 0x40, 0x18): "Winbond W25Q128 (128Mbit/16MB)",
}

try:
    import usb.core
    import usb.util
except ImportError:
    raise ImportError("Can't import module usb.core (Type pip3 install pyusb)")


class uProgDeviceError(Exception):
    """
        uProgDeviceError Exception
    """
    pass


class uProgDeviceBusy(uProgDeviceError):
    """
        Device is busy (flash erase in progress)
    """
    pass


class uProgDevice():
    """
        uProgDeviceError Device
    """
    def __init__(self):
        self.device = usb.core.find(idVendor=USB_VENDOR_ID, idProduct=USB_PRODUCT_ID)
        if self.device is None:
            raise uProgDeviceError("uProgDevice not found!")
        cfg = self.device.get_active_configuration()
        intf = cfg[(0, 0)]
        self.outep = usb.util.find_descriptor(intf, custom_match= lambda e: \
            usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT)
        self.inep = usb.util.find_descriptor(intf, custom_match= lambda e: \
            usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN)
        assert self.inep is not None
        assert self.outep is not None

    def do(self, data):
        data = [PROT_COMMAND] + data
        self.outep.write(bytearray(data))
        from_device = self.inep.read(len(data))
        ret = list(from_device)
        if ret[:2] == [PROT_ANSWER, RESP_BUSY]:
            raise uProgDeviceBusy(f"Command (0x{data[1]:02X}) rejected: device busy (erase in progress)")
        if not ret[:2] == [PROT_ANSWER, RESP_ACK]:
            raise uProgDeviceError(f"Command (0x{data[1]:02X}) not ACK")
        return ret[2:]

    def led_on(self):
        self.do([CMD_LED_ON])

    def led_off(self):
        self.do([CMD_LED_OFF])

    def flash_info(self):
        jedec_info = self.do([CMD_FLASH_INFO, 0x00, 0x00, 0x00])
        p = tuple([int(x) for x in jedec_info])
        try:
            return JEDEC_INFO[p]
        except KeyError:
            return f"Unknown flash (0x{jedec_info[0]:02X},0x{jedec_info[1]:02X},0x{jedec_info[2]:02X})"

    def flash_info_jedec(self):
        jedec_info = self.do([CMD_FLASH_INFO, 0x00, 0x00, 0x00])
        return tuple([int(x) for x in jedec_info])
         
    def flash_erase(self, wait=True, poll=1.0, on_wait=None):
        self.do([CMD_FLASH_ERASE])
        if not wait:
            return
        start = time.monotonic()
        while True:
            time.sleep(poll)
            if on_wait:
                on_wait(time.monotonic() - start)
            try:
                self.flash_info_jedec()
                break
            except uProgDeviceBusy:
                pass
        
    def flash_write_seg(self, data, addr):
        length = len(data)
        if length > MAX_DATA:
            raise uProgDeviceError(f"Too many bytes")
        cmd = [
            CMD_FLASH_WRITE_SEG,
            addr & 0xFF,
            (addr >> 8) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 24) & 0xFF,
            length,
        ]
        cmd += data
        self.do(cmd)
    
    def flash_read_seg(self, addr, length):
        if length > MAX_DATA:
            raise uProgDeviceError(f"Too many bytes")
        cmd = [
            CMD_FLASH_READ_SEG,
            addr & 0xFF,
            (addr >> 8) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 24) & 0xFF,
            length,
        ] + ([0x00]*(64-7))
        return self.do(cmd)[:length] 
