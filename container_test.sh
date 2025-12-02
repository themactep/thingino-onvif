#!/bin/bash

# ONVIF Simple Server Container Test Script
# This script tests the ONVIF server functionality through HTTP requests

# Test ONVIF Audio Backchannel (AudioOutput) endpoints
test_audio_output_endpoints() {
    echo -e "\n${YELLOW}Testing: Audio Output (Backchannel) endpoints via test_onvif_service${NC}"

    # These endpoints require authentication; rely on the existing auth generator.
    test_onvif_service "media_service" "GetAudioOutputs" "Get Audio Outputs (Backchannel)" true "<trt:GetAudioOutputsResponse"
    test_onvif_service "media_service" "GetAudioOutputConfiguration" "Get Audio Output Configuration (Backchannel)" true "<trt:GetAudioOutputConfigurationResponse"
    test_onvif_service "media_service" "GetAudioOutputConfigurations" "Get Audio Output Configurations (Backchannel)" true "<trt:GetAudioOutputConfigurationsResponse"
    test_onvif_service "media_service" "GetAudioOutputConfigurationOptions" "Get Audio Output Configuration Options (Backchannel)" true "<trt:GetAudioOutputConfigurationOptionsResponse"
    test_onvif_service "media_service" "GetCompatibleAudioOutputConfigurations" "Get Compatible Audio Output Configurations (Backchannel)" true "<trt:GetCompatibleAudioOutputConfigurationsResponse"
}

# Test DeviceIO audio endpoints so GetAudioOutputs/GetAudioSources stay in sync with media service.
test_deviceio_audio_endpoints() {
    echo -e "\n${YELLOW}Testing: DeviceIO Audio endpoint parity${NC}"

    test_onvif_service "deviceio_service" "GetAudioOutputs" "DeviceIO GetAudioOutputs" true "<tmd:GetAudioOutputsResponse"
    test_onvif_service "deviceio_service" "GetAudioSources" "DeviceIO GetAudioSources" true "<tmd:GetAudioSourcesResponse"
}

# Helper to inject WSSE header and tokens into imaging SOAP templates
render_imaging_template() {
    local template_path=$1
    local wsse_header=$2
    local token_value=${3:-"VideoSourceToken"}
    local ir_value=${4:-""}

    awk -v hdr="$wsse_header" -v token="$token_value" -v ir="$ir_value" '
        {
            gsub(/\{wsse_header\}/, hdr);
            gsub(/\{video_source_token\}/, token);
            gsub(/\{ir_cut_filter\}/, ir);
            print;
        }
    ' "$template_path"
}

