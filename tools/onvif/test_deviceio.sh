#!/usr/bin/env bash
# ONVIF DeviceIO quick test: relay outputs and optional toggle
# Usage:
#   tools/onvif/test_deviceio.sh -h 192.168.88.116 -u thingino -p thingino [--pulse TOKEN]
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tools/onvif/lib_wsse.sh
source "$SCRIPT_DIR/lib_wsse.sh"

HOST=""; USER=""; PASS=""; PULSE_TOKEN=""
while (( "$#" )); do
  case ${1:-} in
    -h) HOST=${2:?}; shift 2 ;;
    -u) USER=${2:?}; shift 2 ;;
    -p) PASS=${2:?}; shift 2 ;;
    --pulse) PULSE_TOKEN=${2:-}; shift 2 ;;
    *) echo "Usage: $0 -h HOST -u USER -p PASS [--pulse TOKEN]" >&2; exit 2 ;;
  esac
done
[[ -n $HOST && -n $USER && -n $PASS ]] || { echo "Usage: $0 -h HOST -u USER -p PASS [--pulse TOKEN]" >&2; exit 2; }
DEVIO_URL="http://$HOST/onvif/deviceio_service"

WSSE=$(onvif_wsse_header "$USER" "$PASS")

say() { printf "\n==== %s ====\n" "$*"; }

say "GetRelayOutputs"
REQ=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tmd="http://www.onvif.org/ver10/deviceIO/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body><tmd:GetRelayOutputs/></s:Body>
</s:Envelope>
EOF
)
RESP=$(onvif_curl_post "$DEVIO_URL" "http://www.onvif.org/ver10/deviceIO/wsdl/GetRelayOutputs" "$REQ")
NS_OK=$(grep -c '<tmd:GetRelayOutputsResponse' <<<"$RESP" || true)
TOKENS=$(sed -n 's/.*token="\([^"]\+\)".*/\1/p' <<<"$RESP")
echo "tmd namespace OK: $NS_OK"; echo "Relay tokens:"; printf "%s\n" "$TOKENS"

if [[ -n $PULSE_TOKEN ]]; then
  say "SetRelayOutputState pulse token=$PULSE_TOKEN"
  for STATE in active inactive; do
    REQ2=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tmd="http://www.onvif.org/ver10/deviceIO/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
  <s:Header>$WSSE</s:Header>
  <s:Body><tmd:SetRelayOutputState><tmd:RelayOutputToken>$PULSE_TOKEN</tmd:RelayOutputToken><tmd:LogicalState>$STATE</tmd:LogicalState></tmd:SetRelayOutputState></s:Body>
</s:Envelope>
EOF
)

    onvif_curl_post "$DEVIO_URL" "http://www.onvif.org/ver10/deviceIO/wsdl/SetRelayOutputState" "$REQ2" >/dev/null || true
    echo "SetRelayOutputState -> $STATE"
    sleep 1
  done
fi

printf "\nOK: DeviceIO checks complete\n"

