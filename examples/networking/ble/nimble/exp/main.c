/* Include RIOT and NimBLE headers */
#include "random.h"
#include "periph/pm.h"
#include "periph/wdt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

/** Define custom service and characteristic UUIDs */
#define CUSTOM_SVC_UUID                     0xff00
#define CUSTOM_NOTIFY_CHR_UUID              0xee00
#define CUSTOM_WRITE_CHR_UUID               0xee01
/* Convert UUIDs to ble_uuid16_t */
ble_uuid16_t custom_svc_uuid = BLE_UUID16_INIT(CUSTOM_SVC_UUID);
ble_uuid16_t custom_notify_chr_uuid = BLE_UUID16_INIT(CUSTOM_NOTIFY_CHR_UUID);
ble_uuid16_t custom_write_chr_uuid = BLE_UUID16_INIT(CUSTOM_WRITE_CHR_UUID);
/* Define the designed connection interval and variance range (ms) */
/* Data exchange frequency is set according to the connection interval */
#define DESIGNED_CONNECTION_INTERVAL                (500)
#define VARIANCE_RANGE                              (150)
/* Define the supervision timeout (10 ms) */
#define SUPERVISION_TIMEOUT                         (500)
/* Define the maximum and expected numbers of connections */
#define BLE_MAX_CONNECTIONS                         (32)
#define EXPECTED_CONNECTIONS                        (10)
/* Define scanning and advertising period lower and upper bounds (us) */
#define SCAN_ADVERTISE_MIN_PERIOD                   (300)
#define SCAN_ADVERTISE_MAX_PERIOD                   (500)
/* Central: define custom write data parameters */
#define CUSTOM_WRITE_DATA_INIT						(0)
#define CUSTOM_WRITE_DATA_LIMIT						(100)
#define CUSTOM_WRITE_DATA_CHANGE_STEP				(1)
#define CUSTOM_WRITE_DATA_SIZE						(200)
/* Peripheral: define custom notify data parameters */
#define CUSTOM_NOTIFY_DATA_INIT						(0)
#define CUSTOM_NOTIFY_DATA_LIMIT					(100)
#define CUSTOM_NOTIFY_DATA_CHANGE_STEP				(1)
#define CUSTOM_NOTIFY_DATA_SIZE                     (200)

/* Define the stop flags */
#define STOP_WRITING_FLAG                           (1u << 0)
#define STOP_NOTIFYING_FLAG                         (1u << 1)

/* Define address type and device name */
uint8_t addr_type;
const char *device_name = "BLE_multihop_path";

/* Definitions related to discovery */
volatile int is_discovering = 0;

/* Definiations related to custom write */
volatile uint16_t conn_handle_list_for_write[EXPECTED_CONNECTIONS] = {0};
uint16_t custom_write_data_val_handle;
kernel_pid_t write_thread_pids[EXPECTED_CONNECTIONS];
char write_thread_stack[EXPECTED_CONNECTIONS][THREAD_STACKSIZE_DEFAULT];
char write_thread_names[EXPECTED_CONNECTIONS][15] = {
	"write_thread_00", 
	"write_thread_01", 
	"write_thread_02", 
	"write_thread_03", 
	"write_thread_04", 
	"write_thread_05", 
	"write_thread_06", 
	"write_thread_07", 
	"write_thread_08", 
	"write_thread_09", 
};
typedef struct {
    uint16_t conn_handle;
    kernel_pid_t pid;
    bool running; // This indicates if the thread is running or not
} write_thread_state_t;
write_thread_state_t write_thread_states[EXPECTED_CONNECTIONS];
mutex_t write_thread_mutex = MUTEX_INIT; // A mutual exclusion lock to protect shared data