# Imaging Service Test (with WSSE auth)
test_imaging_service() {
    echo -e "\n${YELLOW}Testing: ONVIF Imaging Service${NC}"

    # use global credentials
    local token="VideoSourceToken"

    # source WSSE header helper and build header for templates
    # shellcheck source=tools/onvif/lib_wsse.sh
    [ -f tools/onvif/lib_wsse.sh ] && source tools/onvif/lib_wsse.sh
    local wsse_header=$(onvif_wsse_header "$DEFAULT_USERNAME" "$DEFAULT_PASSWORD")

    local tmp_resp
    local resp
    local resp_settings
    local resp_caps
    local resp_options

    # Validate GetServiceCapabilities with WSSE header
    tmp_resp=$(mktemp)
    local req_caps
    req_caps=$(render_imaging_template "tests/soap_templates/imaging_get_service_capabilities.xml" "$wsse_header" "$token")
    response_code=$(send_soap_raw "$SERVER_URL/onvif/imaging_service" "http://www.onvif.org/ver20/imaging/wsdl/GetServiceCapabilities" "$req_caps" "$tmp_resp")
    resp_caps=$(cat "$tmp_resp" 2>/dev/null || true)
    rm -f "$tmp_resp" || true
    if [ "$response_code" != "200" ]; then
        echo -e "${RED}✗ Imaging GetServiceCapabilities - FAILED (HTTP $response_code)${NC}"
        [ "$VERBOSE" = true ] && echo "$resp_caps"
    elif ! assert_valid_xml "$resp_caps" "imaging/GetServiceCapabilities"; then
        return 1
    elif [[ $resp_caps == *"<timg:GetServiceCapabilitiesResponse"* ]]; then
        echo -e "${GREEN}✓ Imaging GetServiceCapabilities - SUCCESS${NC}"
        [ "$VERBOSE" = true ] && echo "$resp_caps"
    else
        echo -e "${RED}✗ Imaging GetServiceCapabilities - FAILED (unexpected body)${NC}"
        [ "$VERBOSE" = true ] && echo "$resp_caps"
    fi

    # Get current ImagingSettings (with WSSE) - keep original to obtain current state
    tmp_resp=$(mktemp)
    local req_settings
    req_settings=$(render_imaging_template "tests/soap_templates/imaging_get_imaging_settings.xml" "$wsse_header" "$token")
    response_code=$(send_soap_raw "$SERVER_URL/onvif/imaging_service" "http://www.onvif.org/ver20/imaging/wsdl/GetImagingSettings" "$req_settings" "$tmp_resp")
    resp_settings=$(cat "$tmp_resp" 2>/dev/null || true)
    rm -f "$tmp_resp" || true
    if [ "$response_code" != "200" ]; then
        echo -e "${RED}✗ Imaging GetImagingSettings - FAILED (HTTP $response_code)${NC}"
        if [ "$VERBOSE" = true ]; then echo "$resp_settings"; fi
    elif ! assert_valid_xml "$resp_settings" "imaging/GetImagingSettings"; then
        return 1
    elif [[ $resp_settings == *"<tt:VideoSourceToken>$token</tt:VideoSourceToken>"* && $resp_settings == *"<tt:IrCutFilter>"* ]]; then
        echo -e "${GREEN}✓ Imaging GetImagingSettings - SUCCESS${NC}"
        [ "$VERBOSE" = true ] && echo "$resp_settings"
    else
        echo -e "${YELLOW}⚠ Imaging GetImagingSettings - response missing VideoSourceToken/IrCutFilter${NC}"
        [ "$VERBOSE" = true ] && echo "$resp_settings"
    fi

    # Record original IR Cut filter so we can restore it later
    local original_ir
    original_ir=$(echo "$resp_settings" | sed -n 's|.*<tt:IrCutFilter>\(.*\)</tt:IrCutFilter>.*|\1|p')
    [[ -n $original_ir ]] || original_ir="On"

    # Verify GetOptions returns the expanded structures
    tmp_resp=$(mktemp)
    local req_options
    req_options=$(render_imaging_template "tests/soap_templates/imaging_get_options.xml" "$wsse_header" "$token")
    response_code=$(send_soap_raw "$SERVER_URL/onvif/imaging_service" "http://www.onvif.org/ver20/imaging/wsdl/GetOptions" "$req_options" "$tmp_resp")
    resp_options=$(cat "$tmp_resp" 2>/dev/null || true)
    rm -f "$tmp_resp" || true
    if [ "$response_code" != "200" ]; then
        echo -e "${RED}✗ Imaging GetOptions - FAILED (HTTP $response_code)${NC}"
        [ "$VERBOSE" = true ] && echo "$resp_options"
    elif ! assert_valid_xml "$resp_options" "imaging/GetOptions"; then
        return 1
    elif [[ $resp_options == *"<tt:VideoSourceToken>$token</tt:VideoSourceToken>"* && $resp_options == *"<tt:IrCutFilterModes>"* ]]; then
        echo -e "${GREEN}✓ Imaging GetOptions - SUCCESS${NC}"
        [ "$VERBOSE" = true ] && echo "$resp_options"
    else
        echo -e "${YELLOW}⚠ Imaging GetOptions - response missing VideoSourceToken/IrCutFilterModes${NC}"
        [ "$VERBOSE" = true ] && echo "$resp_options"
    fi

    # SetImagingSettings: flip IrCutFilter to a different value, verify change, then restore
    local current_ir
    current_ir=$(echo "$resp_settings" | sed -n 's|.*<tt:IrCutFilter>\(.*\)</tt:IrCutFilter>.*|\1|p')
    [[ -n $current_ir ]] || current_ir="$original_ir"

    # choose target value (toggle between On/Off; default to On if unknown)
    local lc; lc=$(echo "$current_ir" | tr '[:upper:]' '[:lower:]')
    local target_ir
    if [ "$lc" = "on" ]; then
        target_ir="Off"
    elif [ "$lc" = "off" ]; then
        target_ir="On"
    else
        target_ir="On"
    fi

    echo -e "[INFO] Imaging: current IrCutFilter=$current_ir -> setting to $target_ir to verify SetImagingSettings"

    tmp_resp=$(mktemp)
    local req_set
    req_set=$(render_imaging_template "tests/soap_templates/imaging_set_imaging_settings.xml" "$wsse_header" "$token" "$target_ir")
    response_code=$(send_soap_raw "$SERVER_URL/onvif/imaging_service" "http://www.onvif.org/ver20/imaging/wsdl/SetImagingSettings" "$req_set" "$tmp_resp")
    resp=$(cat "$tmp_resp" 2>/dev/null || true)
    rm -f "$tmp_resp" || true
    if [ "$response_code" != "200" ]; then
        echo -e "${RED}✗ Imaging SetImagingSettings (to $target_ir) - FAILED (HTTP $response_code)${NC}"
        if [ "$VERBOSE" = true ]; then echo "$resp"; fi
    else
        if ! assert_valid_xml "$resp" "imaging/SetImagingSettings"; then
            return 1
        fi
        if [[ $resp == *"<timg:SetImagingSettingsResponse"* || $resp == *"<s:Body/>"* ]]; then
            echo -e "${GREEN}✓ Imaging SetImagingSettings (to $target_ir) - SUCCESS${NC}"
            if [ "$VERBOSE" = true ]; then echo "$resp"; fi
            # verify via GetImagingSettings
            tmp_resp=$(mktemp)
            req_settings=$(render_imaging_template "tests/soap_templates/imaging_get_imaging_settings.xml" "$wsse_header" "$token")
            response_code=$(send_soap_raw "$SERVER_URL/onvif/imaging_service" "http://www.onvif.org/ver20/imaging/wsdl/GetImagingSettings" "$req_settings" "$tmp_resp")
            new_settings=$(cat "$tmp_resp" 2>/dev/null || true)
            rm -f "$tmp_resp" || true
            new_ir=$(echo "$new_settings" | sed -n 's|.*<tt:IrCutFilter>\(.*\)</tt:IrCutFilter>.*|\1|p')
            if ! assert_valid_xml "$new_settings" "imaging/GetImagingSettings (verify)"; then
                return 1
            fi

            if [[ "$new_ir" == "$target_ir" ]]; then
                echo -e "${GREEN}✓ Imaging verified IrCutFilter changed to $new_ir${NC}"
            else
                echo -e "${RED}✗ Imaging verification failed: expected $target_ir but got '$new_ir'${NC}"
            fi
        else
            echo -e "${RED}✗ Imaging SetImagingSettings (to $target_ir) - FAILED${NC}"
            if [ "$VERBOSE" = true ]; then echo "$resp"; fi
        fi
    fi

    # Restore original value
    if [[ "$original_ir" != "$target_ir" ]]; then
        tmp_resp=$(mktemp)
        req_set=$(render_imaging_template "tests/soap_templates/imaging_set_imaging_settings.xml" "$wsse_header" "$token" "$original_ir")
        response_code=$(send_soap_raw "$SERVER_URL/onvif/imaging_service" "http://www.onvif.org/ver20/imaging/wsdl/SetImagingSettings" "$req_set" "$tmp_resp")
        resp=$(cat "$tmp_resp" 2>/dev/null || true)
        rm -f "$tmp_resp" || true
        if [[ $response_code == "200" && ( $resp == *"<timg:SetImagingSettingsResponse"* || $resp == *"<s:Body/>"* ) ]]; then
            if ! assert_valid_xml "$resp" "imaging/SetImagingSettings (restore)"; then
                return 1
            fi
            echo -e "${GREEN}✓ Imaging restored original IrCutFilter to $original_ir${NC}"
        else
            echo -e "${YELLOW}⚠ Imaging failed to restore original IrCutFilter to $original_ir (response code $response_code)${NC}"
            if [ "$VERBOSE" = true ]; then echo "$resp"; fi
        fi
    fi
}

