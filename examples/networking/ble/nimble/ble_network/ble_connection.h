/**
 * @file ble_connection.h
 * @brief BLE connection management interface
 */

#ifndef BLE_CONNECTION_H
#define BLE_CONNECTION_H

#include <stdint.h>
#include <host/ble_gap.h>

/* ========================================================================
 * Type Definitions
 * ======================================================================== */

typedef enum {
    BLE_CONN_INIT = 0,
    BLE_CONN_STATE_CONNECTED,
    BLE_CONN_STATE_DISCONNECTED
} ble_conn_state_t;

typedef enum {
    MASTER = 0,
    SLAVE
} ble_conn_role_t;

typedef struct {
    ble_addr_t addr;
    ble_conn_role_t role;
    uint16_t conn_handle;
    ble_conn_state_t state;
    uint8_t in_use;
} ble_connection_t;

/* ========================================================================
 * Function Prototypes
 * ======================================================================== */

/**
 * @brief Initialize the connection management system
 */
void ble_conn_init(void);

/**
 * @brief Add a new connection to the list
 * @param addr BLE address
 * @param role Connection role (MASTER/SLAVE)
 * @return 0 on success, 1 if already present, -1 if list is full
 */
int ble_conn_add(const ble_addr_t *addr, ble_conn_role_t role);

/**
 * @brief Remove a connection from the list
 * @param addr BLE address to remove
 * @return 0 on success, -1 if not found
 */
int ble_conn_remove(const ble_addr_t *addr);

/**
 * @brief Update connection state
 * @param addr BLE address
 * @param conn_handle Connection handle
 * @param state New connection state
 * @return 0 on success, -1 if not found
 */
int ble_conn_update_state(const ble_addr_t *addr, uint16_t conn_handle, ble_conn_state_t state);

/**
 * @brief Get connection by address
 * @param addr BLE address
 * @return Pointer to connection or NULL if not found
 */
ble_connection_t *ble_conn_get_by_addr(const ble_addr_t *addr);

/**
 * @brief Get connection by handle
 * @param conn_handle Connection handle
 * @return Pointer to connection or NULL if not found
 */
ble_connection_t *ble_conn_get_by_handle(uint16_t conn_handle);

/**
 * @brief Check if there are unconnected devices of a specific role
 * @param role Role to check for
 * @return 1 if unconnected devices exist, 0 otherwise
 */
int ble_conn_has_unconnected_role(ble_conn_role_t role);

/**
 * @brief Print all connections
 */
void ble_conn_print_all(void);

/**
 * @brief Convert connection state to string
 * @param s Connection state
 * @return String representation
 */
const char *ble_conn_state_str(ble_conn_state_t s);

/**
 * @brief Convert role to string
 * @param r Connection role
 * @return String representation
 */
const char *ble_conn_role_str(ble_conn_role_t r);

#endif /* BLE_CONNECTION_H */
