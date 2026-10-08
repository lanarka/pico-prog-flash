/* 
    
    USB FLASH PROGRAMMER

*/
#include "main.h"

static bool should_set_address = false;
static uint8_t dev_addr = 0;
static volatile bool configured = false;
static uint8_t ep0_buf[64];
static struct usb_device_configuration dev_config = {
    .device_descriptor = &device_descriptor,
    .interface_descriptor = &interface_descriptor,
    .config_descriptor = &config_descriptor,
    .lang_descriptor = lang_descriptor,
    .descriptor_strings = descriptor_strings,
    .endpoints = {
            {
                .descriptor = &ep0_out,
                .handler = &ep0_out_handler,
                .endpoint_control = NULL,
                .buffer_control = &usb_dpram->ep_buf_ctrl[0].out,
                .data_buffer = &usb_dpram->ep0_buf_a[0],
            },
            {
                .descriptor = &ep0_in,
                .handler = &ep0_in_handler,
                .endpoint_control = NULL,
                .buffer_control = &usb_dpram->ep_buf_ctrl[0].in,
                .data_buffer = &usb_dpram->ep0_buf_a[0],
            },
            {
                .descriptor = &ep1_out,
                .handler = &ep1_out_handler,
                .endpoint_control = &usb_dpram->ep_ctrl[0].out,
                .buffer_control = &usb_dpram->ep_buf_ctrl[1].out,
                .data_buffer = &usb_dpram->epx_data[0 * 64],
            },
            {
                .descriptor = &ep2_in,
                .handler = &ep2_in_handler,
                .endpoint_control = &usb_dpram->ep_ctrl[1].in,
                .buffer_control = &usb_dpram->ep_buf_ctrl[2].in,
                .data_buffer = &usb_dpram->epx_data[1 * 64],
            }
    }
};

struct usb_endpoint_configuration *usb_get_endpoint_configuration(uint8_t addr) {
    struct usb_endpoint_configuration *endpoints = dev_config.endpoints;
    for (int i = 0; i < USB_NUM_ENDPOINTS; i++) {
        if (endpoints[i].descriptor && (endpoints[i].descriptor->bEndpointAddress == addr)) {
            return &endpoints[i];
        }
    }
    return NULL;
}

uint8_t usb_prepare_string_descriptor(const unsigned char *str) {
    uint8_t bLength = 2 + (strlen((const char *)str) * 2);
    static const uint8_t bDescriptorType = 0x03;
    volatile uint8_t *buf = &ep0_buf[0];
    *buf++ = bLength;
    *buf++ = bDescriptorType;
    uint8_t c;
    do {
        c = *str++;
        *buf++ = c;
        *buf++ = 0;
    } while (c != '\0');
    return bLength;
}

static inline uint32_t usb_buffer_offset(volatile uint8_t *buf) {
    return (uint32_t) buf ^ (uint32_t) usb_dpram;
}

void usb_setup_endpoint(const struct usb_endpoint_configuration *ep) {
    //printf("Set up endpoint 0x%x with buffer address 0x%p\n", ep->descriptor->bEndpointAddress, ep->data_buffer);
    if (!ep->endpoint_control) {
        return;
    }
    uint32_t dpram_offset = usb_buffer_offset(ep->data_buffer);
    uint32_t reg = EP_CTRL_ENABLE_BITS
                   | EP_CTRL_INTERRUPT_PER_BUFFER
                   | (ep->descriptor->bmAttributes << EP_CTRL_BUFFER_TYPE_LSB)
                   | dpram_offset;
    *ep->endpoint_control = reg;
}

void usb_setup_endpoints() {
    const struct usb_endpoint_configuration *endpoints = dev_config.endpoints;
    for (int i = 0; i < USB_NUM_ENDPOINTS; i++) {
        if (endpoints[i].descriptor && endpoints[i].handler) {
            usb_setup_endpoint(&endpoints[i]);
        }
    }
}

void usb_device_init() {
    reset_unreset_block_num_wait_blocking(RESET_USBCTRL);
    memset(usb_dpram, 0, sizeof(*usb_dpram));
    irq_set_enabled(USBCTRL_IRQ, true);
    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
    usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;
    usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS;
    usb_hw->inte = USB_INTS_BUFF_STATUS_BITS |
                   USB_INTS_BUS_RESET_BITS |
                   USB_INTS_SETUP_REQ_BITS;
    usb_setup_endpoints();
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}

static inline bool ep_is_tx(struct usb_endpoint_configuration *ep) {
    return ep->descriptor->bEndpointAddress & USB_DIR_IN;
}