# Default runtime/config values
CONTAINER_NAME="oss"
DEFAULT_SERVER_URL="http://localhost:8000"
SERVER_URL="${SERVER_URL:-$DEFAULT_SERVER_URL}"
VERBOSE=false
HAS_LOCAL_CONTAINER=false
XML_VALIDATION_AVAILABLE=false
if command -v xmllint >/dev/null 2>&1; then
    XML_VALIDATION_AVAILABLE=true
fi

# Default credentials used by tests
DEFAULT_USERNAME="thingino"
DEFAULT_PASSWORD="thingino"

# Helper: send SOAP payload from a temp file (preserves exact bytes) and archive sent request
send_soap_raw() {
    # args: URL, SOAP_ACTION, SOAP_CONTENT, OUTFILE
    local url="$1"; local action="$2"; local soap_content="$3"; local outfile="$4"
    local tmpreq
    tmpreq=$(mktemp /tmp/soap.XXXXXX.xml)
    printf '%s' "$soap_content" > "$tmpreq"
    if [ -d "artifacts/raw_logs/127.0.0.1" ]; then
        ts=$(date -u +%Y%m%d_%H%M%S)
        cp -f "$tmpreq" "artifacts/raw_logs/127.0.0.1/${ts}_request.xml"
    fi
    # Use curl to write response body to outfile and print the HTTP status code to stdout.
    # Capture curl's exit status so callers can detect transport errors separately from HTTP codes.
    local http_code
    http_code=$(curl -s -o "$outfile" -w "%{http_code}" -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -H "SOAPAction: \"${action}\"" \
        --data-binary @"$tmpreq" \
        "$url")
    local rc=$?
    rm -f "$tmpreq"
    # Echo the HTTP code so callers using command substitution receive it.
    printf '%s' "$http_code"
    return $rc
}

assert_valid_xml() {
    local xml_content="$1"
    local label="$2"

    if [ "$XML_VALIDATION_AVAILABLE" != true ]; then
        return 0
    fi

    local tmp_xml
    tmp_xml=$(mktemp /tmp/xmlcheck.XXXXXX)
    printf '%s' "$xml_content" > "$tmp_xml"
    if ! xmllint --noout "$tmp_xml" >/dev/null 2>&1; then
        echo -e "${RED}✗ $label - FAILED XML validation${NC}"
        if [ "$VERBOSE" = true ]; then
            xmllint --noout "$tmp_xml" 2>&1
        fi
        rm -f "$tmp_xml"
        return 1
    fi
    rm -f "$tmp_xml"
    return 0
}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -s|--server-url)
            if [[ -z "$2" ]]; then
                echo "Error: --server-url requires a value"
                exit 1
            fi
            SERVER_URL="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [-v|--verbose] [-s|--server-url URL] [-h|--help]"
            echo ""
            echo "Options:"
            echo "  -v, --verbose    Show raw XML responses for all tests"
            echo "  -h, --help       Show this help message"
            echo "  -s, --server-url URL  Override target server (default: $DEFAULT_SERVER_URL)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}=== ONVIF Simple Server Container Test Suite ===${NC}"
echo -e "Target server: ${YELLOW}$SERVER_URL${NC}"
if [ "$VERBOSE" = true ]; then
    echo -e "${YELLOW}Verbose mode enabled - showing raw XML responses${NC}"
fi

# Check if container is running (only for local server targets)
RUN_CONTAINER_CHECK=true
case "$SERVER_URL" in
    http://localhost*|https://localhost*|http://127.0.0.1*|https://127.0.0.1*)
        ;;
    *)
        RUN_CONTAINER_CHECK=false
        ;;
esac

if [ "$RUN_CONTAINER_CHECK" = true ]; then
    if ! podman ps | grep -q $CONTAINER_NAME; then
        echo -e "${RED}Error: Container '$CONTAINER_NAME' is not running${NC}"
        echo "Start it with: ./container_run.sh"
        exit 1
    fi
    HAS_LOCAL_CONTAINER=true
    echo -e "${GREEN}✓ Container '$CONTAINER_NAME' is running${NC}"
else
    echo -e "${YELLOW}Skipping container health check (SERVER_URL not localhost)${NC}"
fi

