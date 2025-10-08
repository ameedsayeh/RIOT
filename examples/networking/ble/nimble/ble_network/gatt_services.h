/**
 * @file gatt_services.h
 * @brief GATT services definitions and management
 */

#ifndef GATT_SERVICES_H
#define GATT_SERVICES_H

#include <stdint.h>
#include <host/ble_gatt.h>
#include "messages.h"

/* ========================================================================
 * GATT Service and Characteristic UUIDs
 * ======================================================================== */

/* Custom service UUID */
#define CUSTOM_SVC_UUID       0xff00

/* Custom characteristic UUIDs */
#define CUSTOM_WRITE_CHR_UUID 0xee01

/* ========================================================================
 * Global Variables
 * ======================================================================== */

/* Value handles for characteristics */
extern uint16_t custom_write_data_val_handle;

/* GATT services definition */
extern const struct ble_gatt_svc_def gatt_svcs[];

/* ========================================================================
 * Function Prototypes
 * ======================================================================== */

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

/**
 * @brief Send an initial sync msg to a connected peer
 * @param conn_handle Connection handle
 * @return 0 on success, error code otherwise
 */
int start_sync(uint16_t conn_handle);

/**
 * @brief Sends sync msg to a connected peer
 * @param conn_handle Connection handle
 * @param msg Sync message to send
 * @return 0 on success, error code otherwise
 */
int send_tsync_msg(uint16_t conn_handle, tsync_msg msg);

/**
 * @brief Handle notification TX event for time sync protocol
 * @param conn_handle Connection handle
 * @param attr_handle Attribute handle
 */
void handle_sync_tx_event(uint16_t conn_handle, uint32_t timestamp);

/**
 * @brief Handle notification RX event for time sync protocol
 * @param conn_handle Connection handle
 * @param om Message buffer
 */
void handle_sync_rx_event(uint16_t conn_handle, struct os_mbuf *om, uint32_t timestamp);

#endif /* GATT_SERVICES_H */
