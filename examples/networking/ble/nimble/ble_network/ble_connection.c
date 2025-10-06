/**
 * @file ble_connection.c
 * @brief BLE connection management implementation
 */

#include "ble_connection.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include "mutex.h"
#include <host/ble_hs.h>

/* ========================================================================
 * Global Variables
 * ======================================================================== */

static ble_connection_t _conns[MAX_CONNECTIONS];
static mutex_t _conns_lock = MUTEX_INIT;

/* ========================================================================
 * Private Functions
 * ======================================================================== */

/**
 * @brief Compare two BLE addresses for equality
 * @param a First address
 * @param b Second address
 * @return 1 if equal, 0 if different
 */
static int _ble_addr_equal(const ble_addr_t *a, const ble_addr_t *b)
{
    return (memcmp(a->val, b->val, 6) == 0);
}

/**
 * @brief Print BLE address
 * @param a Address to print
 */
static void _print_addr(const ble_addr_t *a)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X", a->val[5], a->val[4], a->val[3],
           a->val[2], a->val[1], a->val[0]);
}

/* ========================================================================
 * Public Functions
 * ======================================================================== */

void ble_conn_init(void)
{
    mutex_lock(&_conns_lock);
    memset(_conns, 0, sizeof(_conns));
    mutex_unlock(&_conns_lock);
}

const char *ble_conn_state_str(ble_conn_state_t s)
{
    switch (s) {
    case BLE_CONN_INIT:
        return "init";
    case BLE_CONN_STATE_CONNECTED:
        return "connected";
    case BLE_CONN_STATE_DISCONNECTED:
        return "disconnected";
    default:
        return "?";
    }
}

const char *ble_conn_role_str(ble_conn_role_t r)
{
    return (r == MASTER) ? "master" : "slave";
}

int ble_conn_add(const ble_addr_t *addr, ble_conn_role_t role)
{
    mutex_lock(&_conns_lock);

    /* Check for duplicates */
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _ble_addr_equal(&_conns[i].addr, addr)) {
            mutex_unlock(&_conns_lock);
            return 1; /* already present */
        }
    }

    /* Find free slot */
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!_conns[i].in_use) {
            _conns[i].addr = *addr;
            _conns[i].role = role;
            _conns[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
            _conns[i].state = BLE_CONN_INIT;
            _conns[i].in_use = 1;
            mutex_unlock(&_conns_lock);
            return 0; /* success */
        }
    }

    mutex_unlock(&_conns_lock);
    return -1; /* full */
}

int ble_conn_remove(const ble_addr_t *addr)
{
    int ret = -1;
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _ble_addr_equal(&_conns[i].addr, addr)) {
            _conns[i].in_use = 0;
            _conns[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
            _conns[i].state = BLE_CONN_STATE_DISCONNECTED;
            ret = 0;
            break;
        }
    }
    mutex_unlock(&_conns_lock);
    return ret;
}

int ble_conn_update_state(const ble_addr_t *addr, uint16_t conn_handle, ble_conn_state_t state)
{
    int ret = -1;
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _ble_addr_equal(&_conns[i].addr, addr)) {
            _conns[i].conn_handle = conn_handle;
            _conns[i].state = state;
            ret = 0;
            break;
        }
    }
    mutex_unlock(&_conns_lock);
    return ret;
}

ble_connection_t *ble_conn_get_by_addr(const ble_addr_t *addr)
{
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _ble_addr_equal(&_conns[i].addr, addr)) {
            mutex_unlock(&_conns_lock);
            return &_conns[i];
        }
    }
    mutex_unlock(&_conns_lock);
    return NULL;
}

ble_connection_t *ble_conn_get_by_handle(uint16_t conn_handle)
{
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _conns[i].conn_handle == conn_handle) {
            mutex_unlock(&_conns_lock);
            return &_conns[i];
        }
    }
    mutex_unlock(&_conns_lock);
    return NULL;
}

int ble_conn_has_unconnected_role(ble_conn_role_t role)
{
    int has_unconnected = 0;
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _conns[i].role == role &&
            _conns[i].state != BLE_CONN_STATE_CONNECTED) {
            has_unconnected = 1;
            break;
        }
    }
    mutex_unlock(&_conns_lock);
    return has_unconnected;
}

void ble_conn_print_all(void)
{
    int count = 0;
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use) {
            printf("%d: ", i);
            _print_addr(&_conns[i].addr);
            printf(" role=%s state=%s handle=%u\n",
                   ble_conn_role_str(_conns[i].role),
                   ble_conn_state_str(_conns[i].state),
                   (unsigned)_conns[i].conn_handle);
            count++;
        }
    }
    mutex_unlock(&_conns_lock);
    if (count == 0) {
        puts("empty");
    }
}