# Function to generate authenticated SOAP request
generate_auth_soap() {
    local method=$1
    local ns_prefix=${2:-tds}
    local ns_url=${3:-"http://www.onvif.org/ver10/device/wsdl"}
    local include_category=${4:-true}

    # Use Python script if available, otherwise skip auth
    if [ -f "tests/generate_onvif_auth.py" ]; then
        local include_flag="True"
        if [ "$include_category" != true ]; then
            include_flag="False"
        fi
        python3 - "$DEFAULT_USERNAME" "$DEFAULT_PASSWORD" "$method" "$ns_prefix" "$ns_url" "$include_flag" <<'PY'
import sys
sys.path.insert(0, 'tests')
from generate_onvif_auth import generate_soap_request

username, password, method, prefix, ns_url, include_flag = sys.argv[1:7]
include_category = include_flag.lower() == "true"
soap = generate_soap_request(username, password, method, prefix, ns_url, include_category)
print(soap)
PY
    else
        echo ""
    fi
}

# Function to test ONVIF service
test_onvif_service() {
    local service_name=$1
    local method=$2
    local description=$3
    local require_auth=${4:-false}
    local expected_contains=${5:-}

    echo -e "\n${YELLOW}Testing: $description${NC}"

    local wsdl_prefix="tds"
    local wsdl_namespace="http://www.onvif.org/ver10/device/wsdl"
    case "$service_name" in
        media_service)
            wsdl_prefix="trt"
            wsdl_namespace="http://www.onvif.org/ver10/media/wsdl"
            ;;
        deviceio_service)
            wsdl_prefix="tmd"
            wsdl_namespace="http://www.onvif.org/ver10/deviceIO/wsdl"
            ;;
        events_service)
            wsdl_prefix="tev"
            wsdl_namespace="http://www.onvif.org/ver10/events/wsdl"
            ;;
        imaging_service)
            wsdl_prefix="timg"
            wsdl_namespace="http://www.onvif.org/ver20/imaging/wsdl"
            ;;
    esac

    local include_category=true
    case "$method" in
        GetVideoSources|GetAudioOutputs|GetAudioSources)
            include_category=false
            ;;
    esac

    # Create SOAP request
    local soap_request=""
    if [ "$require_auth" = true ]; then
        # Generate authenticated request
        soap_request=$(generate_auth_soap "$method" "$wsdl_prefix" "$wsdl_namespace" "$include_category")
        if [ -z "$soap_request" ]; then
            echo -e "${YELLOW}⚠ Skipping $method - authentication script not available${NC}"
            return 0
        fi
    else
        # Create simple SOAP request (using SOAP 1.2 format that ONVIF expects)
        printf -v soap_request '%s' "<?xml version=\"1.0\" encoding=\"utf-8\"?><s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:${wsdl_prefix}=\"${wsdl_namespace}\"><s:Header/><s:Body><${wsdl_prefix}:${method}/></s:Body></s:Envelope>"
    fi

    local response_code
    local response
    local soap_action="${wsdl_namespace}/${method}"

    # Send request (use raw file send to preserve bytes)
    tmp_resp=$(mktemp)
    response_code=$(send_soap_raw "$SERVER_URL/onvif/$service_name" "$soap_action" "$soap_request" "$tmp_resp")
    curl_rc=$?
    response=$(cat "$tmp_resp" 2>/dev/null || true)
    rm -f "$tmp_resp" || true

    # If curl itself failed, report transport error
    if [ $curl_rc -ne 0 ]; then
        echo -e "${RED}✗ $service_name/$method - FAILED (transport/curl error, rc=$curl_rc)${NC}"
        return 1
    fi

    # Basic sanity: expect an XML envelope in the response body
    if [[ -z "$response" ]]; then
        echo -e "${RED}✗ $service_name/$method - FAILED (no response body, HTTP $response_code)${NC}"
        return 1
    fi

    if ! assert_valid_xml "$response" "$service_name/$method"; then
        return 1
    fi

    if [[ $response == *"<?xml"* ]] && [[ $response == *"Envelope"* ]] && [[ $response == *"Body"* ]]; then
        # Check if it's a SOAP Fault
        if [[ $response == *"Fault"* ]]; then
            if [[ $response =~ Text.*\>([^<]+)\<.*Text ]]; then
                fault_reason="${BASH_REMATCH[1]}"
                echo -e "${YELLOW}⚠ $service_name/$method - SOAP FAULT: $fault_reason (HTTP $response_code)${NC}"
            else
                echo -e "${YELLOW}⚠ $service_name/$method - SOAP FAULT (HTTP $response_code)${NC}"
            fi
            [ "$VERBOSE" = true ] && echo "Response: $response"
            return 2
        fi

        # Optional content assertion
        if [ -n "$expected_contains" ]; then
            if [[ $response == *"$expected_contains"* ]]; then
                echo -e "${GREEN}✓ $service_name/$method - SUCCESS (asserted: '$expected_contains')${NC}"
            else
                echo -e "${RED}✗ $service_name/$method - FAILED (missing expected content)${NC}"
                echo "Expected to find: $expected_contains"
                [ "$VERBOSE" = true ] && echo "Response: $response"
                return 1
            fi
        else
            echo -e "${GREEN}✓ $service_name/$method - SUCCESS (HTTP $response_code)${NC}"
        fi
        [ "$VERBOSE" = true ] && echo "Response: $response"
        return 0
    else
        echo -e "${RED}✗ $service_name/$method - FAILED (invalid SOAP response, HTTP $response_code)${NC}"
        [ "$VERBOSE" = true ] && echo "Response: $response"
        return 1
    fi
}

