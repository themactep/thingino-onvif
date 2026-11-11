#!/usr/bin/env python3
"""Minimal WS-Discovery client for the container test round.

Default: send an ONVIF Home Assistant style Probe and print ProbeMatches.
--resolve EPR: send a Resolve and print ResolveMatches.
--expect-none: succeed only when no response arrives.
"""

import argparse
import socket
import sys
import time
import uuid

WSD_PORT = 3702
WSD_ADDR = "239.255.255.250"
LOOPBACK = "127.0.0.1"
DISCOVERY_NS = "http://schemas.xmlsoap.org/ws/2005/04/discovery"


def envelope(action, message_id, body, to="urn:schemas-xmlsoap-org:ws:2005:04:discovery"):
    return (
        '<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" '
        'xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" '
        f'xmlns:d="{DISCOVERY_NS}" '
        'xmlns:tdn="http://www.onvif.org/ver10/network/wsdl" '
        'xmlns:tds="http://www.onvif.org/ver10/device/wsdl">'
        f'<s:Header><a:Action>{action}</a:Action>'
        f'<a:MessageID>{message_id}</a:MessageID>'
        f'<a:To>{to}</a:To></s:Header>'
        f'<s:Body>{body}</s:Body></s:Envelope>'
    )


def probe_message(message_id, types, scopes, match_by):
    types_xml = f"<d:Types>{types}</d:Types>" if types else ""
    scopes_xml = (
        f'<d:Scopes MatchBy="{match_by}">{scopes}</d:Scopes>' if scopes else ""
    )
    return envelope(
        "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe",
        message_id,
        f"<d:Probe>{types_xml}{scopes_xml}</d:Probe>",
    )


def resolve_message(message_id, endpoint):
    return envelope(
        "http://schemas.xmlsoap.org/ws/2005/04/discovery/Resolve",
        message_id,
        "<d:Resolve><a:EndpointReference>"
        f"<a:Address>{endpoint}</a:Address>"
        "</a:EndpointReference></d:Resolve>",
    )


def send(payload, destination, timeout, retries):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    try:
        for _ in range(retries):
            sock.sendto(payload.encode(), (destination, WSD_PORT))
            try:
                data, _ = sock.recvfrom(65535)
                return data.decode("utf-8", "replace")
            except socket.timeout:
                time.sleep(0.2)
        return None
    finally:
        sock.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--resolve", metavar="EPR")
    parser.add_argument("--types", default="tdn:NetworkVideoTransmitter")
    parser.add_argument("--scopes", default="onvif://www.onvif.org/Profile/Streaming")
    parser.add_argument(
        "--match-by",
        default="http://schemas.xmlsoap.org/ws/2005/04/discovery/rfc3986",
    )
    parser.add_argument("--unicast", action="store_true", help="send to 127.0.0.1 instead of multicast")
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--retries", type=int, default=3)
    parser.add_argument("--expect-none", action="store_true")
    args = parser.parse_args()

    message_id = f"urn:uuid:{uuid.uuid4()}"
    if args.resolve:
        payload = resolve_message(message_id, args.resolve)
    else:
        payload = probe_message(message_id, args.types, args.scopes, args.match_by)

    destination = LOOPBACK if args.unicast else WSD_ADDR
    response = send(payload, destination, args.timeout, args.retries)
    if args.expect_none:
        return 0 if response is None else 1
    if response is None:
        return 1
    print(response)
    return 0


if __name__ == "__main__":
    sys.exit(main())
