# SiS (SiS interprets Scripts)

SiS is a lightweight scripting language with a C-like syntax, first-class functions, classes, and an extremely minimal standard library of C++ native extensions.

---

## Quick Look

```sis
include "math";
include "string" as str;

class Shape {
    pin name = "shape";

    fn constructor(name) {
        this->name = name;
    }

    fn describe() {
        return str.format("I am a {}", this->name);
    }
}

class Circle extends Shape {
    pin radius = 0;

    fn constructor(r) {
        super->constructor("circle");
        this->radius = r;
    }

    fn area() {
        return math.PI * math.pow(this->radius, 2);
    }
}

pin c = new Circle(5);
print(c.describe());         // I am a circle
print(math.round(c.area())); // 79
```

For the full language reference, see [docs/LangSyntax.md](docs/LangSyntax.md).

---

## Building

Using the provided install scripts in releases is recommended. But you can also build from source.
SiS requires CMake. The build assumes a Linux or Unix environment. Windows cross-compilation is supported via MinGW.
See [docs/Building.md](docs/Building.md) for more information.

---

## Running a Script

```bash
sis my_script.sis
```

Set `$SIS_PATH` to the directory where SiS is installed. The interpreter looks for standard libraries under `$SIS_PATH/lib/managed` or `$SIS_PATH/lib/dynamic`.

---

## Standard Library

Import libraries with `include`. Omit the file extension for standard library modules.

```sis
include "math";
print(math.sqrt(144)); // 12
```

| Library  | Description                        |
|----------|------------------------------------|
| `io`     | File and directory I/O             |
| `math`   | Math functions and constants       |
| `os`     | Environment, shell, process control|
| `random` | Random number and array utilities  |
| `regex`  | Regular expression matching        |
| `string` | String manipulation                |
| `time`   | Timestamps, sleep, and DateTime    |

Full documentation for each library is in [docs/stdlib/](docs/stdlib/).

---

## Extending SiS

SiS loads native extensions from shared libraries (`.so` / `.dll`). Write your extension in C++, implement `sis_module_init`, and drop the compiled library into `$SIS_PATH/lib/dynamic/`.

```cpp
#include <SisDynamicLibMacros.h>

FN_SIGNATURE(fnAdd, args) {
    double a = requireNum(args[0], "add");
    double b = requireNum(args[1], "add");
    return {a + b};
}

SIS_MODULE_INIT(reg) {
    reg->defineFn("add", fnAdd, "@brief Adds two numbers.");
}
```

```bash
g++ -std=c++23 -shared -fPIC \
    -I/path/to/sis/include \
    mylib.cpp \
    -o $SIS_PATH/lib/dynamic/mylib.so
```

```sis
include "mylib";
print(mylib.add(1, 2)); // 3
```

See [docs/LibraryCreation.md](docs/LibraryCreation.md) for the full API and [docs/Building.md](docs/Building.md) for build options.

---

## TODO

- [ ] Make LSP server
- [ ] Add example project
