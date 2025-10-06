/**
 * @file config.c
 * @brief Configuration and utility functions implementation
 */

#include "config.h"
#include "ble_connection.h"
#include "gatt_services.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

/* ========================================================================
 * Device Configuration
 * ======================================================================== */

#ifdef DEVICE_LIST
#  define X(mac, role) { mac, role },
static const Device devices[] = { DEVICE_LIST };
#  undef X
#else
static const Device devices[] = {}; /* empty */
#endif

static const size_t devices_count = sizeof(devices) / sizeof(devices[0]);

/* BLE configuration */
static uint8_t addr_type;
static const char *device_name = "BLE_Device";

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

int parse_mac_address(const char *s, ble_addr_t *out)
{
    unsigned int b[6];
    if (sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x", &b[0], &b[1], &b[2], &b[3], &b[4],
               &b[5]) != 6) {
        return -1;
    }
    /* BLE convention: bytes in reverse order */
    for (int i = 0; i < 6; i++) {
        out->val[5 - i] = (uint8_t)b[i];
    }
    out->type = BLE_ADDR_PUBLIC;
    return 0;
}

void print_addr_from_bytes(const uint8_t *addr)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X",
           addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

void show_own_address(void)
{
    uint8_t addr[6];
    int rc;

    rc = ble_hs_id_copy_addr(BLE_ADDR_RANDOM, addr, NULL);
    if (rc == 0) {
        printf("Self address: ");
        print_addr_from_bytes(addr);
        printf("\n");
    }
}

void init_ble(void)
{
    /* Setting GAP device name */
    printf("Setting device name to: %s\n", device_name);
    int rc = ble_svc_gap_device_name_set(device_name);
    if (rc != 0) {
        printf("Error setting device name: %d\n", rc);
    }
    assert(rc == 0);

    /* Initialize GATT services */
    rc = gatt_services_init();
    if (rc != 0) {
        printf("GATT services initialization failed: %d\n", rc);
        return;
    }

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &addr_type);
    assert(rc == 0);

    show_own_address();
}

void load_connections(void)
{
    printf("Found %zu devices:\n", devices_count);
    for (size_t i = 0; i < devices_count; i++) {
        printf("  [%zu] MAC: %s, Role: %s\n",
               i + 1, devices[i].mac, devices[i].role);

        ble_addr_t addr;
        if (parse_mac_address(devices[i].mac, &addr) == 0) {
            ble_conn_role_t role = (strcmp(devices[i].role, "Master") == 0) ? MASTER : SLAVE;
            ble_conn_add(&addr, role);
        }
    }
}

const char *get_device_name(void)
{
    return device_name;
}

uint8_t get_addr_type(void)
{
    return addr_type;
}
