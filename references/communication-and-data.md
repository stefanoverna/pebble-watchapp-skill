# Communication, Data & Services

AppMessage ↔ phone, PebbleKit JS, Clay config, persistent storage, timers,
wakeups, background workers, event services, and logging. Signatures from
`developer.repebble.com/guides/communication/...`, `/events-and-services/...`,
and the C API reference.

---

## AppMessage (watch ↔ phone / PebbleKit JS)

Bidirectional key/value messaging. Both sides reference the same `messageKeys`
declared in `package.json`, which generate `MESSAGE_KEY_*` (C) and are accessible
via `require('message_keys')` (JS).

### C API
```c
AppMessageResult app_message_open(uint32_t size_inbound, uint32_t size_outbound);

AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb);
AppMessageInboxDropped  app_message_register_inbox_dropped(AppMessageInboxDropped cb);
AppMessageOutboxSent    app_message_register_outbox_sent(AppMessageOutboxSent cb);
AppMessageOutboxFailed  app_message_register_outbox_failed(AppMessageOutboxFailed cb);
void  app_message_deregister_callbacks(void);

uint32_t app_message_inbox_size_maximum(void);    // pass these to app_message_open
uint32_t app_message_outbox_size_maximum(void);

AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator);
AppMessageResult app_message_outbox_send(void);

// Dictionary access:
Tuple *dict_find(DictionaryIterator *iter, uint32_t key);
void   dict_write_int(DictionaryIterator *iter, uint32_t key, const int *value, size_t size, bool is_signed);
void   dict_write_cstring(DictionaryIterator *iter, uint32_t key, const char *cstring);
void   dict_write_uint8(DictionaryIterator *iter, uint32_t key, uint8_t value);
void   dict_write_data(DictionaryIterator *iter, uint32_t key, const uint8_t *data, size_t size);
```

Callback typedefs:
```c
typedef void (*AppMessageInboxReceived)(DictionaryIterator *iter, void *context);
typedef void (*AppMessageInboxDropped)(AppMessageResult reason, void *context);
typedef void (*AppMessageOutboxSent)(DictionaryIterator *iter, void *context);
typedef void (*AppMessageOutboxFailed)(DictionaryIterator *iter, AppMessageResult reason, void *context);
```
`AppMessageResult`: `APP_MSG_OK`, `APP_MSG_SEND_TIMEOUT`, `APP_MSG_SEND_REJECTED`,
`APP_MSG_NOT_CONNECTED`, `APP_MSG_APP_NOT_RUNNING`, `APP_MSG_BUSY`,
`APP_MSG_BUFFER_OVERFLOW`, `APP_MSG_OUT_OF_MEMORY`, … (see docs for the full set).

### Init (register all callbacks, then open)
```c
app_message_register_inbox_received(inbox_received_callback);
app_message_register_inbox_dropped(inbox_dropped_callback);
app_message_register_outbox_sent(outbox_sent_callback);
app_message_register_outbox_failed(outbox_failed_callback);
// Prefer max buffers over hardcoded sizes:
app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
```

### Receiving
A JS Number arrives as `int32`, a JS String as `cstring`, bytes as `data`.
```c
static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *t_temp = dict_find(iter, MESSAGE_KEY_Temperature);
  if (t_temp) { int32_t temperature = t_temp->value->int32; }

  Tuple *t_loc = dict_find(iter, MESSAGE_KEY_LocationName);
  if (t_loc) { char *name = t_loc->value->cstring; /* copy into static buf if kept */ }

  Tuple *t_data = dict_find(iter, MESSAGE_KEY_Data);
  if (t_data) { uint8_t *bytes = t_data->value->data; }
}
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}
static void outbox_failed_callback(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}
static void outbox_sent_callback(DictionaryIterator *iter, void *context) { /* delivered */ }
```
Strings/data in a `Tuple` are only valid during the callback — copy into a
`static` buffer if you keep them.

### Sending
```c
DictionaryIterator *out;
AppMessageResult result = app_message_outbox_begin(&out);
if (result == APP_MSG_OK) {
  int request = 1;
  dict_write_int(out, MESSAGE_KEY_RequestData, &request, sizeof(int), true);
  dict_write_cstring(out, MESSAGE_KEY_Note, "hello");
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) APP_LOG(APP_LOG_LEVEL_ERROR, "send err %d", (int)result);
}
```

### messageKeys → constants
```json
"messageKeys": ["Temperature", "LocationName", "RequestData", "Note"]
```
generates `MESSAGE_KEY_Temperature`, etc., in C. For array-style keys in JS:
`var keys = require('message_keys'); d[keys.LapTimes + 1] = ...`.

---

## PebbleKit JS (`src/pkjs/index.js`)

Runs on the phone (inside the Pebble app). Use it for web requests, geolocation,
and configuration.

```javascript
Pebble.addEventListener('ready', function (e) {
  console.log('PebbleKit JS ready');
  getWeather();
});

Pebble.addEventListener('appmessage', function (e) {
  var dict = e.payload;                       // keyed by your message key names
  if (dict['RequestData']) { getWeather(); }
});

// Send to watch
var dict = { 'Temperature': 29, 'LocationName': 'London, UK' };
Pebble.sendAppMessage(dict,
  function () { console.log('sent: ' + JSON.stringify(dict)); },
  function (e) { console.log('failed: ' + JSON.stringify(e)); }
);

// Per-user / per-watch tokens (stable, per app)
Pebble.getAccountToken();
Pebble.getWatchToken();

// Web requests via XMLHttpRequest
function xhrRequest(url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () { callback(this.responseText); };
  xhr.open(type, url);
  xhr.send();
}
// usage: xhrRequest(apiUrl, 'GET', function (txt) { var json = JSON.parse(txt); ... });
```

