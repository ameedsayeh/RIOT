# name of your application
APPLICATION = ble_multihop

# If no BOARD is found in the environment, use this default:
BOARD ?= nrf52dk
UF2_SOFTDEV = DROP

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../..

# configure and use Nimble
USEMODULE += random
USEMODULE += nimble_svc_gap
# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:

# CFLAGS += -DMYNEWT_VAL_MSYS_1_BLOCK_COUNT=256
# CFLAGS += -DMYNEWT_VAL_MSYS_1_BLOCK_SIZE=256
# CFLAGS += -DMYNEWT_VAL_BLE_LL_CFG_FEAT_DATA_LEN_EXT=1
# CFLAGS += -DMYNEWT_VAL_BLE_LL_MAX_PKT_SIZE=256
# CFLAGS += -DMYNEWT_VAL_BLE_L2CAP_COC_MPS=256
# CFLAGS += -DMYNEWT_VAL_BLE_HCI_EVT_BUF_SIZE=256
# CFLAGS += -DMYNEWT_VAL_BLE_LL_CFG_FEAT_LE_CSA2=1
# CFLAGS += -DMYNEWT_VAL_BLE_L2CAP_COC_MAX_NUM=10
CFLAGS += -DMYNEWT_VAL_BLE_MAX_CONNECTIONS=10
# CFLAGS += -DMYNEWT_VAL_BLE_HS_CONN_MAX=10

DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
