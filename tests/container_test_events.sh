#!/bin/bash
# ONVIF Events tests: PullPoint delivery and hardware failure events.
#
# Run from container_test.sh, or directly with the same env:
#   SERVER_URL=http://localhost:8000 CONTAINER_NAME=oss bash tests/container_test_events.sh
#
# The failure-event tests are conditional. They run only when the event is
# present in the server's config, and only against the local podman container
# (the test has to create and remove the input file).

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

SERVER_URL="${SERVER_URL:-http://localhost:8000}"
DEFAULT_USERNAME="${DEFAULT_USERNAME:-thingino}"
DEFAULT_PASSWORD="${DEFAULT_PASSWORD:-thingino}"
CONTAINER_NAME="${CONTAINER_NAME:-oss}"
VERBOSE="${VERBOSE:-false}"

EVENTS_URL="$SERVER_URL/onvif/events_service"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0

pass() {
    echo -e "${GREEN}✓ $*${NC}"
    PASS=$((PASS + 1))
}

fail() {
    echo -e "${RED}✗ $*${NC}"
    FAIL=$((FAIL + 1))
}

skip() {
    echo -e "${YELLOW}⚠ $*${NC}"
    SKIP=$((SKIP + 1))
}

wsse_header() {
    PYTHONPATH="$PROJECT_ROOT/tests" python3 - "$1" "$2" <<'PY'
import sys
from generate_onvif_auth import generate_ws_security_digest

auth = generate_ws_security_digest(sys.argv[1], sys.argv[2])
print(f'''<wsse:Security s:mustUnderstand="1" xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">
  <wsse:UsernameToken>
    <wsse:Username>{auth['username']}</wsse:Username>
    <wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">{auth['password_digest']}</wsse:Password>
    <wsse:Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">{auth['nonce']}</wsse:Nonce>
    <wsu:Created>{auth['created']}</wsu:Created>
  </wsse:UsernameToken>
</wsse:Security>''')
PY
}

envelope() {
    local body=$1
    local wsse
    wsse=$(wsse_header "$DEFAULT_USERNAME" "$DEFAULT_PASSWORD")
    cat <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
            xmlns:tev="http://www.onvif.org/ver10/events/wsdl"
            xmlns:wsnt="http://docs.oasis-open.org/wsn/b-2"
            xmlns:wsa="http://www.w3.org/2005/08/addressing"
            xmlns:tt="http://www.onvif.org/ver10/schema">
  <s:Header>
$wsse
  </s:Header>
  <s:Body>
$body
  </s:Body>
</s:Envelope>
EOF
}

soap_post() {
    local url=$1
    local body=$2
    curl -sS -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -H 'SOAPAction: ""' \
        --data "$body" \
        "$url"
}

get_event_properties() {
    soap_post "$EVENTS_URL" "$(envelope '<tev:GetEventProperties/>')"
}

create_subscription() {
    local resp sub addr
    resp=$(soap_post "$EVENTS_URL" "$(envelope '<tev:CreatePullPointSubscription><tev:InitialTerminationTime>PT60S</tev:InitialTerminationTime></tev:CreatePullPointSubscription>')")
    resp=$(printf '%s' "$resp" | tr -d '\n')
    addr=$(printf '%s' "$resp" | sed -n 's|.*<wsa:Address>\([^<]*\)</wsa:Address>.*|\1|p')
    sub=$(printf '%s' "$addr" | sed -n 's/.*[?&]sub=\([0-9][0-9]*\).*/\1/p')
    [ -n "$sub" ] && printf '%s' "$sub"
}

pull_messages() {
    local sub=$1
    soap_post "$EVENTS_URL?sub=$sub" "$(envelope '<tev:PullMessages><tev:Timeout>PT5S</tev:Timeout><tev:MessageLimit>10</tev:MessageLimit></tev:PullMessages>')"
}