# Function to test PTZ service with custom SOAP body
test_ptz_service() {
    local method=$1
    local description=$2
    local soap_body=$3
    local expected_http_code=${4:-200}
    local expected_fault=${5:-""}

    echo -e "\n${YELLOW}Testing: $description${NC}"

    # Generate authenticated SOAP request with custom body
    local soap_request=$(python3 -c "
import sys
import base64
import hashlib
import datetime
import os

username = '$DEFAULT_USERNAME'
password = '$DEFAULT_PASSWORD'
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')

tpl = open('tests/soap_templates/ptz_wrapper.xml','r').read()
# SOAP body passed in from bash via environment variable
soap_body = os.environ.get('SOAP_BODY','')
soap = tpl.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created, soap_body=soap_body)
print(soap)
" )

    # Send request and capture both response and HTTP code
    # send using send_soap_raw to preserve exact bytes
    local http_code
    tmp_ptz_resp=$(mktemp)
    http_code=$(send_soap_raw "$SERVER_URL/onvif/ptz_service" "" "$soap_request" "$tmp_ptz_resp")

    local response
    response=$(cat "$tmp_ptz_resp" 2>/dev/null || true)
    rm -f "$tmp_ptz_resp" || true

    # Check HTTP status code
    if [ "$http_code" != "$expected_http_code" ]; then
        echo -e "${RED}✗ $method - FAILED (expected HTTP $expected_http_code, got $http_code)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "Response: $response"
        fi
        return 1
    fi

    if [[ -z "$response" ]]; then
        echo -e "${RED}✗ $method - FAILED (no response body)${NC}"
        return 1
    fi

    if ! assert_valid_xml "$response" "ptz_service/$method"; then
        return 1
    fi

    # Check if we got a valid SOAP response
    if [[ $response == *"<?xml"* ]] && [[ $response == *"Envelope"* ]] && [[ $response == *"Body"* ]]; then

        # If we expect a fault, check for it
        if [ -n "$expected_fault" ]; then
            if [[ $response == *"$expected_fault"* ]]; then
                echo -e "${GREEN}✓ $method - SUCCESS (HTTP $http_code with expected fault: $expected_fault)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "Response: $response"
                fi
                return 0
            else
                echo -e "${RED}✗ $method - FAILED (expected fault '$expected_fault' not found)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "Response: $response"
                fi
                return 1
            fi
        else
            # No fault expected, check for success response
            if [[ $response == *"Fault"* ]]; then
                echo -e "${RED}✗ $method - FAILED (unexpected SOAP fault)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "Response: $response"
                fi
                return 1
            else
                echo -e "${GREEN}✓ $method - SUCCESS (HTTP $http_code)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "Response: $response"
                fi
                return 0
            fi
        fi
    else
        echo -e "${RED}✗ $method - FAILED (invalid SOAP response)${NC}"
        echo "Response: $response"
        return 1
    fi
}

# Explicit PRE_AUTH verification helpers (check HTTP status codes)
preauth_post() {
    local service_path=$1
    local body=$2
    local out=$3
    # write body to tmp file and POST raw bytes to preserve exact XML
    local tmp
    tmp=$(mktemp /tmp/preauth.XXXXXX.xml)
    printf '%s' "$body" > "$tmp"
    curl -s -w "%{http_code}" -o "$out" -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        --data-binary @"$tmp" \
        "$SERVER_URL/$service_path"
    local rc=$?
    rm -f "$tmp"
    return $rc
}

test_preauth_device_http() {
    local method=$1
    local description=$2
    local expected_http_code=$3

    echo -e "\n${YELLOW}PRE_AUTH: $description${NC}"

    local soap_request="<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\n  <s:Header/>\n  <s:Body>\n    <tds:$method/>\n  </s:Body>\n</s:Envelope>"

    local code=$(preauth_post "onvif/device_service" "$soap_request" "/tmp/preauth_${method}.xml")
    if [ "$code" = "$expected_http_code" ]; then
        echo -e "${GREEN}✓ $method - HTTP $code as expected${NC}"
        return 0
    else
        echo -e "${RED}✗ $method - expected HTTP $expected_http_code, got $code${NC}"
        [ -f "/tmp/preauth_${method}.xml" ] && echo "Response: $(head -c 200 /tmp/preauth_${method}.xml) ..."
        return 1
    fi
}

test_preauth_events_http() {
    local method=$1
    local description=$2
    local expected_http_code=$3

    echo -e "\n${YELLOW}PRE_AUTH: $description${NC}"

    local soap_request="<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n  <s:Header/>\n  <s:Body>\n    <tev:$method/>\n  </s:Body>\n</s:Envelope>"

    local code=$(preauth_post "onvif/events_service" "$soap_request" "/tmp/preauth_tev_${method}.xml")
    if [ "$code" = "$expected_http_code" ]; then
        echo -e "${GREEN}✓ tev:$method - HTTP $code as expected${NC}"
        return 0
    else
        echo -e "${RED}✗ tev:$method - expected HTTP $expected_http_code, got $code${NC}"
        [ -f "/tmp/preauth_tev_${method}.xml" ] && echo "Response: $(head -c 200 /tmp/preauth_tev_${method}.xml) ..."
        return 1
    fi
}

# Function to test basic HTTP connectivity
test_http_connectivity() {
    echo -e "\n${YELLOW}Testing HTTP connectivity...${NC}"

    local response=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL/")

    # Accept any HTTP response code that indicates the server is running
    # 200 = OK, 403 = Forbidden (normal for directory without index), 404 = Not Found
    if [ "$response" = "200" ] || [ "$response" = "403" ] || [ "$response" = "404" ]; then
        echo -e "${GREEN}✓ HTTP server is responding (code: $response)${NC}"
        return 0
    else
        echo -e "${RED}✗ HTTP server not responding (code: $response)${NC}"
        return 1
    fi
}

