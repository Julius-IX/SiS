# os

The `os` library provides access to the operating system: environment variables, command-line arguments, the working directory, shell execution, and process control.

```c
include "os";
```

---

## Functions

### `env(key)`

Reads the environment variable named `key`.

```c
pin home = os.env("HOME");
```

**Returns:** The value as a string, or `null` if the variable is not set.

---

### `args()`

Returns the command-line arguments passed to the interpreter.

```c
pin argv = os.args();
print(argv[0]); // path to the interpreter
```

**Returns:** An array of strings. Index `0` is the executable path. User-supplied arguments start at index `1`.

---

### `cwd()`

Returns the current working directory as an absolute path.

```c
print(os.cwd()); // /home/user/project
```

**Returns:** A string.

---

### `platform()`

Returns a string identifying the current operating system.

```c
print(os.platform()); // "linux"
```

**Returns:** One of `"windows"`, `"macos"`, `"linux"`, `"freebsd"`, `"unix"`, or `"unknown"`.

---

### `exec(command)`

Runs `command` in a shell and captures its standard output.

```c
pin result = os.exec("echo hello");
print(result); // "hello\n"
```

**Returns:** The stdout output of the command as a string.

**Throws:** If the command cannot be started.

**Note:** `exec` captures stdout only. Stderr goes to the terminal. The return value includes any trailing newline the command produces.

---

### `exit(code?)`

Terminates the process with the given exit code. `exit` does not return.

```c
os.exit(0);  // success
os.exit(1);  // failure
```

`code` defaults to `0` if omitted.

---

## Example

```c
include "os";

if (os.platform() == "windows") {
    print("Running on Windows");
} else {
    pin user = os.env("USER");
    print("Hello, " + user);
}
```