void usb_start_transfer(struct usb_endpoint_configuration *ep, uint8_t *buf, uint16_t len) {
    assert(len <= 64);
    //printf("Start transfer of len %d on ep addr 0x%x\n", len, ep->descriptor->bEndpointAddress);
    uint32_t val = len | USB_BUF_CTRL_AVAIL;
    if (ep_is_tx(ep)) {
        memcpy((void *) ep->data_buffer, (void *) buf, len);
        val |= USB_BUF_CTRL_FULL;
    }
    val |= ep->next_pid ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID;
    ep->next_pid ^= 1u;
    *ep->buffer_control = val;
}

void usb_handle_device_descriptor(volatile struct usb_setup_packet *pkt) {
    const struct usb_device_descriptor *d = dev_config.device_descriptor;
    struct usb_endpoint_configuration *ep = usb_get_endpoint_configuration(EP0_IN_ADDR);
    ep->next_pid = 1;
    usb_start_transfer(ep, (uint8_t *) d, MIN(sizeof(struct usb_device_descriptor), pkt->wLength));
}

void usb_handle_config_descriptor(volatile struct usb_setup_packet *pkt) {
    uint8_t *buf = &ep0_buf[0];
    const struct usb_configuration_descriptor *d = dev_config.config_descriptor;
    memcpy((void *) buf, d, sizeof(struct usb_configuration_descriptor));
    buf += sizeof(struct usb_configuration_descriptor);
    if (pkt->wLength >= d->wTotalLength) {
        memcpy((void *) buf, dev_config.interface_descriptor, sizeof(struct usb_interface_descriptor));
        buf += sizeof(struct usb_interface_descriptor);
        const struct usb_endpoint_configuration *ep = dev_config.endpoints;
        for (uint i = 2; i < USB_NUM_ENDPOINTS; i++) {
            if (ep[i].descriptor) {
                memcpy((void *) buf, ep[i].descriptor, sizeof(struct usb_endpoint_descriptor));
                buf += sizeof(struct usb_endpoint_descriptor);
            }
        }

    }
    uint32_t len = (uint32_t) buf - (uint32_t) &ep0_buf[0];
    usb_start_transfer(usb_get_endpoint_configuration(EP0_IN_ADDR), &ep0_buf[0], MIN(len, pkt->wLength));
}

void usb_bus_reset(void) {
    dev_addr = 0;
    should_set_address = false;
    usb_hw->dev_addr_ctrl = 0;
    configured = false;
}

void usb_handle_string_descriptor(volatile struct usb_setup_packet *pkt) {
    uint8_t i = pkt->wValue & 0xff;
    uint8_t len = 0;

    if (i == 0) {
        len = 4;
        memcpy(&ep0_buf[0], dev_config.lang_descriptor, len);
    } else {
        len = usb_prepare_string_descriptor(dev_config.descriptor_strings[i - 1]);
    }

    usb_start_transfer(usb_get_endpoint_configuration(EP0_IN_ADDR), &ep0_buf[0], MIN(len, pkt->wLength));
}

void usb_acknowledge_out_request(void) {
    usb_start_transfer(usb_get_endpoint_configuration(EP0_IN_ADDR), NULL, 0);
}

void usb_set_device_address(volatile struct usb_setup_packet *pkt) {
    dev_addr = (pkt->wValue & 0xff);
    //printf("Set address %d\r\n", dev_addr);
    should_set_address = true;
    usb_acknowledge_out_request();
}

void usb_set_device_configuration(__unused volatile struct usb_setup_packet *pkt) {
    //printf("Device Enumerated\r\n");
    usb_acknowledge_out_request();
    configured = true;
}

void usb_handle_setup_packet(void) {
    volatile struct usb_setup_packet *pkt = (volatile struct usb_setup_packet *) &usb_dpram->setup_packet;
    uint8_t req_direction = pkt->bmRequestType;
    uint8_t req = pkt->bRequest;
    usb_get_endpoint_configuration(EP0_IN_ADDR)->next_pid = 1u;
    if (req_direction == USB_DIR_OUT) {
        if (req == USB_REQUEST_SET_ADDRESS) {
            usb_set_device_address(pkt);
        } else if (req == USB_REQUEST_SET_CONFIGURATION) {
            usb_set_device_configuration(pkt);
        } else {
            usb_acknowledge_out_request();
            //printf("Other OUT request (0x%x)\r\n", pkt->bRequest);
        }
    } else if (req_direction == USB_DIR_IN) {
        if (req == USB_REQUEST_GET_DESCRIPTOR) {
            uint16_t descriptor_type = pkt->wValue >> 8;

            switch (descriptor_type) {
                case USB_DT_DEVICE:
                    usb_handle_device_descriptor(pkt);
                    //printf("GET DEVICE DESCRIPTOR\r\n");
                    break;

                case USB_DT_CONFIG:
                    usb_handle_config_descriptor(pkt);
                    //printf("GET CONFIG DESCRIPTOR\r\n");
                    break;

                case USB_DT_STRING:
                    usb_handle_string_descriptor(pkt);
                    //printf("GET STRING DESCRIPTOR\r\n");
                    break;

                default:
                    printf("Unhandled GET_DESCRIPTOR type 0x%x\r\n", descriptor_type);
            }
        } else {
            //printf("Other IN request (0x%x)\r\n", pkt->bRequest);
        }
    }
}

