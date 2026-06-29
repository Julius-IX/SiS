# regex

The `regex` library provides regular expression matching, searching, and replacement. All patterns use ECMAScript syntax.

```c
include "regex";
```

---

## Match Results

Functions that find a match return an array, not a plain string. The array has the following layout:

| Index | Contents                          |
|-------|-----------------------------------|
| `[0]` | The full matched string           |
| `[1]` | First capture group (if any)      |
| `[n]` | nth capture group (if any)        |

When there is no match, the function returns `null`.

---

## Free Functions

Use these functions for one-off operations. If you apply the same pattern repeatedly, use the `Regex` class instead, which compiles the pattern once.

### `match(pattern, s)`

Tests whether `s` matches `pattern` in its entirety. The pattern must cover the whole string.

```c
pin m = regex.match("\\d{4}", "2025");
print(m[0]); // "2025"

regex.match("\\d{4}", "abc"); // null
```

**Returns:** A match array, or `null`.

---

### `search(pattern, s)`

Finds the first occurrence of `pattern` anywhere in `s`.

```c
pin m = regex.search("\\d+", "order 99 placed");
print(m[0]); // "99"
```

**Returns:** A match array, or `null`.

---

### `find_all(pattern, s)`

Finds all non-overlapping matches of `pattern` in `s`.

```c
pin matches = regex.find_all("\\d+", "a1 b22 c333");
print(matches[0][0]); // "1"
print(matches[1][0]); // "22"
print(matches[2][0]); // "333"
```

**Returns:** An array of match arrays. Returns an empty array if there are no matches.

---

### `replace(pattern, s, replacement)`

Replaces all matches of `pattern` in `s` with `replacement`. The replacement string supports back-references such as `$1` for the first capture group.

```c
regex.replace("\\s+", "too   many   spaces", " ");
// "too many spaces"
```

**Returns:** The resulting string.

---

### `split(pattern, s)`

Splits `s` on every match of `pattern`.

```c
regex.split("\\s*,\\s*", "a, b,  c");
// ["a", "b", "c"]
```

**Returns:** An array of substrings.

---

## Regex Class

Compile a pattern once and reuse it across multiple calls. This is more efficient than the free functions when you apply the same pattern in a loop.

```c
pin re = new regex.Regex("(\\w+)@(\\w+\\.\\w+)", "i");
```

**Constructor:** `Regex(pattern, flags?)`

`flags` is an optional string containing one or more of the following characters:

| Flag | Effect              |
|------|---------------------|
| `i`  | Case-insensitive    |
| `m`  | Multiline           |

**Throws:** If the pattern is invalid.

---

### `re.match(s)`

Same behavior as the free `match` function, using the compiled pattern.

### `re.search(s)`

Same behavior as the free `search` function, using the compiled pattern.

### `re.find_all(s)`

Same behavior as the free `find_all` function, using the compiled pattern.

### `re.replace(s, replacement)`

Same behavior as the free `replace` function, using the compiled pattern.

### `re.split(s)`

Same behavior as the free `split` function, using the compiled pattern.

---

## Example

```c
include "regex";

pin log = "2025-01-15 ERROR something went wrong";
pin re = new regex.Regex("(\\d{4}-\\d{2}-\\d{2}) (\\w+) (.+)");

pin m = re.search(log);
if (m) {
    print("date:    " + m[1]);
    print("level:   " + m[2]);
    print("message: " + m[3]);
}
```
