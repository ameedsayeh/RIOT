
#include <stdio.h>
#include <string.h>
#include "ztimer.h"
// NimBLE
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>
#include "host/util/util.h"

#define BLE_SVC1_UUID 0xabcd
#define BLE_SVC2_UUID 0xdcba

const char *device_name = "Ameed BLE";
uint8_t address;

void wait_for_terminal(void)
{
    ztimer_sleep(ZTIMER_MSEC, 5000);
    printf("Device is ready!\n");
}

void set_gap_device_name(void)
{
    // Setting GAP device name
    printf("Setting device name to: %s\n", device_name);
    int rc = ble_svc_gap_device_name_set(device_name);
    if (rc != 0) {
        printf("Error setting device name: %d\n", rc);
    }
    assert(rc == 0);
}

void prepare_address(void)
{
    // Generate address if not existing
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        printf("Error ensuring address: %d\n", rc);
    }
    assert(rc == 0);

    // Store the address in address
    rc = ble_hs_id_infer_auto(0, &address);
    assert(rc == 0);
}

// Advertise
void advertise(void);

int advertise_callback(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        break;
    case BLE_GAP_EVENT_CONNECT:
        printf("Connected\n");
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        printf("Disconnected\n");
        advertise();
        break;
    }

    return 0;
}

void advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    // Clear the adv_params and fields
    memset(&adv_params, 0, sizeof(adv_params));
    memset(&fields, 0, sizeof(fields));

    // Settings connection mode and discovery mode
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    fields.flags = BLE_HS_ADV_F_DISC_GEN;
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    // Setting SVCs IDs
    fields.uuids16 = (ble_uuid16_t[]){
        BLE_UUID16_INIT(BLE_SVC1_UUID),
        BLE_UUID16_INIT(BLE_SVC2_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        printf("Error setting advertisement data: %d\n", rc);
    }
    assert(rc == 0);

    rc = ble_gap_adv_start(address, NULL, 100, &adv_params, advertise_callback, NULL);
    if (rc != 0) {
        printf("Error starting advertising: %d\n", rc);
    }
    assert(rc == 0);
}

int main(void)
{
    wait_for_terminal();
    set_gap_device_name();
    prepare_address();

    // Start advertising
    advertise();

    return 0;
}