static void usb_handle_ep_buff_done(struct usb_endpoint_configuration *ep) {
    uint32_t buffer_control = *ep->buffer_control;
    uint16_t len = buffer_control & USB_BUF_CTRL_LEN_MASK;
    ep->handler((uint8_t *) ep->data_buffer, len);
}

static void usb_handle_buff_done(uint ep_num, bool in) {
    uint8_t ep_addr = ep_num | (in ? USB_DIR_IN : 0);
    //printf("EP %d (in = %d) done\n", ep_num, in);
    for (uint i = 0; i < USB_NUM_ENDPOINTS; i++) {
        struct usb_endpoint_configuration *ep = &dev_config.endpoints[i];
        if (ep->descriptor && ep->handler) {
            if (ep->descriptor->bEndpointAddress == ep_addr) {
                usb_handle_ep_buff_done(ep);
                return;
            }
        }
    }
}

static void usb_handle_buff_status() {
    uint32_t buffers = usb_hw->buf_status;
    uint32_t remaining_buffers = buffers;
    uint bit = 1u;
    for (uint i = 0; remaining_buffers && i < USB_NUM_ENDPOINTS * 2; i++) {
        if (remaining_buffers & bit) {
            usb_hw_clear->buf_status = bit;
            usb_handle_buff_done(i >> 1u, !(i & 1u));
            remaining_buffers &= ~bit;
        }
        bit <<= 1u;
    }
}

void isr_usbctrl(void) {
    uint32_t status = usb_hw->ints;
    uint32_t handled = 0;
    if (status & USB_INTS_SETUP_REQ_BITS) {
        handled |= USB_INTS_SETUP_REQ_BITS;
        usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
        usb_handle_setup_packet();
    }
    if (status & USB_INTS_BUFF_STATUS_BITS) {
        handled |= USB_INTS_BUFF_STATUS_BITS;
        usb_handle_buff_status();
    }
    if (status & USB_INTS_BUS_RESET_BITS) {
        printf("BUS RESET\n");
        handled |= USB_INTS_BUS_RESET_BITS;
        usb_hw_clear->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;
        usb_bus_reset();
    }
    if (status ^ handled) {
        panic("Unhandled IRQ 0x%x\n", (uint) (status ^ handled));
    }
}

void ep0_in_handler(__unused uint8_t *buf, __unused uint16_t len) {
    if (should_set_address) {
        usb_hw->dev_addr_ctrl = dev_addr;
        should_set_address = false;
    } else {
        struct usb_endpoint_configuration *ep = usb_get_endpoint_configuration(EP0_OUT_ADDR);
        usb_start_transfer(ep, NULL, 0);
    }
}

void ep0_out_handler(__unused uint8_t *buf, __unused uint16_t len) {

}

void ep1_out_handler(uint8_t *buf, uint16_t len) {
    gpio_put(PIN_LED_INT, 1);
    main_handler(buf, len);
    struct usb_endpoint_configuration *ep = usb_get_endpoint_configuration(EP2_IN_ADDR);
    usb_start_transfer(ep, buf, len);
}

void ep2_in_handler(uint8_t *buf, uint16_t len) {
    //printf("------Sent %d bytes to host\n", len);
    usb_start_transfer(usb_get_endpoint_configuration(EP1_OUT_ADDR), NULL, 64);
    gpio_put(PIN_LED_INT, 0);
}

int main(void) {
    stdio_init_all();
    print_span();
    
    usb_device_init();
    handler_init();
    
    gpio_init(PIN_BUTTON);
    gpio_set_dir(PIN_BUTTON, GPIO_IN);
    gpio_pull_up(PIN_BUTTON);

    gpio_init(PIN_LED_INT);
    gpio_set_dir(PIN_LED_INT, GPIO_OUT);
    gpio_put(PIN_LED_INT, 0);

    gpio_init(PIN_LED_EXT);
    gpio_set_dir(PIN_LED_EXT, GPIO_OUT);
    gpio_put(PIN_LED_EXT, 0);

    if (!gpio_get(PIN_BUTTON)) {
        gpio_put(PIN_LED_EXT, 1);
        prog_erase();
        gpio_put(PIN_LED_EXT, 0);
        while (1) {
            tight_loop_contents();
        }        
    }

    printf("Device ready!\n");
    while (!configured) {
        tight_loop_contents();
    }

    usb_start_transfer(usb_get_endpoint_configuration(EP1_OUT_ADDR), NULL, 64);
    while (1) {
        flash_worker_loop();
        tight_loop_contents();
    }
}
