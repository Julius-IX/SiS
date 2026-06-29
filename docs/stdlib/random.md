# random

The `random` library provides functions for generating random numbers and selecting random elements from arrays. It uses a Mersenne Twister (MT19937) generator seeded from hardware entropy at startup.

```c
include "random";
```

---

## Functions

### `seed(n)`

Seeds the random number generator with the integer `n`. The same seed always produces the same sequence of values, which is useful for reproducible results in tests.

```c
random.seed(42);
```

**Returns:** `null`

---

### `rand()`

Returns a random floating-point number in `[0.0, 1.0)`.

```c
pin n = random.rand(); // e.g. 0.7341...
```

**Returns:** A `num`.

---

### `randint(lo, hi)`

Returns a random integer in the inclusive range `[lo, hi]`.

```c
pin roll = random.randint(1, 6); // simulates a die roll
```

**Returns:** A `num`.

**Throws:** If `lo` is greater than `hi`.

---

### `choice(list)`

Returns a uniformly random element from `list`.

```c
pin suits = ["hearts", "diamonds", "clubs", "spades"];
pin suit = random.choice(suits);
```

**Returns:** One element from the array.

**Throws:** If `list` is empty.

---

### `shuffle(list)`

Shuffles `list` in place using a uniform random permutation and returns the same array.

```c
pin deck = [1, 2, 3, 4, 5];
random.shuffle(deck);
print(deck); // order is now random
```

**Returns:** The same array, shuffled.

**Note:** `shuffle` mutates the original array. It does not return a copy.

---

### `sample(list, n)`

Returns a new array of `n` elements drawn from `list` without replacement.

```c
pin pool = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
pin picks = random.sample(pool, 3); // e.g. [7, 2, 9]
```

**Returns:** A new array of `n` elements. The original array is not modified.

**Throws:** If `n` is greater than the size of `list`.

---

## Example

```c
include "random";

random.seed(1);

pin words = ["apple", "banana", "cherry", "date"];
pin pick = random.choice(words);
print("Today's word: " + pick);

random.shuffle(words);
print(words[0]); // random first element
```
