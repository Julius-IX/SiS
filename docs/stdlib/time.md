# time

The `time` library provides functions for getting the current time, pausing execution, and working with dates.

```c
include "time";
```

---

## Functions

### `now()`

Returns the current Unix timestamp: the number of seconds since 1 January 1970 00:00:00 UTC.

```c
pin ts = time.now(); // e.g. 1735689600.123
```

**Returns:** A `num`.

---

### `sleep(seconds)`

Pauses execution for `seconds` seconds. Fractional values are accepted.

```c
time.sleep(1);    // pause for 1 second
time.sleep(0.5);  // pause for 500 milliseconds
```

**Returns:** `null`

**Throws:** If `seconds` is negative.

---

### `clock()`

Returns the CPU time consumed by the program so far, in seconds. This measures processor time, not wall-clock time, so it is useful for benchmarking code rather than measuring elapsed real time.

```c
pin start = time.clock();
// ... work ...
pin elapsed = time.clock() - start;
print("CPU time: " + elapsed);
```

**Returns:** A `num`.

---

## DateTime Class

`DateTime` represents a point in time, decomposed into local date and time components.

```c
pin dt = new time.DateTime();          // current time
pin dt = new time.DateTime(1735689600); // from a Unix timestamp
```

**Constructor:** `DateTime(timestamp?)`

If `timestamp` is omitted, the constructor uses the current time.

---

### `dt.year()`

Returns the four-digit year.

```c
dt.year(); // e.g. 2025
```

### `dt.month()`

Returns the month as an integer in `[1, 12]`.

### `dt.day()`

Returns the day of the month as an integer in `[1, 31]`.

### `dt.hour()`

Returns the hour as an integer in `[0, 23]`.

### `dt.minute()`

Returns the minute as an integer in `[0, 59]`.

### `dt.second()`

Returns the second as an integer in `[0, 60]`.

---

### `dt.format(pattern)`

Formats the date and time using a `strftime`-compatible pattern string.

Common pattern codes:

| Code | Meaning                    | Example  |
|------|----------------------------|----------|
| `%Y` | Four-digit year            | `2025`   |
| `%m` | Month, zero-padded         | `01`     |
| `%d` | Day, zero-padded           | `15`     |
| `%H` | Hour (24h), zero-padded    | `09`     |
| `%M` | Minute, zero-padded        | `05`     |
| `%S` | Second, zero-padded        | `00`     |
| `%A` | Full weekday name          | `Monday` |

```c
dt.format("%Y-%m-%d %H:%M:%S"); // "2025-01-15 09:05:00"
```

**Returns:** The formatted string.

**Throws:** If the pattern is invalid or too long.

---

### `dt.to_timestamp()`

Returns the Unix timestamp this `DateTime` was constructed from.

```c
dt.to_timestamp(); // e.g. 1735689600
```

**Returns:** A `num`.

---

## Examples

**Measure elapsed wall time:**

```c
include "time";

pin start = time.now();
time.sleep(0.1);
pin elapsed = time.now() - start;
print("Elapsed: " + elapsed + "s");
```

**Format the current date:**

```c
include "time";

pin dt = new time.DateTime();
print(dt.format("%A, %d %B %Y")); // e.g. "Wednesday, 15 January 2025"
```

**Reconstruct a DateTime from a stored timestamp:**

```c
include "time";

pin ts = time.now();
// ... store ts somewhere ...
pin dt = new time.DateTime(ts);
print(dt.year());
```
