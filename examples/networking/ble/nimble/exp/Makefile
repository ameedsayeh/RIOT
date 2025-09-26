# name of your application
APPLICATION = ble_exp

# If no BOARD is found in the environment, use this default:
BOARD ?= nrf52dk
UF2_SOFTDEV = DROP

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../..

USEMODULE += random
USEMODULE += ztimer_msec
USEMODULE += event_timeout_ztimer
USEMODULE += printf_float
USEMODULE += periph_wdt
USEMODULE += nimble_svc_gap
USEMODULE += nimble_svc_gatt
USEMODULE += nimble_phy_2mbit
USEMODULE += nimble_phy_coded

CFLAGS += -DMYNEWT_VAL_MSYS_1_BLOCK_COUNT=256
CFLAGS += -DMYNEWT_VAL_MSYS_1_BLOCK_SIZE=256
CFLAGS += -DMYNEWT_VAL_BLE_LL_CFG_FEAT_DATA_LEN_EXT=1
CFLAGS += -DMYNEWT_VAL_BLE_LL_MAX_PKT_SIZE=256
CFLAGS += -DMYNEWT_VAL_BLE_L2CAP_COC_MPS=256
CFLAGS += -DMYNEWT_VAL_BLE_HCI_EVT_BUF_SIZE=256
CFLAGS += -DMYNEWT_VAL_BLE_LL_CFG_FEAT_LE_CSA2=1
CFLAGS += -DMYNEWT_VAL_BLE_L2CAP_COC_MAX_NUM=10
CFLAGS += -DMYNEWT_VAL_BLE_MAX_CONNECTIONS=10
CFLAGS += -DMYNEWT_VAL_BLE_HS_CONN_MAX=10

DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