/* Definiations related to custom notify */
volatile uint16_t conn_handle_list_for_notify[EXPECTED_CONNECTIONS] = {0};
uint16_t custom_notify_data_val_handle;
kernel_pid_t notify_thread_pids[EXPECTED_CONNECTIONS];
char notify_thread_stack[EXPECTED_CONNECTIONS][THREAD_STACKSIZE_DEFAULT];
char notify_thread_names[EXPECTED_CONNECTIONS][16] = {
	"notify_thread_00",
	"notify_thread_01",
	"notify_thread_02",
	"notify_thread_03",
	"notify_thread_06",
	"notify_thread_07",
	"notify_thread_08",
	"notify_thread_09", 
};
typedef struct {
    uint16_t conn_handle;
    kernel_pid_t pid;
    bool running; // This indicates if the thread is running or not
} notify_thread_state_t;
notify_thread_state_t notify_thread_states[EXPECTED_CONNECTIONS];
mutex_t notify_thread_mutex = MUTEX_INIT; // A mutual exclusion lock to protect shared data

/* Definiations related to scan and advertise */
kernel_pid_t scan_advertise_thread_pids[1];
char scan_advertise_thread_stack[1][THREAD_STACKSIZE_DEFAULT];
char scan_advertise_thread_names[1][24] = {
	"scan_advertise_thread_00", 
};

uint16_t add_conn_handle(uint16_t len_of_conn_handle_list, 
    volatile uint16_t conn_handle_list[], uint16_t conn_handle)
{
	for (uint16_t i = 0; i < len_of_conn_handle_list; i++) {
		if (conn_handle_list[i] == 0) {
			conn_handle_list[i] = conn_handle;
			return 0;
		}
	}

	return UINT16_MAX;
}

uint16_t delete_conn_handle(uint16_t len_of_conn_handle_list, 
    volatile uint16_t conn_handle_list[], uint16_t conn_handle)
{
	for (uint16_t i = 0; i < len_of_conn_handle_list; i++) {
		if (conn_handle_list[i] == conn_handle) {
			conn_handle_list[i] = 0;
			return 0;
		}
	}

	return UINT16_MAX;
}

uint16_t randomize_conn_interval(uint16_t existing_conn_intervals[], int size, 
    float designed_connection_interval, float variance_range) 
{
    uint16_t min_interval_in_unit = 
        (uint16_t)((designed_connection_interval * 1000 - variance_range * 1000) / 1250);
    uint16_t max_interval_in_unit = 
        (uint16_t)((designed_connection_interval * 1000 + variance_range * 1000) / 1250);

    // Check if all possible connection intervals are already in use
    uint16_t possible_values_count = max_interval_in_unit - min_interval_in_unit + 1;
    if (size >= possible_values_count) {
        printf("Error: All possible connection intervals are already in use\n");
        return 0;
    }

    uint16_t random_interval_in_unit;
    int not_satisfying = 1;

    while (not_satisfying == 1) {
        not_satisfying = 0;
        random_interval_in_unit = 
            random_uint32_range(min_interval_in_unit, max_interval_in_unit + 1);
        
        for (int i = 0; i < size; i++) {
            if (random_interval_in_unit == existing_conn_intervals[i]) {
                not_satisfying = 1;
                break;
            }
        }
    }

    return random_interval_in_unit;
}

int notify_access_cb(uint16_t conn_handle, uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ble_uuid_u16(ctxt->chr->uuid) != CUSTOM_NOTIFY_CHR_UUID) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

int write_access_cb(uint16_t conn_handle, uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ble_uuid_u16(ctxt->chr->uuid) != CUSTOM_WRITE_CHR_UUID) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* Custom Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(CUSTOM_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Custom Notify Characteristic */
            .uuid = BLE_UUID16_DECLARE(CUSTOM_NOTIFY_CHR_UUID),
            .access_cb = notify_access_cb,
            .val_handle = &custom_notify_data_val_handle,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        },  {
            /* Custom Write Characteristic */
            .uuid = BLE_UUID16_DECLARE(CUSTOM_WRITE_CHR_UUID),
            .access_cb = write_access_cb,
            .val_handle = &custom_write_data_val_handle,
            .flags = BLE_GATT_CHR_F_WRITE,
        },  {
            0, /* no more characteristics in this service */
        }, }
    },
    {
        0, /* no more services */
    },
};