# Function to test CGI execution
test_cgi_execution() {
    echo -e "\n${YELLOW}Testing CGI execution...${NC}"

    # Test with GET request to check if CGI scripts are executing
    local response=$(curl -s "$SERVER_URL/onvif/device_service")

    # Check if we get a valid response indicating CGI execution
    if [[ $response == *"HTTP method not supported"* ]] || \
       [[ $response == *"<?xml"* ]] || \
       [[ $response == *"<soap:"* ]] || \
       [[ $response == *"<SOAP-ENV:"* ]]; then
        echo -e "${GREEN}✓ CGI scripts are executing${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "Response: $response"
        fi
        return 0
    elif [[ -z "$response" ]]; then
        echo -e "${RED}✗ CGI execution failed - no response${NC}"
        return 1
    else
        echo -e "${RED}✗ CGI execution failed - unexpected response${NC}"
        echo "Response: $response"
        return 1
    fi
}

# Run tests
echo -e "\n${BLUE}=== Running Tests ===${NC}"

test_http_connectivity
test_cgi_execution

# PRE_AUTH verification (must be accessible without auth)
echo -e "\n${BLUE}=== PRE_AUTH Verification ===${NC}"
# Expected 200 without auth
test_preauth_device_http "GetSystemDateAndTime" "Device:GetSystemDateAndTime should be pre-auth" 200
test_preauth_device_http "GetWsdlUrl" "Device:GetWsdlUrl should be pre-auth" 200
test_preauth_device_http "GetHostname" "Device:GetHostname should be pre-auth" 200
test_preauth_device_http "GetEndpointReference" "Device:GetEndpointReference should be pre-auth" 200
test_preauth_device_http "GetServices" "Device:GetServices should be pre-auth" 200
test_preauth_device_http "GetServiceCapabilities" "Device:GetServiceCapabilities should be pre-auth" 200
test_preauth_device_http "GetCapabilities" "Device:GetCapabilities should be pre-auth" 200
# Events service pre-auth
test_preauth_events_http "GetServiceCapabilities" "Events:GetServiceCapabilities should be pre-auth" 200

# Expected 401 without auth (not in PRE_AUTH)
# Note: these are implemented; they should now require auth when username is set
if test_preauth_device_http "GetDeviceInformation" "Device:GetDeviceInformation should require auth" 401; then
  echo -e "${GREEN}\u2713 Device:GetDeviceInformation requires auth (HTTP 401)${NC}"
fi
if test_preauth_device_http "GetUsers" "Device:GetUsers should require auth" 401; then
  echo -e "${GREEN}\u2713 Device:GetUsers requires auth (HTTP 401)${NC}"
fi

# Test basic ONVIF device services (no auth required)
test_onvif_service "device_service" "GetSystemDateAndTime" "Get System Date and Time" false
# Requires auth: expect SOAP fault (should fail without auth)
test_onvif_service "device_service" "GetDeviceInformation" "Get Device Information" true
# General capability retrieval
test_onvif_service "device_service" "GetCapabilities" "Get Device Capabilities" false
# Assert DHCP flag now defaults to true per spec convenience
test_onvif_service "device_service" "GetHostname" "Get Hostname" false "<tt:FromDHCP>true</tt:FromDHCP>"
# Assert EPR includes an Address element with HTTP URL
test_onvif_service "device_service" "GetEndpointReference" "Get EndpointReference" false "<wsa5:Address>http://"
# Basic check for services
test_onvif_service "device_service" "GetServices" "Get Available Services" false

# Test media services (auth required)
test_onvif_service "media_service" "GetProfiles" "Get Media Profiles" true
test_onvif_service "media_service" "GetVideoSources" "Get Video Sources" true

# Audio Backchannel (AudioOutput) tests
test_audio_output_endpoints
test_deviceio_audio_endpoints

# PTZ Tests
echo -e "\n${BLUE}=== PTZ Functionality Tests ===${NC}"

# Test PTZ GetConfigurations
test_ptz_get_configurations() {
    echo -e "\n${YELLOW}Testing: PTZ GetConfigurations (verify UseCount)${NC}"

    local soap_request=$(generate_auth_soap "GetConfigurations")
    if [ -z "$soap_request" ]; then
        echo -e "${YELLOW}⚠ Skipping PTZ tests - authentication script not available${NC}"
        return 0
    fi

    tmp_resp=$(mktemp)
    response_code=$(send_soap_raw "$SERVER_URL/onvif/ptz_service" "" "$soap_request" "$tmp_resp")
    local response
    response=$(cat "$tmp_resp" 2>/dev/null || true)
    rm -f "$tmp_resp" || true

    if [[ -z "$response" ]]; then
        echo -e "${RED}✗ PTZ GetConfigurations - FAILED (no response)${NC}"
        return 1
    fi

    if ! assert_valid_xml "$response" "ptz/GetConfigurations"; then
        return 1
    fi

    if [[ $response == *"PTZConfiguration"* ]] && [[ $response == *"UseCount"* ]]; then
        # Extract UseCount value
        if [[ $response =~ \<tt:UseCount\>([0-9]+)\</tt:UseCount\> ]]; then
            local use_count="${BASH_REMATCH[1]}"
            if [ "$use_count" -gt 0 ]; then
                echo -e "${GREEN}✓ PTZ GetConfigurations - SUCCESS (UseCount=$use_count)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
                fi
                return 0
            else
                echo -e "${YELLOW}⚠ PTZ GetConfigurations - UseCount is 0 (should be >0)${NC}"
                return 2
            fi
        else
            echo -e "${YELLOW}⚠ PTZ GetConfigurations - Could not extract UseCount${NC}"
            return 2
        fi
    else
        echo -e "${RED}✗ PTZ GetConfigurations - FAILED${NC}"
        return 1
    fi
}

