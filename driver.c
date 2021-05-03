// SPDX-License-Identifier: CC0-1.0

#include "tusb_option.h"

#include "usbtest.h"
#include "device/usbd_pvt.h"

static uint8_t _ctrl_req_buf[256];

#define BULK_BUFLEN    (32 * 1024)

static uint8_t _bulk_in_buf[BULK_BUFLEN];
static uint8_t _bulk_out_buf[BULK_BUFLEN];


static void usbtest_init(void)
{
    USBTEST_LOG1("%s:\n", __func__);

    usbtest_sourcesink_init(_bulk_in_buf, _bulk_out_buf, sizeof(_bulk_in_buf));
    usbtest_loopback_init(_bulk_out_buf, sizeof(_bulk_out_buf));
}

static void usbtest_reset(uint8_t rhport)
{
    (void) rhport;

    USBTEST_LOG1("%s: rhport%u\n", __func__, rhport);
}

static uint16_t usbtest_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
    USBTEST_LOG1("%s: bInterfaceNumber=%u max_len=%u\n", __func__, itf_desc->bInterfaceNumber, max_len);

    TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == itf_desc->bInterfaceClass, 0);

    uint16_t const len = sizeof(tusb_desc_interface_t) + itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t);
    TU_VERIFY(max_len >= len, 0);

    usbtest_sourcesink_disable(rhport);
    usbtest_loopback_disable(rhport);

    if (usbtest_current_configuration_idx == USBTEST_SOURCESINK)
        TU_ASSERT( usbtest_sourcesink_enable(rhport, itf_desc) );
    else
        TU_ASSERT( usbtest_loopback_enable(rhport, itf_desc) );

    USBTEST_LOG2("\n\n\n\n");

    return len;
}

static bool usbtest_control_request(uint8_t rhport, tusb_control_request_t const * req)
{
    uint16_t wLength;
    int ret;

    wLength = tu_min16(req->wLength, sizeof(_ctrl_req_buf));

    USBTEST_LOG2("%s:  bRequest=0x%02x bmRequestType=0x%x %s wLength=%u(%u) \n",
                 __func__, req->bRequest, req->bmRequestType,
                 req->bmRequestType_bit.direction ? "IN" : "OUT", wLength, req->wLength);

    if (req->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT) {
        // REVISIT: Is it necessary to do anything on endpoint set/clear stall?
        USBTEST_LOG2("TUSB_REQ_RCPT_ENDPOINT\n");
        return true;
    }

    if (req->bmRequestType_bit.type != TUSB_REQ_TYPE_STANDARD) {
        USBTEST_LOG2("NOT TUSB_REQ_TYPE_STANDARD: %u\n", req->bmRequestType_bit.type);
        return false;
    }

    switch (req->bRequest) {

    // Used by test #9
    // FIXME: tinyusb core should handle this
    case TUSB_REQ_GET_STATUS:
        if (req->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE ||
            req->bmRequestType_bit.direction != TUSB_DIR_IN)
            return false;

        _ctrl_req_buf[0] = 0;
        _ctrl_req_buf[1] = 0;
        wLength = tu_min16(wLength, 2);
        USBTEST_LOG2("TUSB_REQ_GET_STATUS: intf=%u\n", req->wIndex);
        break;

    // REVISIT: handled by upstream commit eeea19c0ab1c ("usbd ack SET_INTERFACE if it is not implemented by class driver.")
    case TUSB_REQ_SET_INTERFACE:
        if (req->bmRequestType != TUSB_REQ_RCPT_INTERFACE)
            return false;
        USBTEST_LOG2("TUSB_REQ_SET_INTERFACE: alt=%u intf=%u\n", req->wValue, req->wIndex);
        break;

    default:
        USBTEST_LOG2("REQ not recognised (core might handle it)\n");
        return false;
    }

    return tud_control_xfer(rhport, req, _ctrl_req_buf, wLength);
}

static bool usbtest_control_complete(uint8_t rhport, tusb_control_request_t const * req)
{
    USBTEST_LOG2("%s: bRequest=0x%02x bmRequestType=0x%x %s wLength=%u\n",
                 __func__, req->bRequest, req->bmRequestType,
                 req->bmRequestType_bit.direction ? "IN" : "OUT", req->wLength);

    return true;
}

static bool usbtest_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    USBTEST_LOG1("%s: ep_addr=0x%02x result=%u xferred_bytes=%u\n", __func__, ep_addr, result, xferred_bytes);

    board_led_activity();

    TU_VERIFY(result == XFER_RESULT_SUCCESS);

    if (!xferred_bytes)
        USBTEST_LOG2("                 ZLP\n");

    if (usbtest_current_configuration_idx == USBTEST_SOURCESINK)
        return usbtest_sourcesink_complete(rhport, ep_addr, xferred_bytes);
    else
        return usbtest_loopback_complete(rhport, ep_addr, xferred_bytes);
}

static usbd_class_driver_t const _usbtest_driver[] =
{
    {
  #if CFG_TUSB_DEBUG >= 2
        .name             = "usbtest",
  #endif
        .init             = usbtest_init,
        .reset            = usbtest_reset,
        .open             = usbtest_open,
        .control_request  = usbtest_control_request,
        .control_complete = usbtest_control_complete,
        .xfer_cb          = usbtest_xfer_cb,
        .sof              = NULL
    },
};

usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count)
{
	*driver_count += TU_ARRAY_SIZE(_usbtest_driver);

	return _usbtest_driver;
}