void *write_thread(void *arg) {
    thread_flags_clear(STOP_WRITING_FLAG);
    uint16_t conn_handle = (uint16_t)(intptr_t)arg;
    uint8_t write_data = CUSTOM_WRITE_DATA_INIT;
    uint8_t custom_write_data[CUSTOM_WRITE_DATA_SIZE];
    float sleep_time_ms = (float)DESIGNED_CONNECTION_INTERVAL;
    ztimer_t write_timer;
    thread_flags_t flag_result;

    while (1) {
        /* 1. Check if connection is valid */
        struct ble_gap_conn_desc conn_desc;
        int rc = ble_gap_conn_find(conn_handle, &conn_desc);
        if (rc != 0) {
            printf("WARN: Write connection invalid (handle: %d), stopping thread\n", conn_handle);
            break; // Break out of the loop, and terminate the thread
        }

        /* 2. Update write data */
        write_data = (write_data + CUSTOM_WRITE_DATA_CHANGE_STEP) % CUSTOM_WRITE_DATA_LIMIT;
        for (uint8_t i = 0; i < sizeof(custom_write_data); i++) {
            custom_write_data[i] = write_data;
        }

        /* 3. Allocate mbuf */
        struct os_mbuf *om = ble_hs_mbuf_from_flat(custom_write_data, sizeof(custom_write_data));
        if (om == NULL) {
            printf("ERROR: Write mbuf allocation failed (handle: %d)\n", conn_handle);
            break; // Break out of the loop, and terminate the thread
        }

        /* 4. Write */
        rc = ble_gattc_write_no_rsp(conn_handle, custom_write_data_val_handle, om);
        if (rc != 0) {
            printf("ERROR: Write failed (handle: %d, rc: %d)\n", conn_handle, rc);
            os_mbuf_free_chain(om);
            break; // Break out of the loop, and terminate the thread
        }

        /* 5. Set a timer and sleep */
        sleep_time_ms = conn_desc.conn_itvl * 1.25f;
        ztimer_set_timeout_flag(ZTIMER_MSEC, &write_timer, (uint32_t)sleep_time_ms);
        flag_result = thread_flags_wait_any(STOP_WRITING_FLAG | THREAD_FLAG_TIMEOUT);

        if ((flag_result & STOP_WRITING_FLAG) != 0) {
            break; // Break out of the loop, and terminate the thread
        }
    }

    /* 6. Clean up the thread state */
    mutex_lock(&write_thread_mutex);
    for (int i = 0; i < EXPECTED_CONNECTIONS; i++) {
        if (write_thread_states[i].conn_handle == conn_handle) {
            write_thread_states[i].running = false;
            write_thread_states[i].conn_handle = 0;
            break;
        }
    }
    mutex_unlock(&write_thread_mutex);

    return NULL;
}

void start_writing(uint16_t conn_handle) {
    mutex_lock(&write_thread_mutex);

    // Check if the thread already exists
    for (int i = 0; i < EXPECTED_CONNECTIONS; i++) {
        if (write_thread_states[i].conn_handle == conn_handle && 
            write_thread_states[i].running) {
            mutex_unlock(&write_thread_mutex);
            return;
        }
    }

    // Find an available slot
    int loc = -1;
    for (int i = 0; i < EXPECTED_CONNECTIONS; i++) {
        if (write_thread_states[i].conn_handle == 0) {
            loc = i;
            break;
        }
    }

    if (loc == -1) {
        printf("ERROR: No slot available for conn_handle %d\n", conn_handle);
        mutex_unlock(&write_thread_mutex);
        return;
    }

    // Initialize thread state
    write_thread_states[loc].conn_handle = conn_handle;
    write_thread_states[loc].running = true;

    // Create thread
    write_thread_states[loc].pid = thread_create(
        write_thread_stack[loc], 
        sizeof(write_thread_stack[0]), 
        THREAD_PRIORITY_MAIN - 1, 
        THREAD_CREATE_STACKTEST, 
        write_thread, 
        (void *)(intptr_t)conn_handle, 
        "write_thread"
    );

    if (write_thread_states[loc].pid == KERNEL_PID_UNDEF) {
        printf("ERROR: Failed to create thread for handle %d\n", conn_handle);
        write_thread_states[loc].conn_handle = 0;
        write_thread_states[loc].running = false;
    }

    mutex_unlock(&write_thread_mutex);
}

