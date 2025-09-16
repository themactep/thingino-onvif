# WS-Discovery Debug Logging Examples

With the enhanced logging, you'll now see comprehensive IP address information for all WS-Discovery traffic.

## Example 1: Successful Probe Request and Response

```
level=DEBUG file=wsd_simple_server.c line=550 msg="Received WS-Discovery message from 192.168.1.100:3702"
level=DEBUG file=wsd_simple_server.c line=556 msg="Processing request from 192.168.1.100:3702"
level=DEBUG file=wsd_simple_server.c line=557 msg="Request content: <?xml version="1.0" encoding="utf-8"?><Envelope xmlns:tds="http://www.onvif.org/ver10/device/wsdl"..."
level=DEBUG file=wsd_simple_server.c line=566 msg="Accepted probe message from 192.168.1.100:3702 (method: Probe)"
level=INFO file=wsd_simple_server.c line=582 msg="Sending ProbeMatches message."
level=DEBUG file=wsd_simple_server.c line=627 msg="ProbeMatches response: <?xml version="1.0" encoding="UTF-8"?><SOAP-ENV:Envelope..."
level=INFO file=wsd_simple_server.c line=636 msg="ProbeMatches sent to 192.168.1.100:3702"
```

## Example 2: UniviewProbe Request (Now Accepted)

```
level=DEBUG file=wsd_simple_server.c line=550 msg="Received WS-Discovery message from 192.168.1.200:3702"
level=DEBUG file=wsd_simple_server.c line=556 msg="Processing request from 192.168.1.200:3702"
level=DEBUG file=wsd_simple_server.c line=557 msg="Request content: <?xml version="1.0" encoding="utf-8"?><SOAP-ENV:Envelope..."
level=DEBUG file=wsd_simple_server.c line=566 msg="Accepted probe message from 192.168.1.200:3702 (method: UniviewProbe)"
level=INFO file=wsd_simple_server.c line=582 msg="Sending ProbeMatches message."
level=DEBUG file=wsd_simple_server.c line=627 msg="ProbeMatches response: <?xml version="1.0" encoding="UTF-8"?><SOAP-ENV:Envelope..."
level=INFO file=wsd_simple_server.c line=636 msg="ProbeMatches sent to 192.168.1.200:3702"
```

## Example 3: Rejected Non-Probe Message

```
level=DEBUG file=wsd_simple_server.c line=550 msg="Received WS-Discovery message from 192.168.1.150:3702"
level=DEBUG file=wsd_simple_server.c line=556 msg="Processing request from 192.168.1.150:3702"
level=DEBUG file=wsd_simple_server.c line=557 msg="Request content: <?xml version="1.0" encoding="utf-8"?><SOAP-ENV:Envelope..."
level=DEBUG file=wsd_simple_server.c line=563 msg="Rejected non-probe message from 192.168.1.150:3702 (method: Hello)"
```

## Example 4: Response Message Received

```
level=DEBUG file=wsd_simple_server.c line=550 msg="Received WS-Discovery message from 192.168.1.75:3702"
level=DEBUG file=wsd_simple_server.c line=639 msg="Received response message from 192.168.1.75:3702 (ignoring)"
```

## Example 5: Send Error

```
level=DEBUG file=wsd_simple_server.c line=550 msg="Received WS-Discovery message from 192.168.1.100:3702"
level=DEBUG file=wsd_simple_server.c line=556 msg="Processing request from 192.168.1.100:3702"
level=DEBUG file=wsd_simple_server.c line=566 msg="Accepted probe message from 192.168.1.100:3702 (method: Probe)"
level=INFO file=wsd_simple_server.c line=582 msg="Sending ProbeMatches message."
level=ERROR file=wsd_simple_server.c line=630 msg="Error sending ProbeMatches message to 192.168.1.100:3702"
```

## Benefits

1. **Network Troubleshooting**: See exactly which IP addresses are sending requests
2. **Device Identification**: Track different types of devices by their probe message types
3. **Response Tracking**: Confirm which devices receive responses
4. **Error Diagnosis**: Identify network issues with specific IP addresses
5. **Traffic Analysis**: Monitor WS-Discovery traffic patterns on your network
