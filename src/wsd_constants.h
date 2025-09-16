/*
 * Copyright (c) 2024 roleo.
 * Copyright (c) 2025 Thingino project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ONVIF_WSD_CONSTANTS_H
#define ONVIF_WSD_CONSTANTS_H

// WS-Discovery Protocol Constants
#define WSD_MULTICAST_ADDRESS "239.255.255.250"
#define WSD_PORT 3702
#define WSD_DEVICE_TYPE "NetworkVideoTransmitter"

// File System Constants
#ifndef WSD_TEMPLATE_DIR
#define WSD_TEMPLATE_DIR "/var/www/onvif/wsd_files"
#endif

// Buffer Size Constants
#define WSD_RECV_BUFFER_LEN 4096
#define WSD_PID_SIZE 32

// Daemon Control Flags
#define BD_NO_CHDIR 01
#define BD_NO_CLOSE_FILES 02
#define BD_NO_REOPEN_STD_FDS 04
#define BD_NO_UMASK0 010
#define BD_MAX_CLOSE 8192

#endif // ONVIF_WSD_CONSTANTS_H
