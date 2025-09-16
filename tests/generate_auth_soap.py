#!/usr/bin/env python3
"""
Generate ONVIF SOAP requests with proper WS-Security authentication.
Creates PasswordDigest according to ONVIF specification.
"""

import base64
import hashlib
import os
import random
import string
from datetime import datetime, timezone

def generate_nonce(length=16):
    """Generate a random nonce."""
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def generate_timestamp():
    """Generate ISO 8601 timestamp."""
    return datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%S.%fZ')

def calculate_password_digest(nonce, timestamp, password):
    """Calculate password digest: Base64(SHA1(nonce + timestamp + password))."""
    # Decode nonce from base64 if it's already encoded
    try:
        nonce_bytes = base64.b64decode(nonce)
    except:
        nonce_bytes = nonce.encode('utf-8')
    
    # Combine nonce + timestamp + password
    combined = nonce_bytes + timestamp.encode('utf-8') + password.encode('utf-8')
    
    # Calculate SHA1 hash
    sha1_hash = hashlib.sha1(combined).digest()
    
    # Encode as base64
    return base64.b64encode(sha1_hash).decode('utf-8')

def create_authenticated_soap(template_file, output_file, username="admin", password="admin"):
    """Create authenticated SOAP request from template."""
    
    # Generate authentication components
    nonce = generate_nonce()
    nonce_b64 = base64.b64encode(nonce.encode('utf-8')).decode('utf-8')
    timestamp = generate_timestamp()
    digest = calculate_password_digest(nonce_b64, timestamp, password)
    
    # Read template
    with open(template_file, 'r') as f:
        content = f.read()
    
    # Replace placeholders
    content = content.replace('DIGEST_PLACEHOLDER', digest)
    content = content.replace('NONCE_PLACEHOLDER', nonce_b64)
    content = content.replace('TIMESTAMP_PLACEHOLDER', timestamp)
    
    # Write output
    with open(output_file, 'w') as f:
        f.write(content)
    
    print(f"Generated authenticated SOAP request: {output_file}")
    print(f"  Username: {username}")
    print(f"  Nonce: {nonce_b64}")
    print(f"  Timestamp: {timestamp}")
    print(f"  Digest: {digest}")

if __name__ == "__main__":
    # Test with the device information request
    create_authenticated_soap(
        "requests/device_getdeviceinformation_auth.xml",
        "requests/device_getdeviceinformation_authenticated.xml"
    )