void stop_writing(uint16_t conn_handle) {
    mutex_lock(&write_thread_mutex);

    for (int i = 0; i < EXPECTED_CONNECTIONS; i++) {
        if (write_thread_states[i].conn_handle == conn_handle) {
            // Send stop signal
            thread_flags_set(thread_get(write_thread_states[i].pid), STOP_WRITING_FLAG);
            write_thread_states[i].running = false;
            write_thread_states[i].conn_handle = 0;
            break;
        }
    }

    mutex_unlock(&write_thread_mutex);
}

static int discover_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
    uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)error;
    (void)chr_val_handle;
    uint16_t *ccc_handle = (uint16_t *)arg;

    if (dsc != NULL) {
        if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) == 0) {
            *ccc_handle = dsc->handle;
        }
    } else {
        // Manipulate the CCC descriptor to enable notifications
        if (*ccc_handle != 0) {
            uint8_t value[2] = {0x01, 0x00};
            int rc = ble_gattc_write_flat(conn_handle, *ccc_handle, value, sizeof(value), NULL, NULL);
            if (rc != 0) {
                printf("Failed to manipulate notify CCC, terminate connection\n");
                ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
        } else {
            printf("Failed to find notify CCC, terminate connection\n");
            ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    }

    return 0;
}

int discover_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
    const struct ble_gatt_chr *chr, void *arg)
{
    (void)error;
    uint16_t svc_end_handle = (uint16_t)(intptr_t)arg;

    if (ble_uuid_cmp(&chr->uuid.u, &custom_notify_chr_uuid.u) == 0) {
        static uint16_t ccc_handle = 0; // Use static to ensure discover_dsc_cb can access the variable
        ble_gattc_disc_all_dscs(conn_handle,
                                         chr->val_handle, 
                                         svc_end_handle, 
                                         discover_dsc_cb,
                                         &ccc_handle);
    }
    else if (ble_uuid_cmp(&chr->uuid.u, &custom_write_chr_uuid.u) == 0) {
        add_conn_handle(EXPECTED_CONNECTIONS, 
            conn_handle_list_for_write, 
            conn_handle);
        start_writing(conn_handle);
    }

    return 0;
}

int discover_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
    const struct ble_gatt_svc *service, void *arg)
{
    (void)arg;
    (void)error;

    if (service != NULL) {
        ble_gattc_disc_all_chrs(conn_handle, 
                    service->start_handle, 
                    service->end_handle, 
                    discover_chr_cb,
                    (void *)(intptr_t)service->end_handle);
    }

    return 0;
}

/* connection has separate event handler from scan */
int central_conn_event(struct ble_gap_event *event, void *arg)
{
	(void)arg;

	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ble_gap_set_prefered_le_phy(event->connect.conn_handle, 
                BLE_GAP_LE_PHY_1M_MASK, 
                BLE_GAP_LE_PHY_1M_MASK, 
                BLE_GAP_LE_PHY_CODED_ANY);
            ble_gap_set_data_len(event->connect.conn_handle, 251, 17040);
            /* Perform service discovery */
            is_discovering = 1;
            int rc = ble_gattc_disc_svc_by_uuid(event->connect.conn_handle, 
                                                &custom_svc_uuid.u, 
                                                discover_svc_cb, NULL);
            if (rc != 0) {
                ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
            is_discovering = 0;
        }
        return 0;
	case BLE_GAP_EVENT_DISCONNECT:
		printf("Disconnected, central reason code: %i\n", event->disconnect.reason);
        delete_conn_handle(
            EXPECTED_CONNECTIONS, 
            conn_handle_list_for_write, 
            event->disconnect.conn.conn_handle);
        stop_writing(event->disconnect.conn.conn_handle);
		return 0;
	case BLE_GAP_EVENT_NOTIFY_RX:
		return 0;
	}
	return 0;
}

