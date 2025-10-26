#!/usr/bin/env bash
# Common WS-Security UsernameToken Digest helper for ONVIF SOAP requests
# Usage:
#   WSSE=$(onvif_wsse_header "$USER" "$PASS")
# Prints an XML <wsse:Security> block to stdout.
set -euo pipefail

onvif_wsse_header() {
  local USERNAME=${1:-}
  local PASSWORD=${2:-}
  if [[ -z "$USERNAME" || -z "$PASSWORD" ]]; then
    echo "onvif_wsse_header: missing USER or PASS" >&2
    return 2
  fi
  local NONCE_BIN NONCE_B64 CREATED DIGEST NF CF
  NONCE_BIN=$(mktemp)
  head -c 16 /dev/urandom > "$NONCE_BIN"
  NONCE_B64=$(base64 -w0 "$NONCE_BIN" 2>/dev/null || base64 "$NONCE_BIN")
  CREATED=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  NF="$NONCE_BIN"
  CF=$(mktemp)
  printf "%s" "$CREATED$PASSWORD" > "$CF"
  DIGEST=$(cat "$NF" "$CF" | openssl sha1 -binary | base64 -w0)
  rm -f "$NF" "$CF"
  cat <<WSSE
<wsse:Security s:mustUnderstand="1" xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">
  <wsse:UsernameToken>
    <wsse:Username>${USERNAME}</wsse:Username>
    <wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">${DIGEST}</wsse:Password>
    <wsse:Nonce>${NONCE_B64}</wsse:Nonce>
    <wsu:Created>${CREATED}</wsu:Created>
  </wsse:UsernameToken>
</wsse:Security>
WSSE
}

onvif_curl_post() {
  # onvif_curl_post URL SOAP_ACTION SOAP_BODY
  local URL=${1:?} ACTION=${2:?}
  shift 2 || true
  curl -sS -X POST \
    -H "Content-Type: application/soap+xml; charset=utf-8" \
    -H "SOAPAction: \"${ACTION}\"" \
    --data "$*" \
    "$URL"
}

