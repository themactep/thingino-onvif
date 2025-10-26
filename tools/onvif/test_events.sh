#!/usr/bin/env bash
# ONVIF Events test: capabilities, properties, PullPoint messages
# Usage:
#   tools/onvif/test_events.sh -h 192.168.88.116 -u thingino -p thingino [-t 60]
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tools/onvif/lib_wsse.sh
source "$SCRIPT_DIR/lib_wsse.sh"

HOST=""; USER=""; PASS=""; DURATION=30
while getopts ":h:u:p:t:" opt; do
  case $opt in
    h) HOST=$OPTARG ;;
    u) USER=$OPTARG ;;
    p) PASS=$OPTARG ;;
    t) DURATION=$OPTARG ;;
    *) echo "Usage: $0 -h HOST -u USER -p PASS [-t seconds]" >&2; exit 2 ;;
  esac
done
[[ -n $HOST && -n $USER && -n $PASS ]] || { echo "Usage: $0 -h HOST -u USER -p PASS [-t seconds]" >&2; exit 2; }
EVENTS_URL="http://$HOST/onvif/events_service"

WSSE=$(onvif_wsse_header "$USER" "$PASS")

say() { printf "\n==== %s ====\n" "$*"; }

# 1) GetServiceCapabilities
say "GetServiceCapabilities"
REQ_CAPS=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tev="http://www.onvif.org/ver10/events/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body><tev:GetServiceCapabilities/></s:Body>
</s:Envelope>
EOF
)
RESP_CAPS=$(onvif_curl_post "$EVENTS_URL" "http://www.onvif.org/ver10/events/wsdl/GetServiceCapabilities" "$REQ_CAPS")
PULL=$(printf "%s" "$RESP_CAPS" | sed -n 's/.*<tev:WSPullPointSupport>\(true\|false\)<.*/\1/p')
echo "WSPullPointSupport: ${PULL:-missing}"

# 2) GetEventProperties
say "GetEventProperties"
REQ_PROPS=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tev="http://www.onvif.org/ver10/events/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body><tev:GetEventProperties/></s:Body>
</s:Envelope>
EOF
)
RESP_PROPS=$(onvif_curl_post "$EVENTS_URL" "http://www.onvif.org/ver10/events/wsdl/GetEventProperties" "$REQ_PROPS")
HAS_MOTION=$(echo "$RESP_PROPS" | grep -i -E "VideoSource/MotionAlarm|CellMotionDetector/Motion" -c || true)
HAS_SCHEMA=$(echo "$RESP_PROPS" | grep -i -E "SimpleItemDescription[^>]*Name=\"IsMotion\"|SimpleItemDescription[^>]*Name=\'IsMotion\'" -c || true)
echo "Motion topic present: $HAS_MOTION"; echo "IsMotion schema present: $HAS_SCHEMA"

# 3) CreatePullPointSubscription
say "CreatePullPointSubscription"
REQ_CREATE=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tev="http://www.onvif.org/ver10/events/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body><tev:CreatePullPointSubscription><tev:InitialTerminationTime>PT5M</tev:InitialTerminationTime></tev:CreatePullPointSubscription></s:Body>
</s:Envelope>
EOF
)
RESP_CREATE=$(onvif_curl_post "$EVENTS_URL" "http://www.onvif.org/ver10/events/wsdl/CreatePullPointSubscription" "$REQ_CREATE")
EPR_URL=$(printf "%s" "$RESP_CREATE" | sed -n 's|.*<wsa5:Address>\(http://[^<]*\)</wsa5:Address>.*|\1|p')
[[ -n $EPR_URL ]] || EPR_URL=$(printf "%s" "$RESP_CREATE" | sed -n 's|.*<wsa:Address>\(http://[^<]*\)</wsa:Address>.*|\1|p')
SUB_ID=$(printf "%s" "$EPR_URL" | sed -n 's|.*[?&]sub=\([0-9][0-9]*\).*|\1|p')
echo "EPR: $EPR_URL (sub=$SUB_ID)"

# 4) PullMessages loop
say "PullMessages for ${DURATION}s (looking for Initialized/Changed IsMotion)"
END=$(( $(date +%s) + DURATION ))
INIT=0; CHG_TRUE=0; CHG_FALSE=0; ITER=0
while [[ $(date +%s) -lt $END ]]; do
  ITER=$((ITER+1))
  REQ_PULL=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tev="http://www.onvif.org/ver10/events/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body><tev:PullMessages><tev:Timeout>PT3S</tev:Timeout><tev:MessageLimit>16</tev:MessageLimit></tev:PullMessages></s:Body>
</s:Envelope>
EOF
)
  RESP_PULL=$(onvif_curl_post "$EVENTS_URL?sub=$SUB_ID" "http://www.onvif.org/ver10/events/wsdl/PullMessages" "$REQ_PULL")
  grep -q 'PropertyOperation="Initialized"' <<<"$RESP_PULL" && INIT=1 || true
  grep -q 'PropertyOperation="Changed"' <<<"$RESP_PULL" && {
    grep -q 'Name="IsMotion" Value="true"' <<<"$RESP_PULL" && CHG_TRUE=1 || true
    grep -q 'Name="IsMotion" Value="false"' <<<"$RESP_PULL" && CHG_FALSE=1 || true
  } || true
  [[ $INIT -eq 1 && $CHG_TRUE -eq 1 && $CHG_FALSE -eq 1 ]] && break
  sleep 1
done

echo "Initialized: $INIT, Changed(true): $CHG_TRUE, Changed(false): $CHG_FALSE"
[[ $INIT -eq 1 ]] || { echo "WARN: No Initialized event seen"; }
[[ $CHG_TRUE -eq 1 ]] || { echo "WARN: No Changed(true) seen - try moving in front of the camera"; }
[[ $CHG_FALSE -eq 1 ]] || { echo "WARN: No Changed(false) seen - wait for motion end"; }

printf "\nOK: Events checks complete\n"

