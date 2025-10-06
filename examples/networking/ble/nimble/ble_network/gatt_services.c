/**
 * @file gatt_services.c
 * @brief GATT services implementation
 */

#include "gatt_services.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

/* ========================================================================
 * Global Variables
 * ======================================================================== */

/* Value handles for GATT characteristics */
uint16_t custom_notify_data_val_handle;
uint16_t custom_write_data_val_handle;

/* ========================================================================
 * GATT Characteristic Access Callbacks
 * ======================================================================== */

int notify_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        printf("[GATT] Notify characteristic read\n");
        /* Return some dummy data for notification */
        {
            static const char *notify_data = "Hello from notify!";
            int rc = os_mbuf_append(ctxt->om, notify_data, strlen(notify_data));
            return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        printf("[GATT] Notify characteristic write (not supported)\n");
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;

    default:
        printf("[GATT] Notify characteristic: unexpected access op %d\n", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

int write_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        printf("[GATT] Write characteristic read\n");
        /* Return current write buffer content */
        {
            static const char *write_data = "Write buffer";
            int rc = os_mbuf_append(ctxt->om, write_data, strlen(write_data));
            return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        printf("[GATT] Write characteristic write: len=%d\n",
               OS_MBUF_PKTLEN(ctxt->om));

        /* Print received data */
        struct os_mbuf *om = ctxt->om;
        uint8_t buf[256];
        int len = OS_MBUF_PKTLEN(om);
        if (len > sizeof(buf) - 1) {
            len = sizeof(buf) - 1;
        }

        if (os_mbuf_copydata(om, 0, len, buf) == 0) {
            buf[len] = '\0';
            printf("[GATT] Received data: %s\n", buf);
        }

        return 0;

    default:
        printf("[GATT] Write characteristic: unexpected access op %d\n", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* ========================================================================
 * GATT Services Definition
 * ======================================================================== */

const struct ble_gatt_svc_def gatt_svcs[] = {
    { /* Custom Service */
      .type = BLE_GATT_SVC_TYPE_PRIMARY,
      .uuid = BLE_UUID16_DECLARE(CUSTOM_SVC_UUID),
      .characteristics = (struct ble_gatt_chr_def[]){
          {
              /* Custom Notify Characteristic */
              .uuid = BLE_UUID16_DECLARE(CUSTOM_NOTIFY_CHR_UUID),
              .access_cb = notify_access_cb,
              .val_handle = &custom_notify_data_val_handle,
              .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
          },
          {
              /* Custom Write Characteristic */
              .uuid = BLE_UUID16_DECLARE(CUSTOM_WRITE_CHR_UUID),
              .access_cb = write_access_cb,
              .val_handle = &custom_write_data_val_handle,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
          },
          {
              0, /* no more characteristics in this service */
          } } },
    {
        0, /* no more services */
    },
};

/* ========================================================================
 * GATT Services Initialization
 * ======================================================================== */

int gatt_services_init(void)
{
    int rc;

    /* Count the configuration */
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        printf("ble_gatts_count_cfg() failed: %d\n", rc);
        return rc;
    }

    /* Add services to the GATT server */
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        printf("ble_gatts_add_svcs() failed: %d\n", rc);
        return rc;
    }

    /* Start GATT server */
    rc = ble_gatts_start();
    if (rc != 0) {
        printf("ble_gatts_start() failed: %d\n", rc);
        return rc;
    }

    printf("GATT services initialized successfully\n");
    printf("  - Custom Service UUID: 0x%04X\n", CUSTOM_SVC_UUID);
    printf("  - Notify Characteristic UUID: 0x%04X (handle: %d)\n",
           CUSTOM_NOTIFY_CHR_UUID, custom_notify_data_val_handle);
    printf("  - Write Characteristic UUID: 0x%04X (handle: %d)\n",
           CUSTOM_WRITE_CHR_UUID, custom_write_data_val_handle);

    return 0;
}
