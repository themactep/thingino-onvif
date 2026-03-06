# Motion Detection Event Communication

This document describes how motion detection events are produced, delivered, and what
clients should do to recover from missed events.

---

## Event Triggering

Motion events are **file-based**. The `onvif_notify_server` daemon watches the
`/run/motion/` directory via **inotify**:

| Filesystem action | Event produced |
|-------------------|----------------|
| File **created** in `/run/motion/` | Motion **start** (`IsMotion = true`) |
| File **deleted** from `/run/motion/` | Motion **stop** (`IsMotion = false`) |

This means the camera's motion detection pipeline only needs to create/delete a file to
signal state changes — it does not talk to the ONVIF layer directly.

---

## Delivery Models

### Pull-Point (polling)

1. Client calls `CreatePullPointSubscription` → receives an endpoint URL containing
   `sub=N`.
2. Client periodically calls `PullMessages?sub=N` with a timeout value.
3. The server blocks the response until an event is available **or** the timeout
   expires, then returns all pending events as a batch.
4. The pending-event bitmask for that subscription slot is **cleared after the
   response is sent**, so events are not re-delivered on the next poll.

**Recovery behaviour**: pending events (including a motion-stop) remain in shared
memory until successfully acknowledged by a pull response. A client that reconnects
after a network outage will receive all queued events on its next successful poll.

### Base Subscription (push)

1. Client calls `Subscribe` with a `NotificationProducerRP` reference URL.
2. The server immediately HTTP-POSTs each event to that URL as it occurs.
3. No acknowledgement or retry is performed. If the POST fails, **the event is lost**.

---

## Subscription Lifecycle

- Subscriptions expire after a configurable TTL (default **10 minutes**).
- `clean_expired_subscriptions()` in `onvif_notify_server` removes stale entries
  automatically.
- A client must call `Renew` before the TTL expires to keep the subscription alive.
- On a fresh subscription the server sends an **"Initialized"** property message that
  reflects the *current* motion state, so clients always start with a correct baseline.

---

## Debounce / Rate Limiting

The `events_min_interval_ms` setting (in `/etc/onvif.d/events.json`) suppresses
repeated events within the configured window:

```json
{
  "events_min_interval_ms": 1000
}
```

Set to `0` to disable. This prevents event floods when a noisy source toggles rapidly.

---

## ⚠️ Missing Motion-Stop Event (Network Issues)

### Pull subscribers

A missed stop event is **not lost** — it stays queued in shared memory until the client
successfully polls. As soon as connectivity is restored the next `PullMessages` call
will deliver the stop event. Pull-based clients are therefore **self-healing**.

### Push subscribers

There is **no retry**. If the network is unavailable when a motion-stop event fires, the
client will never receive it and will believe motion is still active indefinitely.

The server has **no built-in watchdog** that automatically forces motion to the `false`
state on the client side.

### Recommended client-side mitigations

1. **Watchdog timer**: after receiving a motion-start event, start a timer. If no
   motion-stop event arrives within a reasonable window (e.g. 30–60 s), treat motion as
   stale and reset to the no-motion state locally.

2. **Re-subscribe on reconnect**: after any connectivity disruption, tear down and
   re-create the subscription. The server sends an **"Initialized"** property on the new
   subscription that reflects the true current state.

3. **Use `GetEventProperties` / `SynchronizationPoint`**: the ONVIF spec provides a
   `SynchronizationPoint` call that triggers the server to re-emit the current state of
   all properties to existing subscribers. Call this after reconnecting instead of
   re-subscribing from scratch.

4. **Prefer Pull over Push** on unreliable networks: as described above, Pull
   subscriptions queue events server-side and are inherently more resilient to transient
   connectivity loss.

---

## Shared Memory Layout (reference)

```c
typedef struct {
    time_t   e_time;                  // Timestamp of last state change
    int      is_on;                   // Current motion state: 1=active, 0=inactive
    uint32_t pull_send_initialized;   // Bitmask: send Initialized property per subscriber
    uint32_t pull_notify;             // Bitmask: send Changed property per subscriber
} event_shm_t;

typedef struct {
    int      id;                      // Subscription ID (1–65535)
    char     reference[256];          // Client callback URL (Push only)
    int      used;                    // SUB_UNUSED / SUB_PULL / SUB_PUSH
    time_t   expire;                  // Subscription expiration (Unix timestamp)
    char     topic_expression[1024];  // Topic filter, e.g. "**/MotionAlarm"
} subscription_shm_t;
```

Up to **32 concurrent subscriptions** are supported (one bit per slot in the bitmasks
above).