int central_scan_event(struct ble_gap_event *event, void *arg)
{
	(void)arg;

    int uuid_cmp_result;
	struct ble_hs_adv_fields parsed_fields;
	memset(&parsed_fields, 0, sizeof(parsed_fields));

    struct ble_gap_conn_desc conn_desc;

	switch (event->type) {
	case BLE_GAP_EVENT_DISC:
        if (ble_gap_conn_find_by_addr(&event->disc.addr, &conn_desc) == 0) {
            return 0;  // Already connected
        }
		ble_hs_adv_parse_fields(&parsed_fields, event->disc.data,
								event->disc.length_data);
		/* Predefined UUID is compared to recieved one;
		   if doesn't fit - end procedure and go back to scanning,
		   else - connect */
		uuid_cmp_result = ble_uuid_cmp(&custom_svc_uuid.u, &parsed_fields.uuids16->u);
		if (uuid_cmp_result == 0) {
            ble_gap_disc_cancel();
			ble_gap_connect(addr_type, &(event->disc.addr), 100,
							NULL, central_conn_event, NULL);
		}
		return 0;
	case BLE_GAP_EVENT_DISC_COMPLETE:
		return 0;
	}
	return 0;
}

void scan(void)
{
	int rc;

	/* Set scan parameters, and perform discovery */
	const struct ble_gap_disc_params scan_params = {10000, 200, 0, 0, 0, 1};
	rc = ble_gap_disc(addr_type, 100, &scan_params, central_scan_event, NULL);
	if (rc != 0) {
		printf("ble_gap_disc(): %d\n", rc);
        pm_reboot();
	}
}

void *notify_thread(void *arg) {
    thread_flags_clear(STOP_NOTIFYING_FLAG);
    uint16_t conn_handle = (uint16_t)(intptr_t)arg;
    uint8_t notify_data = CUSTOM_NOTIFY_DATA_INIT;
    uint8_t custom_notify_data[CUSTOM_NOTIFY_DATA_SIZE];
    float sleep_time_ms = (float)DESIGNED_CONNECTION_INTERVAL;
    ztimer_t notify_timer;
    thread_flags_t flag_result;

    while (1) {
        /* 1. Check if connection is valid */
        struct ble_gap_conn_desc conn_desc;
        int rc = ble_gap_conn_find(conn_handle, &conn_desc);
        if (rc != 0) {
            printf("WARN: Notify connection invalid (handle: %d), stopping thread\n", conn_handle);
            break; // Break out of the loop, and terminate the thread
        }

        /* 2. Update notify data */
        notify_data = (notify_data + CUSTOM_NOTIFY_DATA_CHANGE_STEP) % CUSTOM_NOTIFY_DATA_LIMIT;
        for (uint8_t i = 0; i < sizeof(custom_notify_data); i++) {
            custom_notify_data[i] = notify_data;
        }

        /* 3. Allocate mbuf */
        struct os_mbuf *om = ble_hs_mbuf_from_flat(custom_notify_data, sizeof(custom_notify_data));
        if (om == NULL) {
            printf("ERROR: Notify mbuf allocation failed (handle: %d)\n", conn_handle);
            break; // Break out of the loop, and terminate the thread
        }

        /* 4. Notify */
        rc = ble_gatts_notify_custom(conn_handle, custom_notify_data_val_handle, om);
        if (rc != 0) {
            printf("ERROR: Notify failed (handle: %d, rc: %d)\n", conn_handle, rc);
            os_mbuf_free_chain(om);
            break; // Break out of the loop, and terminate the thread
        }

        /* 5. Set a timer and sleep */
        sleep_time_ms = conn_desc.conn_itvl * 1.25f;
        ztimer_set_timeout_flag(ZTIMER_MSEC, &notify_timer, (uint32_t)sleep_time_ms);
        flag_result = thread_flags_wait_any(STOP_NOTIFYING_FLAG | THREAD_FLAG_TIMEOUT);

        if ((flag_result & STOP_NOTIFYING_FLAG) != 0) {
            break; // Break out of the loop, and terminate the thread
        }
    }

    /* 6. Clean up the thread state */
    mutex_lock(&notify_thread_mutex);
    for (int i = 0; i < EXPECTED_CONNECTIONS; i++) {
        if (notify_thread_states[i].conn_handle == conn_handle) {
            notify_thread_states[i].running = false;
            notify_thread_states[i].conn_handle = 0;
            break;
        }
    }
    mutex_unlock(&notify_thread_mutex);

    return NULL;
}

