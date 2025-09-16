# Set HAVE_WOLFSSL or HAVE_MBEDTLS variable if you want to use WOLFSSL or
# MBEDTLS instead of LIBTOMCRYPT

SRC_DIR := src

OBJECTS_O = $(SRC_DIR)/onvif_simple_server.o \
			$(SRC_DIR)/device_service.o \
			$(SRC_DIR)/media_service.o \
			$(SRC_DIR)/media2_service.o \
			$(SRC_DIR)/ptz_service.o \
			$(SRC_DIR)/events_service.o \
			$(SRC_DIR)/deviceio_service.o \
			$(SRC_DIR)/fault.o \
			$(SRC_DIR)/conf.o \
			$(SRC_DIR)/utils.o \
			$(SRC_DIR)/log.o \
			$(SRC_DIR)/ezxml_wrapper.o \
			ezxml/ezxml.o

OBJECTS_N = $(SRC_DIR)/onvif_notify_server.o \
			$(SRC_DIR)/conf.o \
			$(SRC_DIR)/utils.o \
			$(SRC_DIR)/log.o \
			$(SRC_DIR)/ezxml_wrapper.o \
			ezxml/ezxml.o

OBJECTS_W = $(SRC_DIR)/wsd_simple_server.o \
			$(SRC_DIR)/utils.o \
			$(SRC_DIR)/log.o \
			$(SRC_DIR)/ezxml_wrapper.o \
			ezxml/ezxml.o

ifdef HAVE_WOLFSSL
INCLUDE = -DHAVE_WOLFSSL -ffunction-sections -fdata-sections
LIBS_O = -Wl,--gc-sections -lwolfssl -ljson-c -lpthread -lrt
LIBS_N = -Wl,--gc-sections -lwolfssl -ljson-c -lpthread -lrt
else
ifdef HAVE_MBEDTLS
INCLUDE = -DHAVE_MBEDTLS -ffunction-sections -fdata-sections
LIBS_O = -Wl,--gc-sections -lmbedcrypto -ljson-c -lpthread -lrt
LIBS_N = -Wl,--gc-sections -lmbedcrypto -ljson-c -lpthread -lrt
else
INCLUDE = -ffunction-sections -fdata-sections
LIBS_O = -Wl,--gc-sections -ltomcrypt -ljson-c -lpthread -lrt
LIBS_N = -Wl,--gc-sections -ltomcrypt -ljson-c -lpthread -lrt
endif
endif
LIBS_W = -Wl,--gc-sections -lpthread

# Ensure headers under src/ and ezxml under project root are found
INCLUDE += -I$(SRC_DIR) -I.

ifeq ($(STRIP), )
    STRIP=echo
endif

all: onvif_simple_server onvif_notify_server wsd_simple_server

# Debug build with AddressSanitizer and debugging symbols
debug: CFLAGS_DEBUG = -g -O0 -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -DDEBUG
debug: LDFLAGS_DEBUG = -fsanitize=address -fsanitize=undefined
debug: clean onvif_simple_server_debug onvif_notify_server_debug wsd_simple_server_debug

$(SRC_DIR)/log.o: $(SRC_DIR)/log.c $(HEADERS)
	$(CC) -c $< -std=c99 -fPIC -Os $(INCLUDE) -o $@

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

# Debug build rules
$(SRC_DIR)/%.debug.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) -c $< $(CFLAGS_DEBUG) $(INCLUDE) -o $@

ezxml/%.debug.o: ezxml/%.c
	$(CC) -c $< $(CFLAGS_DEBUG) $(INCLUDE) -o $@

$(SRC_DIR)/log.debug.o: $(SRC_DIR)/log.c $(HEADERS)
	$(CC) -c $< -std=c99 $(CFLAGS_DEBUG) $(INCLUDE) -o $@

OBJECTS_O_DEBUG = $(OBJECTS_O:.o=.debug.o)
OBJECTS_N_DEBUG = $(OBJECTS_N:.o=.debug.o)
OBJECTS_W_DEBUG = $(OBJECTS_W:.o=.debug.o)

onvif_simple_server_debug: $(OBJECTS_O_DEBUG)
	$(CC) $(OBJECTS_O_DEBUG) $(LIBS_O) $(LDFLAGS_DEBUG) -o $@

onvif_notify_server_debug: $(OBJECTS_N_DEBUG)
	$(CC) $(OBJECTS_N_DEBUG) $(LIBS_N) $(LDFLAGS_DEBUG) -o $@

wsd_simple_server_debug: $(OBJECTS_W_DEBUG)
	$(CC) $(OBJECTS_W_DEBUG) $(LIBS_W) $(LDFLAGS_DEBUG) -o $@

.PHONY: clean debug

clean:
	rm -f onvif_simple_server onvif_simple_server_debug
	rm -f onvif_notify_server onvif_notify_server_debug
	rm -f wsd_simple_server wsd_simple_server_debug
	rm -f $(OBJECTS_O) $(OBJECTS_O_DEBUG)
	rm -f $(OBJECTS_N) $(OBJECTS_N_DEBUG)
	rm -f $(OBJECTS_W) $(OBJECTS_W_DEBUG)