---

## Configuration pages

### Clay (recommended — auto-generates the page)
In `src/pkjs/index.js`:
```javascript
var Clay = require('@rebble/clay');
var clayConfig = require('./config');     // JSON array describing the form
var clay = new Clay(clayConfig);
```
Clay auto-wires `showConfiguration`/`webviewclosed` and delivers settings to the
watch — config item `messageKey`s must match your declared `messageKeys`, so the
C side just reads them via `dict_find` in `inbox_received_callback`. Add Clay to
`package.json` dependencies.

### Manual config page
```javascript
Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL('https://example.com/config.html');
});
Pebble.addEventListener('webviewclosed', function (e) {
  var cfg = JSON.parse(decodeURIComponent(e.response));
  Pebble.sendAppMessage({
    'BackgroundColor': cfg.background_color,
    'SecondTick': cfg.second_ticks
  });
});
```
Set `"capabilities": ["configurable"]` in `package.json` so the gear icon
appears.

---

## Persistent storage

~4 KB per app; each value ≤ 256 bytes (`PERSIST_DATA_MAX_LENGTH`). Keys are
`uint32_t` constants. Read in `init`, write in `deinit`; guard reads with
`persist_exists`.

```c
status_t persist_write_int(uint32_t key, int32_t value);
int32_t  persist_read_int(uint32_t key);
status_t persist_write_bool(uint32_t key, bool value);
bool     persist_read_bool(uint32_t key);
int      persist_write_string(uint32_t key, const char *cstring);
int      persist_read_string(uint32_t key, char *buffer, size_t buffer_size);
int      persist_write_data(uint32_t key, const void *data, size_t size);
int      persist_read_data(uint32_t key, void *buffer, size_t buffer_size);
bool     persist_exists(uint32_t key);
status_t persist_delete(uint32_t key);
```
```c
#define SETTINGS_KEY 1
int value = persist_exists(SETTINGS_KEY) ? persist_read_int(SETTINGS_KEY) : 10; // default 10
// ... later:
persist_write_int(SETTINGS_KEY, value);
```

---

## Timers

```c
typedef void (*AppTimerCallback)(void *data);
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback cb, void *data);
bool      app_timer_reschedule(AppTimer *timer, uint32_t new_timeout_ms);
void      app_timer_cancel(AppTimer *timer);
```
For repeating work, re-register inside the callback. AppTimers only fire while the
app is running — for background scheduling use Wakeup.

---

## Wakeup API (launch app in the future, even if closed)

```c
typedef int32_t WakeupId;
typedef void (*WakeupHandler)(WakeupId wakeup_id, int32_t cookie);
void     wakeup_service_subscribe(WakeupHandler handler);
WakeupId wakeup_schedule(time_t timestamp, int32_t cookie, bool notify_if_missed);
void     wakeup_cancel(WakeupId wakeup_id);
void     wakeup_cancel_all(void);
bool     wakeup_get_launch_event(WakeupId *wakeup_id, int32_t *cookie);
```
Limits: max 8 scheduled wakeups per app; can't schedule within 30 s of now; fires
within ±1 minute.
```c
WakeupId id = wakeup_schedule(time(NULL) + 30 * SECONDS_PER_MINUTE, 0, true);
if (id >= 0) persist_write_int(43, id);
// at startup:
if (launch_reason() == APP_LAUNCH_WAKEUP) {
  WakeupId wid = 0; int32_t cookie = 0;
  wakeup_get_launch_event(&wid, &cookie);
}
```

---

## Background worker

One worker system-wide (`pebble new-project --worker`, `src/c/worker.c`). Workers
**cannot** use UI, AppMessage, or load resources. They **can** use
AccelerometerService, CompassService, DataLogging, HealthService,
ConnectionService, BatteryStateService, TickTimerService, and Storage. For timed
background launches, use Wakeup — not a worker.

---

## Event services

### ConnectionService
```c
typedef void (*ConnectionHandler)(bool connected);
typedef struct {
  ConnectionHandler pebble_app_connection_handler;   // BT link to phone app
  ConnectionHandler pebblekit_connection_handler;    // companion app
} ConnectionHandlers;
void connection_service_subscribe(ConnectionHandlers handlers);
bool connection_service_peek_pebble_app_connection(void);
```

### BatteryStateService
```c
typedef struct { uint8_t charge_percent; bool is_charging; bool is_plugged_in; } BatteryChargeState;
void               battery_state_service_subscribe(BatteryStateHandler handler);
BatteryChargeState battery_state_service_peek(void);
// handler: void (*)(BatteryChargeState charge);  charge.charge_percent is 0–100
```

### HealthService
Available (also inside background workers) for step/activity data via
`health_service_*` (e.g. `health_service_sum_today`,
`health_service_peek_current_value`). Requires `"capabilities": ["health"]` and
`PBL_HEALTH`. Fetch the HealthService C reference for exact signatures when
needed.

### AccelerometerService / CompassService
`accel_tap_service_subscribe`, `accel_data_service_subscribe` (batched samples,
e.g. `ACCEL_SAMPLING_10HZ`), `compass_service_subscribe` /
`compass_service_set_heading_filter`. Batch and filter to save battery.

---

## Logging

```c
APP_LOG(APP_LOG_LEVEL_DEBUG, "Loop index now %d", i);
```
Levels: `APP_LOG_LEVEL_ERROR`, `_WARNING`, `_INFO`, `_DEBUG`. printf-style format.
Stream with `pebble logs` or `pebble install --logs`.
