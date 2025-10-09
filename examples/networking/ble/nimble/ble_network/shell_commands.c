/**
 * @file shell_commands.c
 * @brief Shell command implementation for BLE network management
 */

#include "shell_commands.h"
#include "ble_connection.h"
#include "config.h"
#include "gatt_services.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <host/ble_gap.h>

/* ========================================================================
 * Private Functions
 * ======================================================================== */

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
 * Shell Command Functions
 * ======================================================================== */

int cmd_addm(int argc, char **argv)
{
    if (argc != 2) {
        puts("usage: addm <AA:BB:CC:DD:EE:FF>");
        return 1;
    }
    ble_addr_t addr;
    if (parse_mac_address(argv[1], &addr) != 0) {
        puts("error: invalid address format");
        return 1;
    }
    int rc = ble_conn_add(&addr, MASTER);
    if (rc == 0) {
        printf("added (master role) ");
        _print_addr(&addr);
        puts("");
        return 0;
    }
    if (rc == 1) {
        puts("already present");
        return 0;
    }
    puts("error: list full");
    return 1;
}

int cmd_adds(int argc, char **argv)
{
    if (argc != 2) {
        puts("usage: adds <AA:BB:CC:DD:EE:FF>");
        return 1;
    }
    ble_addr_t addr;
    if (parse_mac_address(argv[1], &addr) != 0) {
        puts("error: invalid address format");
        return 1;
    }
    int rc = ble_conn_add(&addr, SLAVE);
    if (rc == 0) {
        printf("added (slave role) ");
        _print_addr(&addr);
        puts("");
        return 0;
    }
    if (rc == 1) {
        puts("already present");
        return 0;
    }
    puts("error: list full");
    return 1;
}

int cmd_rm(int argc, char **argv)
{
    if (argc != 2) {
        puts("usage: rm <AA:BB:CC:DD:EE:FF>");
        return 1;
    }
    ble_addr_t addr;
    if (parse_mac_address(argv[1], &addr) != 0) {
        puts("error: invalid address format");
        return 1;
    }
    if (ble_conn_remove(&addr) == 0) {
        printf("removed ");
        _print_addr(&addr);
        puts("");
        return 0;
    }
    puts("not found");
    return 1;
}

int cmd_list(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        puts("usage: list");
        return 1;
    }
    ble_conn_print_all();
    return 0;
}

int cmd_sync(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: sync <handle>");
        puts("  handle: connection handle number");
        return 1;
    }

    uint16_t conn_handle = (uint16_t)atoi(argv[1]);
    int rc = start_sync(conn_handle);
    if (rc == 0) {
        // printf("Sync started for handle %d\n", conn_handle);
    }
    else {
        printf("Failed to start sync for handle %d\n", conn_handle);
    }

    return (rc == 0) ? 0 : 1;
}

int cmd_update(int argc, char **argv)
{
    if (argc != 3) {
        puts("usage: update <conn_handle> <conn_interval>");
        puts("  conn_handle: connection handle number");
        puts("  conn_interval: connection interval in units of 1.25ms (min: 6, max: 3200)");
        return 1;
    }

    uint16_t conn_handle = (uint16_t)atoi(argv[1]);
    uint16_t conn_interval = (uint16_t)atoi(argv[2]);

    /* Validate connection interval (6-3200 units of 1.25ms) */
    if (conn_interval < 6 || conn_interval > 3200) {
        printf("Error: connection interval must be between 6 and 3200 (7.5ms to 4000ms)\n");
        return 1;
    }

    /* Set up connection parameters */
    struct ble_gap_upd_params params = {
        .itvl_min = BLE_GAP_CONN_ITVL_MS(conn_interval),
        .itvl_max = BLE_GAP_CONN_ITVL_MS(conn_interval),
        .latency = 0,
        .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(20 * conn_interval),
        .min_ce_len = 0,
        .max_ce_len = 0,
    };

    printf("Updating connection %d to interval %d\n",
           conn_handle, conn_interval);

    int rc = ble_gap_update_params(conn_handle, &params);
    if (rc == 0) {
        printf("Connection parameter update initiated successfully\n");
    }
    else {
        printf("Failed to update connection parameters: %d\n", rc);
    }

    return (rc == 0) ? 0 : 1;
}

/* ========================================================================
 * Shell Commands Array
 * ======================================================================== */

static const shell_command_t shell_commands[] = {
    { "addm", "Add master BLE address", cmd_addm },
    { "adds", "Add slave BLE address", cmd_adds },
    { "rm", "Remove BLE address", cmd_rm },
    { "list", "List connections", cmd_list },
    { "sync", "Start time synchronization with handle", cmd_sync },
    { "update", "Update connection parameters", cmd_update },
    { NULL, NULL, NULL }
};

const shell_command_t *get_shell_commands(void)
{
    return shell_commands;
}
