# Set HAVE_WOLFSSL or HAVE_MBEDTLS variable if you want to use WOLFSSL or
# MBEDTLS instead of LIBTOMCRYPT

SRC_DIR		:= src

OBJECTS_O	 = $(SRC_DIR)/onvif_simple_server.o \
		   $(SRC_DIR)/onvif_dispatch.o \
		   $(SRC_DIR)/device_service.o \
		   $(SRC_DIR)/media_service.o \
		   $(SRC_DIR)/imaging_service.o \
		   $(SRC_DIR)/media2_service.o \
		   $(SRC_DIR)/ptz_service.o \
		   $(SRC_DIR)/events_service.o \
		   $(SRC_DIR)/deviceio_service.o \
		   $(SRC_DIR)/fault.o \
		   $(SRC_DIR)/conf.o \
		   $(SRC_DIR)/utils.o \
		   $(SRC_DIR)/log.o \
		   $(SRC_DIR)/mxml_wrapper.o \
		   $(SRC_DIR)/xml_logger.o \
		   $(SRC_DIR)/audio_output_enabled.o \
		   $(SRC_DIR)/prudynt_bridge.o

OBJECTS_N	 = $(SRC_DIR)/onvif_notify_server.o \
		   $(SRC_DIR)/conf.o \
		   $(SRC_DIR)/utils.o \
		   $(SRC_DIR)/log.o \
		   $(SRC_DIR)/mxml_wrapper.o \
		   $(SRC_DIR)/xml_logger.o

OBJECTS_W	 = $(SRC_DIR)/wsd_simple_server.o \
		   $(SRC_DIR)/utils.o \
		   $(SRC_DIR)/log.o \
		   $(SRC_DIR)/mxml_wrapper.o

# Common compiler and linker flags
INCLUDE		 = -ffunction-sections -fdata-sections
LIBS_O		 = -Wl,--gc-sections
LIBS_N		 = -Wl,--gc-sections

# Crypto library selection
ifdef HAVE_WOLFSSL
INCLUDE		+= -DHAVE_WOLFSSL
LIBS_O		+= -lwolfssl
LIBS_N		+= -lwolfssl
LIBS_W		 = -Wl,--gc-sections -lwolfssl
else
ifdef HAVE_MBEDTLS
INCLUDE		+= -DHAVE_MBEDTLS
LIBS_O		+= -lmbedcrypto
LIBS_N		+= -lmbedcrypto
LIBS_W		 = -Wl,--gc-sections -lmbedcrypto
else
LIBS_O		+= -ltomcrypt
LIBS_N		+= -ltomcrypt
LIBS_W		 = -Wl,--gc-sections -ltomcrypt
endif
endif

PKG_CONFIG	 ?= pkg-config
MXML_PKG	 ?= libmxml4
MXML_CFLAGS	 := $(shell $(PKG_CONFIG) --cflags $(MXML_PKG) 2>/dev/null)
MXML_LIBS	 := $(shell $(PKG_CONFIG) --libs $(MXML_PKG) 2>/dev/null)

ifeq ($(strip $(MXML_CFLAGS)$(MXML_LIBS)),)
MXML_PKG	 := mxml
MXML_CFLAGS	 := $(shell $(PKG_CONFIG) --cflags $(MXML_PKG) 2>/dev/null)
MXML_LIBS	 := $(shell $(PKG_CONFIG) --libs $(MXML_PKG) 2>/dev/null)
endif

ifeq ($(strip $(MXML_LIBS)),)
ifneq ($(strip $(STAGING_DIR)),)
ifneq ($(wildcard $(STAGING_DIR)/usr/lib/libmxml4.*),)
MXML_LIBS	 := -L$(STAGING_DIR)/usr/lib -lmxml4
else
MXML_LIBS	 := -L$(STAGING_DIR)/usr/lib -lmxml
endif
else
MXML_LIBS	 := -lmxml
endif
endif

ifeq ($(strip $(MXML_CFLAGS)),)
ifneq ($(strip $(STAGING_DIR)),)
MXML_CFLAGS	 := -I$(STAGING_DIR)/usr/include/libmxml4 -I$(STAGING_DIR)/usr/include
else
MXML_CFLAGS	 := -I/usr/include/libmxml4 -I/usr/include
endif
endif

# Common libraries for all builds
# Note: For Buildroot builds, mxml is provided by the system
# For development builds, mxml is built locally via build.sh
LIBS_O		+= -ljct -lpthread -lrt
LIBS_N		+= -ljct -lpthread -lrt
LIBS_W		+= -lpthread
LIBS_O		+= $(MXML_LIBS)
LIBS_N		+= $(MXML_LIBS)
LIBS_W		+= $(MXML_LIBS)

# Ensure headers under src/ and ezxml under project root are found
# jct headers are expected to be in system include path
INCLUDE		+= -I$(SRC_DIR) -I.
INCLUDE		+= $(MXML_CFLAGS)

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


# JCT library compilation (for development)
# In production, this would not be needed as jct is a system library
jct/src/%.o: jct/src/%.c
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

.PHONY: clean distclean debug

clean:
	rm -f onvif_simple_server onvif_simple_server_debug
	rm -f onvif_notify_server onvif_notify_server_debug
	rm -f wsd_simple_server wsd_simple_server_debug
	rm -f $(OBJECTS_O) $(OBJECTS_O_DEBUG)
	rm -f $(OBJECTS_N) $(OBJECTS_N_DEBUG)
	rm -f $(OBJECTS_W) $(OBJECTS_W_DEBUG)

distclean: clean
	@echo "Removing local development artifacts..."
	rm -rf jct/
	rm -rf mxml/
	rm -rf local/
	rm -rf container/*_files/
	rm -f container/onvif_simple_server
	rm -f container/onvif_notify_server
	rm -f container/wsd_simple_server
	rm -f container/onvif.json
	@echo "Development environment cleaned."
