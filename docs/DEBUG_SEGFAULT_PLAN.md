# Debug Plan for Events Service Segfault

## **🔍 Debugging Strategy**

Since you can compile strace and gdbserver, we can get much more detailed information about the crash.

## **Step 1: Strace Analysis**

### **Compile and Run with Strace**
```bash
# First, build the current version with our cat() fixes
./build.sh

# Run the events_service under strace to capture system calls
strace -f -o /tmp/events_strace.log -e trace=all onvif_simple_server

# Or if you want to attach to an existing process:
# Find the PID of events_service when it's running
ps aux | grep events_service
strace -f -p <PID> -o /tmp/events_strace.log
```

### **What to Look For in Strace Output**
1. **Memory access patterns** before the crash
2. **System calls** that fail or return unexpected values
3. **File operations** that might be failing
4. **Memory allocation/deallocation** patterns
5. **The exact sequence** leading to the segfault

## **Step 2: GDB Analysis (if needed)**

### **Compile Debug Version**
```bash
# Build debug version with symbols
make clean
CFLAGS="-g -O0 -DDEBUG" ./build.sh

# Or modify build.sh to include debug flags
```

### **Run with GDBServer**
```bash
# Start gdbserver
gdbserver :1234 onvif_simple_server

# From another terminal or remote machine:
gdb onvif_simple_server
(gdb) target remote :1234
(gdb) continue
```

### **GDB Commands for Analysis**
```gdb
# Set breakpoints
break cat
break events_pull_messages
break events_get_event_properties

# When crash occurs:
bt                    # Backtrace
info registers        # Register state
x/20x $sp            # Stack contents
print par_to_sub     # Variable values
print par_to_find    # Variable values
```

## **Step 3: Immediate Strace Analysis**

### **Key Areas to Examine**

1. **Memory Operations**:
   ```
   mmap/munmap calls
   brk/sbrk calls
   Memory protection changes
   ```

2. **File Operations**:
   ```
   open/close calls for XML templates
   read operations on template files
   File descriptor leaks
   ```

3. **Process/Thread Operations**:
   ```
   clone/fork calls
   execve calls
   Signal handling
   ```

4. **Network Operations**:
   ```
   socket operations
   accept/connect calls
   HTTP request processing
   ```

## **Step 4: Specific Debugging Commands**

### **Capture Full Strace**
```bash
# Comprehensive strace with timestamps
strace -f -tt -T -o /tmp/events_debug.log -e trace=all onvif_simple_server

# Focus on memory and file operations
strace -f -tt -T -o /tmp/events_memory.log -e trace=mmap,munmap,brk,sbrk,read,write,open,close onvif_simple_server

# Focus on signals and crashes
strace -f -tt -T -o /tmp/events_signals.log -e trace=signal,rt_sigaction,rt_sigprocmask onvif_simple_server
```

### **Monitor Specific Process**
```bash
# If events_service runs as CGI, monitor the web server
ps aux | grep -E "(httpd|nginx|lighttpd|onvif)"

# Monitor all ONVIF processes
pgrep -f onvif | xargs -I {} strace -f -p {} -o /tmp/onvif_{}.log
```

## **Step 5: Analysis Checklist**

### **In Strace Output, Look For**:
- [ ] **Last successful system calls** before crash
- [ ] **Failed system calls** (return -1, EFAULT, EINVAL)
- [ ] **Memory allocation patterns** (mmap, brk)
- [ ] **File access patterns** (open, read, close)
- [ ] **Signal delivery** (SIGSEGV details)
- [ ] **Process creation** (clone, execve)

### **Common Patterns to Identify**:
1. **NULL pointer dereference**: Access to address 0x0
2. **Buffer overflow**: Writing beyond allocated memory
3. **Use-after-free**: Accessing freed memory
4. **Double-free**: Freeing same memory twice
5. **Stack overflow**: Excessive recursion or large local variables

## **Step 6: Expected Findings**

### **Based on Error Pattern**:
```
epc = 77121504 in libc.so
ra  = 7719f5ec in onvif.cgi
```

**Expected strace patterns**:
1. **CGI process creation** (clone/execve)
2. **HTTP request processing** (read from stdin)
3. **XML template access** (open/read template files)
4. **String processing** (the actual crash point)
5. **Signal delivery** (SIGSEGV)

### **Key Questions to Answer**:
1. **Which exact function** in onvif.cgi is calling the libc function?
2. **What parameters** are being passed to the crashing function?
3. **What's the state** of memory allocations before the crash?
4. **Are there any** file descriptor or resource leaks?

## **Step 7: Quick Start Commands**

### **Immediate Action**:
```bash
# 1. Build current version
./build.sh

# 2. Run with strace (replace with your actual command)
strace -f -tt -o /tmp/debug.log onvif_simple_server &

# 3. Trigger the crash (make ONVIF requests)
curl -X POST http://localhost/onvif/events_service \
  -H "Content-Type: application/soap+xml" \
  -d '<soap:Envelope>...</soap:Envelope>'

# 4. Analyze the log
tail -100 /tmp/debug.log
grep -A5 -B5 "SIGSEGV\|segfault\|fault" /tmp/debug.log
```

### **Share Results**:
After running strace, please share:
1. **Last 50-100 lines** of the strace output before crash
2. **Any error messages** or failed system calls
3. **Process creation patterns** if events_service runs as CGI
4. **Memory allocation patterns** around the crash time

This will give us the exact system-level view of what's happening when the segfault occurs.