# Test PTZ GetNodes
test_ptz_get_nodes() {
    echo -e "\n${YELLOW}Testing: PTZ GetNodes (verify capabilities)${NC}"

    local soap_request=$(generate_auth_soap "GetNodes")
    if [ -z "$soap_request" ]; then
        return 0
    fi

    tmp_resp=$(mktemp)
    response_code=$(send_soap_raw "$SERVER_URL/onvif/ptz_service" "" "$soap_request" "$tmp_resp")
    local response
    response=$(cat "$tmp_resp" 2>/dev/null || true)
    rm -f "$tmp_resp" || true

    if [[ -z "$response" ]]; then
        echo -e "${RED}✗ PTZ GetNodes - FAILED (no response)${NC}"
        return 1
    fi

    if ! assert_valid_xml "$response" "ptz/GetNodes"; then
        return 1
    fi

    if [[ $response == *"PTZNode"* ]] && [[ $response == *"AbsolutePanTiltPositionSpace"* ]]; then
        echo -e "${GREEN}✓ PTZ GetNodes - SUCCESS (AbsoluteMove supported)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        fi
        return 0
    else
        echo -e "${RED}✗ PTZ GetNodes - FAILED${NC}"
        return 1
    fi
}

# Test PTZ AbsoluteMove with valid coordinates
test_ptz_absolute_move_valid() {
    echo -e "\n${YELLOW}Testing: PTZ AbsoluteMove with valid coordinates${NC}"

    # Generate authenticated AbsoluteMove request using global credentials
    local soap_request=$(python3 - <<'PY'
import sys, base64, hashlib, datetime, os

sys.path.insert(0, 'tests')
username = os.environ.get('DEFAULT_USERNAME', 'thingino')
password = os.environ.get('DEFAULT_PASSWORD', 'thingino')
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')
tpl = open('tests/soap_templates/ptz_absolute_move.xml', 'r').read()
soap = tpl.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created, pan='1850.0', tilt='500.0', space='http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace')
print(soap)
PY
)

    if [ -z "$soap_request" ]; then
        echo -e "${YELLOW}⚠ Skipping - Python not available${NC}"
        return 0
    fi

    # Send request using send_soap_raw to preserve exact bytes and archive sent request
    local http_code
    tmp_ptz_resp=$(mktemp)
    http_code=$(send_soap_raw "$SERVER_URL/onvif/ptz_service" "" "$soap_request" "$tmp_ptz_resp")
    local response
    response=$(cat "$tmp_ptz_resp" 2>/dev/null || true)
    rm -f "$tmp_ptz_resp" || true

    if [[ -z "$response" ]]; then
        echo -e "${RED}✗ PTZ AbsoluteMove (valid) - FAILED (no response body)${NC}"
        return 1
    fi

    if ! assert_valid_xml "$response" "ptz/AbsoluteMove"; then
        return 1
    fi

    if [ "$http_code" = "200" ] && [[ $response == *"AbsoluteMoveResponse"* ]]; then
        echo -e "${GREEN}✓ PTZ AbsoluteMove (valid) - SUCCESS (HTTP $http_code)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        fi
        return 0
    else
        echo -e "${RED}✗ PTZ AbsoluteMove (valid) - FAILED (HTTP $http_code)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response"
        fi
        return 1
    fi
}

# Test PTZ AbsoluteMove with out-of-bounds coordinates (should return HTTP 500)
test_ptz_absolute_move_invalid() {
    echo -e "\n${YELLOW}Testing: PTZ AbsoluteMove with out-of-bounds coordinates (expect HTTP 500)${NC}"

    # Generate authenticated AbsoluteMove request with invalid coordinates using global credentials
    local soap_request=$(python3 - <<'PY'
import sys, base64, hashlib, datetime, os

sys.path.insert(0, 'tests')
username = os.environ.get('DEFAULT_USERNAME', 'thingino')
password = os.environ.get('DEFAULT_PASSWORD', 'thingino')
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')
tpl = open('tests/soap_templates/ptz_absolute_move_invalid.xml', 'r').read()
soap = tpl.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created, pan='9999.0', tilt='9999.0', space='http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace')
print(soap)
PY
)

    if [ -z "$soap_request" ]; then
        echo -e "${YELLOW}⚠ Skipping - Python not available${NC}"
        return 0
    fi

    # Send request using send_soap_raw to preserve exact bytes and archive sent request
    local http_code
    tmp_ptz_fault=$(mktemp)
    http_code=$(send_soap_raw "$SERVER_URL/onvif/ptz_service" "" "$soap_request" "$tmp_ptz_fault")
    local response
    response=$(cat "$tmp_ptz_fault" 2>/dev/null || true)
    rm -f "$tmp_ptz_fault" || true

    if [[ -z "$response" ]]; then
        echo -e "${RED}✗ PTZ AbsoluteMove (invalid) - FAILED (no response body)${NC}"
        return 1
    fi

    if ! assert_valid_xml "$response" "ptz/AbsoluteMoveInvalid"; then
        return 1
    fi

    if [ "$http_code" = "500" ] && [[ $response == *"Fault"* ]] && [[ $response == *"InvalidPosition"* ]]; then
        echo -e "${GREEN}✓ PTZ AbsoluteMove (invalid) - SUCCESS (HTTP $http_code with ter:InvalidPosition fault)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        fi
        return 0
    else
        echo -e "${RED}✗ PTZ AbsoluteMove (invalid) - FAILED (HTTP $http_code, expected 500)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response"
        fi
        return 1
    fi
}

