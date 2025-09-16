#!/usr/bin/env python3

import base64
import hashlib
import datetime
import os

def generate_ws_security_digest(username, password, nonce_b64=None, created=None):
    """
    Generate WS-Security digest for ONVIF authentication
    Digest = Base64(SHA1(Nonce + Created + Password))
    """
    
    # Generate nonce if not provided
    if nonce_b64 is None:
        nonce = os.urandom(16)
        nonce_b64 = base64.b64encode(nonce).decode('utf-8')
    else:
        nonce = base64.b64decode(nonce_b64)
    
    # Generate created timestamp if not provided
    if created is None:
        created = datetime.datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')
    
    # Calculate digest: SHA1(Nonce + Created + Password)
    digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
    digest = hashlib.sha1(digest_input).digest()
    digest_b64 = base64.b64encode(digest).decode('utf-8')
    
    return {
        'username': username,
        'password_digest': digest_b64,
        'nonce': nonce_b64,
        'created': created
    }

def generate_soap_request(username, password, method='GetCapabilities'):
    """Generate SOAP request with proper WS-Security authentication"""
    
    # Use fixed values for reproducible testing
    nonce_b64 = "LKqI6G/AikKCQrN0zqZFlg=="
    created = "2024-01-01T00:00:00Z"
    
    auth = generate_ws_security_digest(username, password, nonce_b64, created)
    
    soap_template = '''<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl" xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">
    <SOAP-ENV:Header>
        <wsse:Security SOAP-ENV:mustUnderstand="true">
            <wsse:UsernameToken wsu:Id="UsernameToken-1">
                <wsse:Username>{username}</wsse:Username>
                <wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">{password_digest}</wsse:Password>
                <wsse:Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">{nonce}</wsse:Nonce>
                <wsu:Created>{created}</wsu:Created>
            </wsse:UsernameToken>
        </wsse:Security>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
        <tds:{method}>
            <tds:Category>All</tds:Category>
        </tds:{method}>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'''
    
    return soap_template.format(
        username=auth['username'],
        password_digest=auth['password_digest'],
        nonce=auth['nonce'],
        created=auth['created'],
        method=method
    )

if __name__ == "__main__":
    # Test with thingino credentials
    username = "thingino"
    password = "thingino"
    
    print("ONVIF WS-Security Authentication Generator")
    print("==========================================")
    print(f"Username: {username}")
    print(f"Password: {password}")
    print()
    
    # Generate authentication
    auth = generate_ws_security_digest(username, password, "LKqI6G/AikKCQrN0zqZFlg==", "2024-01-01T00:00:00Z")
    
    print("Generated Authentication:")
    print(f"  Nonce: {auth['nonce']}")
    print(f"  Created: {auth['created']}")
    print(f"  Password Digest: {auth['password_digest']}")
    print()
    
    # Generate SOAP request
    soap_request = generate_soap_request(username, password)
    print("Generated SOAP Request:")
    print("=" * 50)
    print(soap_request)
    print("=" * 50)
    print()
    print(f"Content-Length: {len(soap_request)}")