void start_notifying(uint16_t conn_handle) {
    mutex_lock(&notify_thread_mutex);

    // Check if the thread already exists
    for (int i = 0; i < EXPECTED_CONNECTIONS; i++) {
        if (notify_thread_states[i].conn_handle == conn_handle && 
            notify_thread_states[i].running) {
            mutex_unlock(&notify_thread_mutex);
            return;
        }
    }

    // Find an available slot
    int loc = -1;
    for (int i = 0; i < EXPECTED_CONNECTIONS; i++) {
        if (notify_thread_states[i].conn_handle == 0) {
            loc = i;
            break;
        }
    }

    if (loc == -1) {
        printf("ERROR: No slot available for conn_handle %d\n", conn_handle);
        mutex_unlock(&notify_thread_mutex);
        return;
    }

    // Initialize thread state
    notify_thread_states[loc].conn_handle = conn_handle;
    notify_thread_states[loc].running = true;

    // Create thread
    notify_thread_states[loc].pid = thread_create(
        notify_thread_stack[loc], 
        sizeof(notify_thread_stack[0]), 
        THREAD_PRIORITY_MAIN - 1, 
        THREAD_CREATE_STACKTEST, 
        notify_thread, 
        (void *)(intptr_t)conn_handle, 
        "notify_thread"
    );

    if (notify_thread_states[loc].pid == KERNEL_PID_UNDEF) {
        printf("ERROR: Failed to create notify thread for handle %d\n", conn_handle);
        notify_thread_states[loc].conn_handle = 0;
        notify_thread_states[loc].running = false;
    }

    mutex_unlock(&notify_thread_mutex);
}

void stop_notifying(uint16_t conn_handle) {
    mutex_lock(&notify_thread_mutex);

    for (int i = 0; i < EXPECTED_CONNECTIONS; i++) {
        if (notify_thread_states[i].conn_handle == conn_handle) {
            // Send stop signal
            thread_flags_set(thread_get(notify_thread_states[i].pid), STOP_NOTIFYING_FLAG);
            notify_thread_states[i].running = false;
            notify_thread_states[i].conn_handle = 0;
            break;
        }
    }

    mutex_unlock(&notify_thread_mutex);
}

int peripheral_conn_event(struct ble_gap_event *event, void *arg)
{
	(void)arg;

	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ble_gap_set_prefered_le_phy(event->connect.conn_handle, 
                BLE_GAP_LE_PHY_1M_MASK, 
                BLE_GAP_LE_PHY_1M_MASK, 
                BLE_GAP_LE_PHY_CODED_ANY);
            ble_gap_set_data_len(event->connect.conn_handle, 251, 17040);
        }
        return 0;
	case BLE_GAP_EVENT_DISCONNECT:
		printf("Disconnected, peripheral reason code: %d\n", event->disconnect.reason);
		/* Normally before disconnection, 
		the subscription should be automatically stopped.
		As a reuslt, conn_handle_list_for_notify should have been updated. 
		However, in case of specicial cases, we do a double check below */
        delete_conn_handle(EXPECTED_CONNECTIONS, 
            conn_handle_list_for_notify, 
            event->disconnect.conn.conn_handle);
        stop_notifying(event->disconnect.conn.conn_handle);
		return 0;
	case BLE_GAP_EVENT_SUBSCRIBE:
		if (event->subscribe.attr_handle == custom_notify_data_val_handle) {
			if (event->subscribe.cur_notify == 1) {
                add_conn_handle(EXPECTED_CONNECTIONS, 
					conn_handle_list_for_notify, 
					event->subscribe.conn_handle);
				start_notifying(event->subscribe.conn_handle);
			} else {
				delete_conn_handle(EXPECTED_CONNECTIONS, 
					conn_handle_list_for_notify, 
					event->subscribe.conn_handle);
				stop_notifying(event->subscribe.conn_handle);
			}
		}
		return 0;
	}
	return 0;
}