# GetEventProperties lists topics as nested elements, so look for the leaf.
event_present() {
    local topic=$1
    [ -n "$(topic_description "$(get_event_properties)" "${topic##*/}")" ]
}

container_available() {
    command -v podman >/dev/null 2>&1 || return 1
    podman ps --format '{{.Names}}' 2>/dev/null | grep -qx "$CONTAINER_NAME"
}

set_flag() {
    local flag=$1
    local on=$2
    if [ "$on" = true ]; then
        podman exec "$CONTAINER_NAME" sh -c "mkdir -p /run/motion && : > '$flag'" >/dev/null 2>&1
    else
        podman exec "$CONTAINER_NAME" sh -c "rm -f '$flag'" >/dev/null 2>&1
    fi
}

value_present() {
    local response=$1
    local name=$2
    local value=$3
    printf '%s' "$response" | tr -d '\n' | grep -Eq "Name=\"$name\"[^>]*Value=\"$value\""
}

# The topic's leaf element in GetEventProperties, e.g. StorageFailure, with its
# MessageDescription (the topic set nests Device > HardwareFailure > leaf).
topic_description() {
    local props=$1
    local leaf=$2
    printf '%s' "$props" | tr -d '\n' | sed -n "s|.*\(<\([a-z0-9]*:\)\{0,1\}$leaf[ >].*</\([a-z0-9]*:\)\{0,1\}$leaf>\).*|\1|p"
}

# GetEventProperties advertises the data item and, when the spec has one, the
# Token source.
check_failure_description() {
    local topic=$1
    local data_name=$2
    local source=$3
    local desc
    desc=$(topic_description "$(get_event_properties)" "${topic##*/}")

    if printf '%s' "$desc" | grep -q "Name=\"$data_name\" Type=\"xsd:boolean\""; then
        pass "$topic advertises $data_name (xsd:boolean)"
    else
        fail "$topic does not advertise $data_name (xsd:boolean)"
        [ "$VERBOSE" = true ] && printf '%s\n' "$desc"
    fi

    if [ -n "$source" ]; then
        if printf '%s' "$desc" | grep -q "<tt:Source><tt:SimpleItemDescription Name=\"$source\""; then
            pass "$topic advertises the $source source"
        else
            fail "$topic does not advertise the $source source"
        fi
    elif printf '%s' "$desc" | grep -q "<tt:Source"; then
        fail "$topic advertises a Source; the spec defines none"
    else
        pass "$topic advertises no Source"
    fi
}

check_failure_event() {
    local topic=$1
    local flag=$2
    local data_name=$3
    local source=${4:-}

    echo -e "\n${YELLOW}Testing: $topic${NC}"

    if ! event_present "$topic"; then
        skip "$topic is not configured; skipping"
        return 0
    fi

    check_failure_description "$topic" "$data_name" "$source"

    if ! container_available; then
        skip "$topic is configured; failure flags need the local test container"
        return 0
    fi

    # Start clean so the subscription's initial event is a known state.
    set_flag "$flag" false
    sleep 2

    # No flag: no card or a healthy one (the firmware monitor decides). The
    # Initialized message must report false.
    local sub resp
    sub=$(create_subscription)
    resp=$([ -n "$sub" ] && pull_messages "$sub")
    if printf '%s' "$resp" | grep -qF "$topic" && value_present "$resp" "$data_name" false; then
        pass "$topic reports $data_name=false without the flag (no card, or healthy)"
    else
        fail "$topic did not report $data_name=false without the flag (no card, or healthy)"
        [ "$VERBOSE" = true ] && printf '%s\n' "$resp"
    fi

    set_flag "$flag" true
    sleep 2

    sub=$(create_subscription)
    if [ -z "$sub" ]; then
        fail "could not create a PullPoint subscription for $topic"
        set_flag "$flag" false
        return 0
    fi

    resp=$(pull_messages "$sub")
    if printf '%s' "$resp" | grep -qF "$topic" && value_present "$resp" "$data_name" true; then
        pass "$topic reports $data_name=true while the flag is present"
    else
        fail "$topic did not report $data_name=true"
        [ "$VERBOSE" = true ] && printf '%s\n' "$resp"
    fi

    set_flag "$flag" false
    sleep 2

    resp=$(pull_messages "$sub")
    if printf '%s' "$resp" | grep -qF "$topic" && value_present "$resp" "$data_name" false; then
        pass "$topic returns to $data_name=false when the flag is removed"
    else
        fail "$topic did not return to $data_name=false"
        [ "$VERBOSE" = true ] && printf '%s\n' "$resp"
    fi

    set_flag "$flag" false
}

echo -e "${BLUE}=== ONVIF Events: hardware failure coverage ===${NC}"

# The generic PullPoint path must work even when no failure event is configured.
sub=$(create_subscription)
if [ -n "$sub" ]; then
    resp=$(pull_messages "$sub")
    if [ -n "$resp" ] && printf '%s' "$resp" | grep -q "PullMessagesResponse"; then
        pass "PullPoint subscription and PullMessages"
    else
        fail "PullPoint subscription and PullMessages"
        [ "$VERBOSE" = true ] && printf '%s\n' "$resp"
    fi
else
    fail "CreatePullPointSubscription"
fi

# StorageFailure: ONVIF Core 25.12 8.9.10, data field Failed.
check_failure_event \
    "tns1:Device/HardwareFailure/StorageFailure" \
    "/run/motion/storage_failure" \
    "Failed" \
    "Token"

# TemperatureCritical: ONVIF Core 25.12 8.9.10, data field Critical.
check_failure_event \
    "tns1:Device/HardwareFailure/TemperatureCritical" \
    "/run/motion/temperature_critical" \
    "Critical"

echo -e "\n${BLUE}Events: ${PASS} passed, ${FAIL} failed, ${SKIP} skipped${NC}"
[ "$FAIL" -eq 0 ]
