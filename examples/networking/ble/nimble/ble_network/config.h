/**
 * @file config.h
 * @brief Configuration constants and device management
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <host/ble_gap.h>

/* ========================================================================
 * Configuration Constants
 * ======================================================================== */

/* Thread and queue configuration */
#define MAIN_QUEUE_SIZE (8)
#define MAX_CONNECTIONS (10)

/* BLE timing parameters (in milliseconds) */
#define ADV_ITVL_MS     (90U)  /* Advertisement interval */
#define SCN_WIN_MS      (100U) /* Scan window */
#define CONN_ITVL       (75U)  /* Connection interval */
#define CONN_TIMEOUT_MS (600U) /* Connection timeout */

/* Note: BLE service and characteristic UUIDs are defined in gatt_services.h */

/* ========================================================================
 * Device Configuration Structure
 * ======================================================================== */

typedef struct {
    const char *mac;
    const char *role;
} Device;

/* ========================================================================
 * Function Prototypes
 * ======================================================================== */

/**
 * @brief Parse MAC address string into ble_addr_t structure
 * @param s MAC address string in format "AA:BB:CC:DD:EE:FF"
 * @param out Parsed address structure
 * @return 0 on success, -1 on error
 */
int parse_mac_address(const char *s, ble_addr_t *out);

/**
 * @brief Print BLE address from bytes
 * @param addr Address bytes to print
 */
void print_addr_from_bytes(const uint8_t *addr);

/**
 * @brief Show device's own BLE address
 */
void show_own_address(void);

/**
 * @brief Initialize BLE stack and configure device settings
 */
void init_ble(void);

/**
 * @brief Load predefined connections from device list
 */
void load_connections(void);

/**
 * @brief Get device name
 * @return Device name string
 */
const char *get_device_name(void);

/**
 * @brief Get address type
 * @return Address type
 */
uint8_t get_addr_type(void);

/**
 * @brief Initialize GATT services (declared here for config module use)
 * @return 0 on success, error code otherwise
 */
int gatt_services_init(void);

#endif /* CONFIG_H */