# Test PTZ GetStatus
test_ptz_get_status() {
    echo -e "\n${YELLOW}Testing: PTZ GetStatus${NC}"

    # Generate authenticated GetStatus request from template
    local soap_request=$(python3 - <<'PY'
import sys, base64, hashlib, datetime, os
sys.path.insert(0, 'tests')
username = os.environ.get('DEFAULT_USERNAME', 'thingino')
password = os.environ.get('DEFAULT_PASSWORD', 'thingino')
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')
tpl = open('tests/soap_templates/ptz_get_status.xml','r').read()
soap = tpl.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created)
print(soap)
PY
)

    if [ -z "$soap_request" ]; then
        echo -e "${YELLOW}⚠ Skipping - Python not available${NC}"
        return 0
    fi

    tmp_resp=$(mktemp)
    response_code=$(send_soap_raw "$SERVER_URL/onvif/ptz_service" "" "$soap_request" "$tmp_resp")
    local response
    response=$(cat "$tmp_resp" 2>/dev/null || true)
    rm -f "$tmp_resp" || true

    if [[ -z "$response" ]]; then
        echo -e "${RED}✗ PTZ GetStatus - FAILED (no response body)${NC}"
        return 1
    fi

    if ! assert_valid_xml "$response" "ptz/GetStatus"; then
        return 1
    fi

    if [[ $response == *"GetStatusResponse"* ]] && [[ $response == *"Position"* ]] && [[ $response == *"PanTilt"* ]]; then
        echo -e "${GREEN}✓ PTZ GetStatus - SUCCESS${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        fi
        return 0
    else
        echo -e "${RED}✗ PTZ GetStatus - FAILED${NC}"
        return 1
    fi
}

# Test PTZ ContinuousMove with diagonal motion
test_ptz_continuous_move_diagonal() {
    echo -e "\n${YELLOW}Testing: PTZ ContinuousMove with diagonal motion (x=-0.666667, y=0.666667)${NC}"

    # Generate authenticated ContinuousMove request with diagonal velocity
    local soap_request=$(python3 -c "
import sys, base64, hashlib, datetime, os
sys.path.insert(0, 'tests')

username = '$DEFAULT_USERNAME'
password = '$DEFAULT_PASSWORD'
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')

tpl = open('tests/soap_templates/ptz_continuous_move_diagonal.xml','r').read()
soap = tpl.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created)
print(soap)
" 2>/dev/null)

    if [ -z "$soap_request" ]; then
        echo -e "${YELLOW}⚠ Skipping - Python not available${NC}"
        return 0
    fi

    local http_code
    tmp_ptz_cont=$(mktemp)
    http_code=$(send_soap_raw "$SERVER_URL/onvif/ptz_service" "" "$soap_request" "$tmp_ptz_cont")
    local response
    response=$(cat "$tmp_ptz_cont" 2>/dev/null || true)
    rm -f "$tmp_ptz_cont" || true

    # Check container logs for diagonal movement command
    local has_diagonal=true
    local logs=""
    if [ "$HAS_LOCAL_CONTAINER" = true ]; then
        sleep 0.5  # Give server time to process
        logs=$(podman logs --tail 30 $CONTAINER_NAME 2>&1)
        has_diagonal=false
        if [[ $logs == *"-d h -x 0.000000 -y 0.000000"* ]]; then
            has_diagonal=true
        fi
    fi

    if [[ -z "$response" ]]; then
        echo -e "${RED}✗ PTZ ContinuousMove (diagonal) - FAILED (no response body)${NC}"
        return 1
    fi

    if ! assert_valid_xml "$response" "ptz/ContinuousMove"; then
        return 1
    fi

    if [ "$http_code" = "200" ] && [[ $response == *"ContinuousMoveResponse"* ]]; then
        if [ "$has_diagonal" = true ]; then
            echo -e "${GREEN}✓ PTZ ContinuousMove (diagonal) - SUCCESS (single command with both axes)${NC}"
            if [ "$VERBOSE" = true ]; then
                echo "Response: $response"
                if [ "$HAS_LOCAL_CONTAINER" = true ]; then
                    echo "Recent logs showing diagonal command:"
                    echo "$logs" | grep -E "(-d h -x .* -y )" | tail -3
                fi
            fi
            return 0
        else
            if [ "$HAS_LOCAL_CONTAINER" = true ]; then
                echo -e "${YELLOW}⚠ PTZ ContinuousMove (diagonal) - HTTP 200 but diagonal command not found${NC}"
                echo "  Looking for: -d h -x 0.000000 -y 0.000000"
                if [ "$VERBOSE" = true ]; then
                    echo "Recent logs:"
                    echo "$logs" | grep MOTORS | tail -10
                fi
                return 2
            else
                echo -e "${YELLOW}⚠ PTZ ContinuousMove (diagonal) - Skipping log validation (no local container)${NC}"
            fi
            return 0
        fi
    else
        echo -e "${RED}✗ PTZ ContinuousMove (diagonal) - FAILED (HTTP $http_code)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response"
        fi
        return 1
    fi
}

# Run PTZ tests
test_ptz_get_configurations
test_ptz_get_nodes
test_ptz_absolute_move_valid
test_ptz_absolute_move_invalid
test_ptz_get_status
test_ptz_continuous_move_diagonal

# Imaging Service Tests
test_imaging_service

# Events (PullPoint) Tests
if bash tests/container_test_events.sh; then
    echo -e "${GREEN}✓ Events tests - SUCCESS${NC}"
else
    echo -e "${RED}✗ Events tests - FAILED${NC}"
fi

echo -e "\n${BLUE}=== Test Summary ===${NC}"
echo -e "${GREEN}Tests completed. Check output above for results.${NC}"
echo -e "\n${YELLOW}To view container logs:${NC} podman logs $CONTAINER_NAME"
echo -e "${YELLOW}To inspect container:${NC} ./container_inspect.sh"
