# math

The `math` library provides common mathematical functions and constants.

```sis
include "math";
```

All functions accept and return `num` values. Angles are in radians.

---

## Constants

| Name   | Value                  | Description              |
|--------|------------------------|--------------------------|
| `PI`   | 3.141592653589793      | Pi                       |
| `E`    | 2.718281828459045      | Euler's number           |
| `INF`  | Infinity               | Positive infinity        |
| `NAN`  | NaN                    | Not a number             |

```sis
print(math.PI);  // 3.141592653589793
print(math.INF); // Infinity
```

---

## Rounding

### `abs(n)`

Returns the absolute value of `n`.

```sis
math.abs(-5); // 5
```

### `floor(n)`

Rounds `n` down to the nearest integer.

```sis
math.floor(3.9); // 3
```

### `ceil(n)`

Rounds `n` up to the nearest integer.

```sis
math.ceil(3.1); // 4
```

### `round(n)`

Rounds `n` to the nearest integer. Ties round away from zero.

```sis
math.round(2.5); // 3
```

### `clamp(value, min, max)`

Constrains `value` to the range `[min, max]`.

```sis
math.clamp(15, 0, 10); // 10
math.clamp(-5, 0, 10); // 0
math.clamp(7, 0, 10);  // 7
```

---

## Powers and Roots

### `sqrt(n)`

Returns the square root of `n`. `n` must be non-negative.

```sis
math.sqrt(9); // 3
```

### `cbrt(n)`

Returns the cube root of `n`.

```sis
math.cbrt(27); // 3
```

### `pow(base, exp)`

Raises `base` to the power of `exp`.

```sis
math.pow(2, 10); // 1024
```

---

## Logarithms

### `log(n)`

Returns the natural logarithm (base e) of `n`. `n` must be positive.

### `log2(n)`

Returns the base-2 logarithm of `n`. `n` must be positive.

### `log10(n)`

Returns the base-10 logarithm of `n`. `n` must be positive.

```sis
math.log(math.E); // 1
math.log2(8);     // 3
math.log10(1000); // 3
```

---

## Trigonometry

All trig functions work in radians. Use `math.PI` to convert from degrees: `degrees * math.PI / 180`.

### `sin(radians)` / `cos(radians)` / `tan(radians)`

Returns the sine, cosine, or tangent of an angle.

### `asin(n)` / `acos(n)` / `atan(n)`

Returns the arc sine, arc cosine, or arc tangent of `n`, in radians.

- `asin` and `acos` require `n` in `[-1, 1]`.
- `asin` returns a value in `[-PI/2, PI/2]`.
- `acos` returns a value in `[0, PI]`.
- `atan` returns a value in `[-PI/2, PI/2]`.

### `atan2(y, x)`

Returns the angle in radians between the positive x-axis and the point `(x, y)`. The result is in `[-PI, PI]`. Use `atan2` instead of `atan` when you need the correct quadrant.

```sis
math.atan2(1, 1); // PI/4
```

---

## Min and Max

### `min(a, b)`

Returns the smaller of `a` and `b`.

### `max(a, b)`

Returns the larger of `a` and `b`.

```sis
math.min(3, 7); // 3
math.max(3, 7); // 7
```

---

## Example

```sis
include "math";

pin hypotenuse = fn(a, b) {
    return math.sqrt(math.pow(a, 2) + math.pow(b, 2));
};

print(hypotenuse(3, 4)); // 5
```
