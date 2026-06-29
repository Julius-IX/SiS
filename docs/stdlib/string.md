# string

The `string` library provides functions for transforming, searching, and formatting strings.

```c
include "string";
```

All functions treat strings as sequences of bytes. They do not perform Unicode-aware operations.

---

## Case

### `upper(s)`

Converts every character in `s` to uppercase.

```c
string.upper("hello"); // "HELLO"
```

### `lower(s)`

Converts every character in `s` to lowercase.

```c
string.lower("HELLO"); // "hello"
```

---

## Whitespace

### `trim(s)`

Removes leading and trailing whitespace from `s`.

```c
string.trim("  hello  "); // "hello"
```

### `ltrim(s)`

Removes leading whitespace only.

### `rtrim(s)`

Removes trailing whitespace only.

---

## Searching

### `contains(s, sub)`

Returns `true` if `s` contains `sub`.

```c
string.contains("hello world", "world"); // true
```

### `starts_with(s, prefix)`

Returns `true` if `s` begins with `prefix`.

```c
string.starts_with("hello", "he"); // true
```

### `ends_with(s, suffix)`

Returns `true` if `s` ends with `suffix`.

```c
string.ends_with("hello", "lo"); // true
```

### `find(s, sub)`

Returns the zero-based index of the first occurrence of `sub` in `s`.

```c
string.find("hello", "ll"); // 2
string.find("hello", "z");  // -1
```

**Returns:** The index as a `num`, or `-1` if not found.

---

## Extracting and Replacing

### `substr(s, start, len?)`

Extracts a substring starting at index `start`. If `len` is provided, at most `len` characters are returned.

```c
string.substr("hello world", 6);    // "world"
string.substr("hello world", 6, 3); // "wor"
```

**Returns:** The extracted substring, or an empty string if `start` is out of range.

### `replace(s, from, to)`

Replaces every occurrence of `from` in `s` with `to`.

```c
string.replace("aabbcc", "b", "x"); // "aaxxcc"
```

**Returns:** The resulting string.

---

## Splitting and Joining

### `split(s, delimiter)`

Splits `s` into an array of substrings, cutting at each occurrence of `delimiter`.

```c
string.split("a,b,c", ","); // ["a", "b", "c"]
```

**Returns:** An array of strings. Returns the original string unchanged if `delimiter` is empty.

### `join(list, delimiter)`

Joins an array of strings into a single string, inserting `delimiter` between each element.

```c
string.join(["a", "b", "c"], "-"); // "a-b-c"
```

**Returns:** The concatenated string.

---

## Formatting

### `format(template, ...args)`

Replaces each `{}` placeholder in `template` with the next argument, in order.

```c
string.format("Hello, {}! You are {} years old.", "Alice", 30);
// "Hello, Alice! You are 30 years old."
```

Use `{{` and `}}` to include a literal brace in the output.

```c
string.format("{{{}}}",  "x"); // "{x}"
```

**Throws:** If the number of `{}` placeholders does not match the number of arguments.

---

## Example

```c
include "string";

pin csv = "alice,30,engineer";
pin parts = string.split(csv, ",");

print(string.upper(parts[0]));  // "ALICE"
print(string.format("Name: {}, Age: {}, Role: {}", parts[0], parts[1], parts[2]));
```
