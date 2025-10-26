#!/usr/bin/env bash
# Aggregate ONVIF camera check (media, events, deviceio)
# Usage:
#   tools/onvif/test_all.sh -h 192.168.88.116 -u thingino -p thingino [--events-seconds 45] [--pulse TOKEN]
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

HOST=""; USER=""; PASS=""; EV_SECS=30; PULSE_TOKEN=""
while (( "$#" )); do
  case ${1:-} in
    -h) HOST=${2:?}; shift 2 ;;
    -u) USER=${2:?}; shift 2 ;;
    -p) PASS=${2:?}; shift 2 ;;
    --events-seconds) EV_SECS=${2:?}; shift 2 ;;
    --pulse) PULSE_TOKEN=${2:?}; shift 2 ;;
    *) echo "Usage: $0 -h HOST -u USER -p PASS [--events-seconds N] [--pulse TOKEN]" >&2; exit 2 ;;
  esac
done
[[ -n $HOST && -n $USER && -n $PASS ]] || { echo "Usage: $0 -h HOST -u USER -p PASS [--events-seconds N] [--pulse TOKEN]" >&2; exit 2; }

"$SCRIPT_DIR/test_media.sh" -h "$HOST" -u "$USER" -p "$PASS"
"$SCRIPT_DIR/test_deviceio.sh" -h "$HOST" -u "$USER" -p "$PASS" ${PULSE_TOKEN:+--pulse "$PULSE_TOKEN"}
"$SCRIPT_DIR/test_events.sh" -h "$HOST" -u "$USER" -p "$PASS" -t "$EV_SECS"

echo "\nOK: All tests done"

