#!/usr/bin/env bash
# Minimal ONVIF Imaging service check: capabilities, settings, options, set
# Usage:
#   tools/onvif/test_imaging.sh -h 192.168.88.116 -u thingino -p thingino [-t VideoSourceToken]
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tools/onvif/lib_wsse.sh
source "$SCRIPT_DIR/lib_wsse.sh"

HOST=""; USER=""; PASS=""; TOKEN="VideoSourceToken"
while getopts ":h:u:p:t:" opt; do
  case $opt in
    h) HOST=$OPTARG ;;
    u) USER=$OPTARG ;;
    p) PASS=$OPTARG ;;
    t) TOKEN=$OPTARG ;;
    *) echo "Usage: $0 -h HOST -u USER -p PASS [-t VideoSourceToken]" >&2; exit 2 ;;
  esac
done
[[ -n $HOST && -n $USER && -n $PASS ]] || { echo "Usage: $0 -h HOST -u USER -p PASS [-t VideoSourceToken]" >&2; exit 2; }
IMAGING_URL="http://$HOST/onvif/imaging_service"
WSSE=$(onvif_wsse_header "$USER" "$PASS")

echo "== GetServiceCapabilities =="
REQ_CAPS=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:timg="http://www.onvif.org/ver20/imaging/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body><timg:GetServiceCapabilities/></s:Body>
</s:Envelope>
EOF
)
RESP_CAPS=$(onvif_curl_post "$IMAGING_URL" "http://www.onvif.org/ver20/imaging/wsdl/GetServiceCapabilities" "$REQ_CAPS")
printf '%s\n' "$RESP_CAPS" | sed -n 's|.*<timg:\([^>]*\)>\(true\|false\)</timg.*|\1=\2|p'

echo "\n== GetImagingSettings ($TOKEN) =="
REQ_SETTINGS=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:timg="http://www.onvif.org/ver20/imaging/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body>
    <timg:GetImagingSettings>
      <timg:VideoSourceToken>$TOKEN</timg:VideoSourceToken>
    </timg:GetImagingSettings>
  </s:Body>
</s:Envelope>
EOF
)
RESP_SETTINGS=$(onvif_curl_post "$IMAGING_URL" "http://www.onvif.org/ver20/imaging/wsdl/GetImagingSettings" "$REQ_SETTINGS")
CURRENT_IR=$(printf '%s' "$RESP_SETTINGS" | sed -n 's|.*<tt:IrCutFilter>\(.*\)</tt:IrCutFilter>.*|\1|p')
[[ -n $CURRENT_IR ]] || CURRENT_IR="Unknown"
echo "IrCutFilter: $CURRENT_IR"

echo "\n== GetOptions ($TOKEN) =="
REQ_OPTIONS=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:timg="http://www.onvif.org/ver20/imaging/wsdl">
  <s:Header>$WSSE</s:Header>
  <s:Body>
    <timg:GetOptions>
      <timg:VideoSourceToken>$TOKEN</timg:VideoSourceToken>
    </timg:GetOptions>
  </s:Body>
</s:Envelope>
EOF
)
RESP_OPTIONS=$(onvif_curl_post "$IMAGING_URL" "http://www.onvif.org/ver20/imaging/wsdl/GetOptions" "$REQ_OPTIONS")
printf '%s\n' "$RESP_OPTIONS" | sed -n 's|.*<tt:IrCutFilterModes>\(.*\)</tt:IrCutFilterModes>.*|mode: \1|p'

echo "\n== SetImagingSettings (noop) =="
REQ_SET=$(cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:timg="http://www.onvif.org/ver20/imaging/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
  <s:Header>$WSSE</s:Header>
  <s:Body>
    <timg:SetImagingSettings>
      <timg:VideoSourceToken>$TOKEN</timg:VideoSourceToken>
      <timg:ImagingSettings>
        <tt:ImagingSettings20 token="$TOKEN">
          <tt:IrCutFilter>$CURRENT_IR</tt:IrCutFilter>
        </tt:ImagingSettings20>
      </timg:ImagingSettings>
    </timg:SetImagingSettings>
  </s:Body>
</s:Envelope>
EOF
)
onvif_curl_post "$IMAGING_URL" "http://www.onvif.org/ver20/imaging/wsdl/SetImagingSettings" "$REQ_SET" >/dev/null

echo "\nOK: Imaging service checks complete"
