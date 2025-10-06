/**
 * @file ble_operations.h
 * @brief BLE advertising, scanning, and connection operations
 */

#ifndef BLE_OPERATIONS_H
#define BLE_OPERATIONS_H

#include <host/ble_gap.h>

/* ========================================================================
 * Function Prototypes
 * ======================================================================== */

/**
 * @brief Start BLE advertising
 */
void ble_start_advertise(void);

/**
 * @brief Stop BLE advertising
 */
void ble_stop_advertise(void);

/**
 * @brief Start BLE scanning
 */
void ble_start_scan(void);

/**
 * @brief Stop BLE scanning
 */
void ble_stop_scan(void);

/**
 * @brief Connection manager thread function
 * @param arg Thread argument (unused)
 * @return Thread return value
 */
void *connection_manager_thread(void *arg);

/**
 * @brief Advertising event callback
 * @param event GAP event
 * @param arg Callback argument
 * @return 0 on success
 */
int advertise_callback(struct ble_gap_event *event, void *arg);

/**
 * @brief Connection event callback
 * @param event GAP event
 * @param arg Callback argument
 * @return 0 on success
 */
int connect_callback(struct ble_gap_event *event, void *arg);

/**
 * @brief Scan event callback
 * @param event GAP event
 * @param arg Callback argument
 */
void scan_callback(struct ble_gap_event *event, void *arg);

#endif /* BLE_OPERATIONS_H */
