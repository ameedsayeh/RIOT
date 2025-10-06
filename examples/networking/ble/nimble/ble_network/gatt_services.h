/**
 * @file gatt_services.h
 * @brief GATT services definitions and management
 */

#ifndef GATT_SERVICES_H
#define GATT_SERVICES_H

#include <stdint.h>
#include <host/ble_gatt.h>

/* ========================================================================
 * GATT Service and Characteristic UUIDs
 * ======================================================================== */

/* Custom service UUID */
#define CUSTOM_SVC_UUID        0xff00

/* Custom characteristic UUIDs */
#define CUSTOM_NOTIFY_CHR_UUID 0xee00
#define CUSTOM_WRITE_CHR_UUID  0xee01

/* ========================================================================
 * Global Variables
 * ======================================================================== */

/* Value handles for characteristics */
extern uint16_t custom_notify_data_val_handle;
extern uint16_t custom_write_data_val_handle;

/* GATT services definition */
extern const struct ble_gatt_svc_def gatt_svcs[];

/* ========================================================================
 * Function Prototypes
 * ======================================================================== */

/**
 * @brief GATT notification characteristic access callback
 * @param conn_handle Connection handle
 * @param attr_handle Attribute handle
 * @param ctxt Access context
 * @param arg Callback argument
 * @return 0 on success, error code otherwise
 */
int notify_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt, void *arg);

/**
 * @brief GATT write characteristic access callback
 * @param conn_handle Connection handle
 * @param attr_handle Attribute handle
 * @param ctxt Access context
 * @param arg Callback argument
 * @return 0 on success, error code otherwise
 */
int write_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt, void *arg);

/**
 * @brief Initialize GATT services
 * @return 0 on success, error code otherwise
 */
int gatt_services_init(void);

#endif /* GATT_SERVICES_H */
