#!/usr/bin/env bash
# ONVIF Media quick test: profiles, stream URIs, snapshot URIs
# Usage:
#   tools/onvif/test_media.sh -h 192.168.88.116 -u thingino -p thingino
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tools/onvif/lib_wsse.sh
source "$SCRIPT_DIR/lib_wsse.sh"

HOST=""; USER=""; PASS=""
while getopts ":h:u:p:" opt; do
  case $opt in
    h) HOST=$OPTARG ;;
    u) USER=$OPTARG ;;
    p) PASS=$OPTARG ;;
    *) echo "Usage: $0 -h HOST -u USER -p PASS" >&2; exit 2 ;;
  esac
done
[[ -n $HOST && -n $USER && -n $PASS ]] || { echo "Usage: $0 -h HOST -u USER -p PASS" >&2; exit 2; }
MEDIA_URL="http://$HOST/onvif/media_service"

WSSE=$(onvif_wsse_header "$USER" "$PASS")

# GetProfiles
REQ_PROFILES=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body><trt:GetProfiles/></s:Body>
</s:Envelope>
EOF
)
RESP_PROFILES=$(onvif_curl_post "$MEDIA_URL" "http://www.onvif.org/ver10/media/wsdl/GetProfiles" "$REQ_PROFILES")
TOKENS=$(printf "%s" "$RESP_PROFILES" | sed -n 's/.*Profiles[^>]*token="\([^"]\+\)".*/\1/p')
COUNT=$(printf "%s\n" "$TOKENS" | wc -l | tr -d ' ')
echo "Profiles ($COUNT):"; printf "%s\n" "$TOKENS"

# GetStreamUri + GetSnapshotUri for each
while IFS= read -r T; do
  [[ -n $T ]] || continue
  printf "\n== %s ==\n" "$T"
  REQ_STREAM=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
  <s:Header>$WSSE</s:Header>
  <s:Body>
    <trt:GetStreamUri>
      <trt:StreamSetup><tt:Stream>RTP-Unicast</tt:Stream><tt:Transport><tt:Protocol>RTSP</tt:Protocol></tt:Transport></trt:StreamSetup>
      <trt:ProfileToken>$T</trt:ProfileToken>
    </trt:GetStreamUri>
  </s:Body>
</s:Envelope>
EOF
)
  RESP_STREAM=$(onvif_curl_post "$MEDIA_URL" "http://www.onvif.org/ver10/media/wsdl/GetStreamUri" "$REQ_STREAM")
  echo "$RESP_STREAM" | sed -n 's|.*<tt:Uri>\(.*\)</tt:Uri>.*|StreamUri: \1|p'

  REQ_SNAP=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body><trt:GetSnapshotUri><trt:ProfileToken>$T</trt:ProfileToken></trt:GetSnapshotUri></s:Body>
</s:Envelope>
EOF
)
  RESP_SNAP=$(onvif_curl_post "$MEDIA_URL" "http://www.onvif.org/ver10/media/wsdl/GetSnapshotUri" "$REQ_SNAP")
  echo "$RESP_SNAP" | sed -n 's|.*<tt:Uri>\(.*\)</tt:Uri>.*|SnapshotUri: \1|p'
done <<< "$TOKENS"

printf "\nOK: Media checks complete\n"

