# Set HAVE_WOLFSSL or HAVE_MBEDTLS variable if you want to use WOLFSSL or
# MBEDTLS instead of LIBTOMCRYPT

SRC_DIR := src

OBJECTS_O = $(SRC_DIR)/onvif_simple_server.o \
	$(SRC_DIR)/device_service.o $(SRC_DIR)/media_service.o $(SRC_DIR)/media2_service.o \
	$(SRC_DIR)/ptz_service.o $(SRC_DIR)/events_service.o $(SRC_DIR)/deviceio_service.o \
	$(SRC_DIR)/fault.o $(SRC_DIR)/conf.o $(SRC_DIR)/utils_zlib.o $(SRC_DIR)/log.o \
	$(SRC_DIR)/ezxml_wrapper.o ezxml/ezxml.o
OBJECTS_N = $(SRC_DIR)/onvif_notify_server.o $(SRC_DIR)/conf.o $(SRC_DIR)/utils.o \
	$(SRC_DIR)/log.o $(SRC_DIR)/ezxml_wrapper.o ezxml/ezxml.o
OBJECTS_W = $(SRC_DIR)/wsd_simple_server.o $(SRC_DIR)/utils.o $(SRC_DIR)/log.o \
	$(SRC_DIR)/ezxml_wrapper.o ezxml/ezxml.o

ifdef HAVE_WOLFSSL
INCLUDE = -DHAVE_WOLFSSL -ffunction-sections -fdata-sections
LIBS_O = -Wl,--gc-sections -lwolfssl $(LIBS_Z) -lcjson -lpthread -lrt
LIBS_N = -Wl,--gc-sections -lwolfssl -lcjson -lpthread -lrt
else
ifdef HAVE_MBEDTLS
INCLUDE = -DHAVE_MBEDTLS -ffunction-sections -fdata-sections
LIBS_O = -Wl,--gc-sections -lmbedcrypto $(LIBS_Z) -lcjson -lpthread -lrt
LIBS_N = -Wl,--gc-sections -lmbedcrypto -lcjson -lpthread -lrt
else
INCLUDE = -ffunction-sections -fdata-sections
LIBS_O = -Wl,--gc-sections -ltomcrypt $(LIBS_Z) -lcjson -lpthread -lrt
LIBS_N = -Wl,--gc-sections -ltomcrypt -lcjson -lpthread -lrt
endif
endif
LIBS_W = -Wl,--gc-sections -lpthread

# Ensure headers under src/ and ezxml under project root are found
INCLUDE += -I$(SRC_DIR) -I.

ifdef USE_ZLIB
DUSE_ZLIB = -DUSE_ZLIB
LIBS_Z = -lz
else
DUSE_ZLIB =
LIBS_Z =
endif

ifeq ($(STRIP), )
    STRIP=echo
endif

all: onvif_simple_server onvif_notify_server wsd_simple_server

$(SRC_DIR)/log.o: $(SRC_DIR)/log.c $(HEADERS)
	$(CC) -c $< -std=c99 -fPIC -Os $(INCLUDE) -o $@

$(SRC_DIR)/utils_zlib.o: $(SRC_DIR)/utils.c $(HEADERS)
	$(CC) -c $< $(DUSE_ZLIB) -fPIC -Os $(INCLUDE) -o $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) -c $< -fPIC -Os $(INCLUDE) -o $@

ezxml/%.o: ezxml/%.c
	$(CC) -c $< -fPIC -Os $(INCLUDE) -o $@

onvif_simple_server: $(OBJECTS_O)
	$(CC) $(OBJECTS_O) $(LIBS_O) -fPIC -Os -o $@
	$(STRIP) $@

onvif_notify_server: $(OBJECTS_N)
	$(CC) $(OBJECTS_N) $(LIBS_N) -fPIC -Os -o $@
	$(STRIP) $@

wsd_simple_server: $(OBJECTS_W)
	$(CC) $(OBJECTS_W) $(LIBS_W) -fPIC -Os -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f onvif_simple_server
	rm -f onvif_notify_server
	rm -f wsd_simple_server
	rm -f $(OBJECTS_O)
	rm -f $(OBJECTS_N)
	rm -f $(OBJECTS_W)
