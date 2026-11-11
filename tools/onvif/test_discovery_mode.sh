#!/bin/bash
# Check GetDiscoveryMode, SetDiscoveryMode and IPv6 reporting through the
# ONVIF device service, using the existing WSSE test credential generator.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BIN="$PROJECT_ROOT/onvif_simple_server"
TMP="$(mktemp -d)"
CONFIG="$TMP/onvif.json"
TEMPLATES="$PROJECT_ROOT/res"
STATE="/tmp/onvif_discovery_mode"
trap 'rm -rf "$TMP"; rm -f "$STATE"' EXIT

failures=0
check() {
    local name="$1"
    local condition="$2"
    if [ "$condition" = "1" ]; then
        echo "PASS  $name"
    else
        echo "FAIL  $name"
        failures=$((failures + 1))
    fi
}

[ -x "$BIN" ] || { echo "FAIL  $BIN not built"; exit 1; }

python3 - "$TMP" "$PROJECT_ROOT/res/onvif.json" <<'PY'
import base64
import datetime
import hashlib
import os
import sys

out = sys.argv[1]
import json
with open(sys.argv[2]) as f:
    cfg = json.load(f)
cfg.setdefault('server', {})['ifs'] = 'lo'
cfg['server']['username'] = 'admin'
cfg['server']['password'] = 'admin'
with open(os.path.join(sys.argv[1], 'onvif.json'), 'w') as f:
    json.dump(cfg, f)
username = "admin"
password = "admin"
nonce = os.urandom(16)
created = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
digest = base64.b64encode(hashlib.sha1(nonce + created.encode() + password.encode()).digest()).decode()
nonce_b64 = base64.b64encode(nonce).decode()

header = (
    '<SOAP-ENV:Header>'
    '<wsse:Security SOAP-ENV:mustUnderstand="true" '
    'xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" '
    'xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">'
    '<wsse:UsernameToken>'
    f'<wsse:Username>{username}</wsse:Username>'
    '<wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">'
    f'{digest}</wsse:Password>'
    f'<wsse:Nonce>{nonce_b64}</wsse:Nonce>'
    f'<wsu:Created>{created}</wsu:Created>'
    '</wsse:UsernameToken></wsse:Security></SOAP-ENV:Header>'
)

def request(body):
    return (
        '<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" '
        'xmlns:tds="http://www.onvif.org/ver10/device/wsdl">'
        f'{header}<SOAP-ENV:Body>{body}</SOAP-ENV:Body></SOAP-ENV:Envelope>'
    )

requests = {
    'get': request('<tds:GetDiscoveryMode/>'),
    'set_non': request('<tds:SetDiscoveryMode><tds:DiscoveryMode>NonDiscoverable</tds:DiscoveryMode></tds:SetDiscoveryMode>'),
    'set_disc': request('<tds:SetDiscoveryMode><tds:DiscoveryMode>Discoverable</tds:DiscoveryMode></tds:SetDiscoveryMode>'),
    'interfaces': request('<tds:GetNetworkInterfaces/>'),
}
for name, xml in requests.items():
    with open(os.path.join(out, name + '.xml'), 'w') as f:
        f.write(xml)
PY

call_device() {
    local name="$1"
    local file="$TMP/$name.xml"
    (cd "$TEMPLATES" && REQUEST_METHOD=POST CONTENT_TYPE=application/soap+xml \
        SCRIPT_NAME=/onvif/device_service CONTENT_LENGTH="$(wc -c < "$file")" \
        "$BIN" -c "$CONFIG" device_service < "$file" 2>/dev/null)
}

check "GetDiscoveryMode starts Discoverable" \
    "$(call_device get | grep -q '<tds:DiscoveryMode>Discoverable</tds:DiscoveryMode>' && echo 1 || echo 0)"

call_device set_non >/dev/null
check "SetDiscoveryMode writes NonDiscoverable" \
    "$(grep -q '^NonDiscoverable$' "$STATE" 2>/dev/null && echo 1 || echo 0)"
check "GetDiscoveryMode reports NonDiscoverable" \
    "$(call_device get | grep -q '<tds:DiscoveryMode>NonDiscoverable</tds:DiscoveryMode>' && echo 1 || echo 0)"

call_device set_disc >/dev/null
check "GetDiscoveryMode returns to Discoverable" \
    "$(call_device get | grep -q '<tds:DiscoveryMode>Discoverable</tds:DiscoveryMode>' && echo 1 || echo 0)"

check "GetNetworkInterfaces contains an IPv6 block" \
    "$(call_device interfaces | grep -q '<tt:IPv6>' && echo 1 || echo 0)"

if [ "$failures" -eq 0 ]; then
    echo "All discovery mode checks passed."
    exit 0
fi

echo "$failures discovery mode check(s) failed."
exit 1