void advertise(void)
{
	int rc;

	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;

	memset(&adv_params, 0, sizeof(adv_params));
	memset(&fields, 0, sizeof(fields));

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

	fields.flags = BLE_HS_ADV_F_DISC_GEN;
    fields.name = (uint8_t *)device_name;
	fields.name_len = strlen(device_name);
	fields.name_is_complete = 1;

	fields.uuids16 = (ble_uuid16_t[]) {custom_svc_uuid};
	fields.num_uuids16 = 1;
	fields.uuids16_is_complete = 0;

	ble_gap_adv_set_fields(&fields);
	rc = ble_gap_adv_start(addr_type, NULL, 100, 
						   &adv_params, peripheral_conn_event, NULL);
	if(rc != 0) {
		printf("ble_gap_adv_start() rc = %d\n", rc);
        pm_reboot();
	}
}

void *scan_advertise_thread(void *arg)
{
	(void)arg;
	int lower = SCAN_ADVERTISE_MIN_PERIOD;
	int upper = SCAN_ADVERTISE_MAX_PERIOD;
    int period;

	while (1) {
        period = random_uint32_range(lower, upper + 1);
        if (is_discovering == 0) {
            if (ble_gap_disc_active() == 0 && ble_gap_adv_active() == 0) {
                scan();
                ztimer_sleep(ZTIMER_MSEC, period);
                ble_gap_disc_cancel();
            }
            if (ble_gap_disc_active() == 0 && ble_gap_adv_active() == 0) {
                advertise();
                ztimer_sleep(ZTIMER_MSEC, period);
                ble_gap_adv_stop();
            }
        }
        ztimer_sleep(ZTIMER_MSEC, period);
	}

    return NULL;
}

int main(void)
{
    int rc;

    /* Sleep for 1 sec to wait for the terminal to be ready for output */
	ztimer_init();
	ztimer_sleep(ZTIMER_MSEC, 1000);

    /* Update BLE connection MTU */
    rc = ble_att_set_preferred_mtu(BLE_ATT_MTU_MAX);
    if (rc != 0) {
        printf("ble_att_set_preferred_mtu() rc = %d\n", rc);
        return -1;
    }

	/* Verify and add our custom services */
	rc = ble_svc_gap_device_name_set(device_name);
    if (rc != 0) {
        printf("ble_svc_gap_device_name_set() rc = %d\n", rc);
        return -1;
    }
	rc = ble_gatts_count_cfg(gatt_svr_svcs);
	if (rc != 0) {
        printf("ble_gatts_count_cfg() rc = %d\n", rc);
        return -1;
    }
	rc = ble_gatts_add_svcs(gatt_svr_svcs);
	if (rc != 0) {
        printf("ble_gatts_add_svcs() rc = %d\n", rc);
        return -1;
    }

	/* Reload the GATT server to link our added services */
	rc = ble_gatts_start();
	if (rc != 0) {
        printf("ble_gatts_start() rc = %d\n", rc);
        return -1;
    }

    /* addr_type will store type of address we use */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &addr_type);
    assert(rc == 0);

    /* Start scanning and advertising thread */
	scan_advertise_thread_pids[0] = 
	thread_create(
		scan_advertise_thread_stack[0], 
		sizeof(scan_advertise_thread_stack[0]), 
		THREAD_PRIORITY_MAIN - 1, 
		THREAD_CREATE_STACKTEST, 
		scan_advertise_thread, 
		NULL, 
		scan_advertise_thread_names[0]);
    
    wdt_setup_reboot(0, 10 * 1000);
    wdt_start();

	return 0;
}
