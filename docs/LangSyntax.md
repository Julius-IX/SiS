# Language Syntax Reference

This document covers the full syntax of the SiS language. It assumes you already know basic programming concepts like variables, loops, and functions.
Most syntax is similar to C, mostly omitting typing and other more complex features.

---

## Table of Contents

1. [Comments](#comments)
2. [Types and Literals](#types-and-literals)
3. [Variables](#variables)
4. [Operators](#operators)
5. [Control Flow](#control-flow)
6. [Functions](#functions)
7. [Arrays](#arrays)
8. [Classes](#classes)
9. [Modules](#modules)
10. [Built-in Functions](#built-in-functions)

---

## Comments

Comments are C-styled, `//` for single-line and `/*` and `*/` for multi-line.

```Cpp
// This is a comment
pin x = 10; // Inline comment
```

Doc comments use `///` and must appear directly before a named `fn` or `class` declaration. They attach documentation to that declaration.
The comment content is directly accessible as a string by using the `.__doc__` property.

```Cpp
/// Adds two numbers together.
fn add(a, b) {
    return a + b;
}

add.__doc__; // "Adds two numbers together."
```

---

## Types and Literals

The language has six value types.

| Type       | Example                      |
|------------|------------------------------|
| `null`     | `null`                       |
| `bool`     | `true`, `false`              |
| `num`      | `42`, `3.14`, `-7`           |
| `string`   | `"hello"`, `"world"`         |
| `array`    | `[1, 2, 3]`                  |
| `function` | `fn(x) { return x + 1; }`    |

All numbers are floating-point (`double` internally). There is no separate integer type.

**Truthiness rules:**

- `null` is falsy.
- `0` is falsy. All other numbers are truthy.
- `""` (empty string) is falsy. Non-empty strings are truthy.
- `[]` (empty array) is falsy. Non-empty arrays are truthy.
- Functions, classes, and instances are always truthy.

---

## Variables

Use `pin` to declare a variable. All variables are mutable after declaration.

```Cpp
pin x = 10;
pin name = "Alice";
pin nothing; // Initializes to null
```

Reassignment does not use `pin`.

```Cpp
pin x = 5;
x = 20; // x is now 20
x += 3; // x is now 23
```

Variable scope is lexical. A variable declared inside a block is not accessible outside it.

```Cpp
fn foo() {
    pin inner = 42;
}
// inner is not accessible here
```

---

## Operators

### Arithmetic

| Operator | Description    |
|----------|----------------|
| `+`      | Add            |
| `-`      | Subtract       |
| `*`      | Multiply       |
| `/`      | Divide         |
| `%`      | Modulo         |

Division by zero throws a runtime error.

`+` also concatenates strings. If either operand is a string, both sides are converted to strings and joined.

```Cpp
pin result = "Score: " + 42; // "Score: 42"
```

### Comparison

| Operator | Description              |
|----------|--------------------------|
| `==`     | Equal                    |
| `!=`     | Not equal                |
| `<`      | Less than                |
| `<=`     | Less than or equal       |
| `> `     | Greater than             |
| `>=`     | Greater than or equal    |

Values of different types are never equal. A number and a string holding the same digits are not `==`.

Arrays, classes, and instances compare by identity (same object in memory), not by contents.

### Logical

| Operator | Description |
|----------|-------------|
| `&&`     | And         |
| `\|\|`   | Or          |
| `!`      | Not         |

`&&` and `||` short-circuit. `&&` returns the first falsy value, or the last value if all are truthy. `||` returns the first truthy value, or the last value if all are falsy.

### Assignment

| Operator | Description         |
|----------|---------------------|
| `=`      | Assign              |
| `+=`     | Add and assign      |
| `-=`     | Subtract and assign |
| `*=`     | Multiply and assign |
| `/=`     | Divide and assign   |
| `%=`     | Modulo and assign   |

### Ternary

Ternary operations are structured as `condition ? if_true : if_false`.

```Cpp
pin label = score > 50 ? "pass" : "fail";
```

---

## Control Flow

### if / else

```Cpp
if (x > 0) {
    print("positive");
} else if (x < 0) {
    print("negative");
} else {
    print("zero");
}
```

The condition must be wrapped in parentheses. Braces are required.

### while

```Cpp
pin i = 0;
while (i < 5) {
    print(i);
    i += 1;
}
```

### for

The `for` loop has three optional clauses: initializer, condition, and increment.

```Cpp
for (pin i = 0; i < 10; i += 1) {
    print(i);
}
```

All three clauses are optional. Omitting the condition makes the loop run forever until a `break`.

```Cpp
for (;;) {
    // runs forever
}
```

### switch

`switch` compares a subject against a series of `case` values. Cases fall through by default, so use `break` to stop after a match.

```Cpp
switch (status) {
    case 0:
        print("idle");
        break;
    case 1:
        print("running");
        break;
    default:
        print("unknown");
        break;
}
```

`default` matches when no `case` matches. It can appear anywhere in the body, but conventionally goes last.

### break and continue

`break` exits the nearest enclosing loop or switch. `continue` skips the rest of the current iteration and moves to the next.

```Cpp
for (pin i = 0; i < 10; i += 1) {
    if (i == 5) { break; }
    if (i % 2 == 0) { continue; }
    print(i); // prints 1, 3
}
```

---

## Functions

### Anonymous functions

Functions are first-class values. You can assign them to variables or pass them as arguments.

```Cpp
pin square = fn(x) { return x * x; };
print(square(5)); // 25
```
### Named functions

Named functions are syntax sugar for anonymous functions. Internally they are treated the same way as `pin name = fn(args) { /* body */ };`.

```Cpp
fn greet(name) {
    return "Hello, " + name + "!";
}

print(greet("Alice")); // Hello, Alice!
```

### Closures

Functions capture the scope they are defined in.

```Cpp
fn makeCounter() {
    pin count = 0;
    return fn() {
        count += 1;
        return count;
    };
}

pin counter = makeCounter();
print(counter()); // 1
print(counter()); // 2
```

### Return values

`return` exits the function and returns a value. A function with no `return` statement implicitly returns the last value. You can also use `return;` to return `null` explicitly.
Explicit `return` should be used in function bodies to keep the intent clear.

---

## Arrays

Arrays are ordered key-value stores. They can act as lists, dictionaries, or a mix of both.

### List syntax

```Cpp
pin fruits = ["apple", "banana", "cherry"];
print(fruits[0]); // apple
fruits[1] = "blueberry";
```

Unkeyed elements get auto-assigned numeric keys starting at `0`.

### Dictionary syntax

```Cpp
pin person = ["name": "Alice", "age": 30];
print(person["name"]); // Alice
```

### Mixed syntax

```Cpp
pin data = [10, 20, "key": 99, 30];
// Numeric keys: 0 -> 10, 1 -> 20, 2 -> 30
// String key:   "key" -> 99
```

Numeric auto-keys only count unkeyed entries. Keyed entries do not advance the counter.

### Nested arrays

```Cpp
pin matrix = [[1, 2], [3, 4]];
print(matrix[0][1]); // 2
```

### Mutation

Arrays have reference semantics. Assigning an array to a new variable creates an alias, not a copy.

```Cpp
pin a = [1, 2, 3];
pin b = a;
b[0] = 99;
print(a[0]); // 99 -- same array
```

---

## Classes

### Declaration

```Cpp
class Animal {
    pin name = null;
    pin sound = "...";

    fn constructor(name) {
        this->name = name;
    }

    fn speak() {
        print(this->name + " says " + this->sound);
    }
}
```

Fields are declared with `pin` inside the class body. Methods are declared with `fn`. The constructor must be named `constructor`.

### Instantiation

```Cpp
pin a = new Animal("Cat");
a.speak(); // Cat says ...
```

Use `new ClassName(args)` to create an instance. Access fields and methods with `.` on the instance.

### `this` and `->`

Inside a method, use `this->field` to access fields and `this->method()` to call other methods on the same instance.
Using `this.field` is not valid and will throw an error, the `->` operator is mandatory.

```Cpp
fn greet() {
    return "Hi, I am " + this->name;
}
```

`this` alone (without `->`) is valid as a value, for example `return this;` for method chaining.

### Inheritance

```Cpp
class Dog extends Animal {
    fn constructor(name) {
        super->constructor(name);
        this->sound = "woof";
    }
}

pin d = new Dog("Rex");
d.speak(); // Rex says woof
```

Use `extends` to inherit from a parent class. Use `super->method()` to call the parent's version of a method.

A subclass can override any method from the parent. Overriding is done redefining the member function and does not need a special keyword.
Field defaults from the parent class are applied before the child's constructor runs.

---

## Modules

Use `include` to load another file. Include statements must appear at the top of the file, before any other code.

```Cpp
include "utils.sis";           // loads utils.sis as `utils` namespace
include "helpers.sis" as hlp;  // loads helpers.sis as `hlp` namespace
```

The included file's top-level declarations are available as a namespace under the file's stem name (or the alias you provide).
For example, if `utils.sis` defines `fn add(a, b)`, you call it as `utils.add(1, 2)`.

For classes in a namespace, use `new namespace.ClassName()`.
Nested access to namespaces is not supported.

```Cpp
include "math";
pin v = new math.Vector(1, 2);
```

Circular includes are not allowed and will throw an error.

### Standard library

Libraries are treated differently to normal includes. Import them with `include` using their name, omitting the `.sis` extension.
Any include that lacks a file extension is assumed to be a library. Libraries are located under `$SIS_PATH/lib/dynamic` or `$SIS_PATH/lib/managed`.
Files under `$SIS_PATH/lib/dynamic` are dynamic libraries. Files under `$SIS_PATH/lib/managed` are plain .sis libraries.

The following standard libraries are available:

| Library  | Path              | Description                              |
|----------|-------------------|------------------------------------------|
| `io`     | `io`              | File and stream I/O                      |
| `math`   | `math`            | Math functions and constants             |
| `os`     | `os`              | Operating system interaction             |
| `random` | `random`          | Random value generation                  |
| `regex`  | `regex`           | Regular expression matching              |
| `string` | `string`          | String manipulation utilities            |
| `time`   | `time`            | Date, time, and timing utilities         |

```Cpp
include "math";
print(math.sqrt(16)); // 4
```

Each library is very minimal and its encouraged to create your own libraries for more complex functionality. See [LibraryCreation.md](LibraryCreation.md) for more information.
Each library's full API is documented in its own file under `./stdlib/<library_name>`.

---

## Built-in Functions

These functions are available everywhere without any import.

### `print(...args)`

Prints all arguments to stdout, separated by spaces, followed by a newline.

```Cpp
print("hello", "world"); // hello world
print(42);               // 42
```

### `len(value)`

Returns the length of a string or array.

```Cpp
len("hello");       // 5
len([1, 2, 3]);     // 3
```

### `type(value)`

Returns the type name of a value as a string.

```Cpp
type(42);       // "num"
type("hello");  // "string"
type(null);     // "null"
type([]);       // "array"
```

### `str(value)`

Converts a value to its string representation.

```Cpp
str(3.14);  // "3.14"
str(true);  // "true"
str(null);  // "null"
```

### `num(value)`

Converts a string, bool, or number to a number. Throws if the conversion fails.

```Cpp
num("42");   // 42
num(true);   // 1
num(false);  // 0
```

### `push(array, value)`

Appends a value to an array and returns the array. The new element gets the next available numeric key.

```Cpp
pin arr = [1, 2];
push(arr, 3); // arr is now [1, 2, 3]
```

### `pop(array)`

Removes and returns the last element of an array. Throws if the array is empty.

```Cpp
pin arr = [1, 2, 3];
pin last = pop(arr); // last = 3, arr = [1, 2]
```

### `read(prompt?)`

Reads a line of input from stdin. If a prompt string is provided, it is printed first with no newline.

```Cpp
pin name = read("Enter your name: ");
print("Hello, " + name);
```

### Type checking functions

Each returns a `bool`.

| Function        | Returns `true` when value is... |
|-----------------|----------------------------------|
| `isNull(v)`     | `null`                           |
| `isBool(v)`     | a `bool`                         |
| `isNum(v)`      | a `num`                          |
| `isString(v)`   | a `string`                       |
| `isArray(v)`    | an `array`                       |
| `isFunction(v)` | a function                       |
| `isClass(v)`    | a class                          |
| `isInstance(v)` | a class instance                 |

```Cpp
isNum(42);       // true
isString(42);    // false
isNull(null);    // true
```

