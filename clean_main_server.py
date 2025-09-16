#!/usr/bin/env python3

import re

# Read the original file
with open('src/onvif_simple_server.c', 'r') as f:
    content = f.read()

# Find the start of the nested if/else mess (after the dispatch call we added)
start_pattern = r'(log_debug\("DEBUG: Dispatch completed with result=%d", dispatch_result\);\s*log_debug\("DEBUG: Finished processing authenticated request"\);\s*)'
end_pattern = r'(\s*} else {\s*log_debug\("DEBUG: Authentication failed, sending auth error"\);)'

# Find the positions
start_match = re.search(start_pattern, content, re.DOTALL)
end_match = re.search(end_pattern, content, re.DOTALL)

if start_match and end_match:
    # Keep everything before the nested mess and after it
    before = content[:start_match.end()]
    after = content[end_match.start():]
    
    # Combine with clean transition
    clean_content = before + "\n" + after
    
    # Write the cleaned file
    with open('src/onvif_simple_server.c', 'w') as f:
        f.write(clean_content)
    
    print("Successfully cleaned the nested if/else mess!")
    print(f"Removed {len(content) - len(clean_content)} characters")
else:
    print("Could not find the pattern to clean")
    if not start_match:
        print("Start pattern not found")
    if not end_match:
        print("End pattern not found")
