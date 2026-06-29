# io

The `io` library provides functions for reading, writing, and navigating the file system.

```sis
include "io";
```

---

## Free Functions

### `makeFile(path)`

Creates an empty file at `path`. If the file already exists, `makeFile` leaves it unchanged. Equivalent to the Unix `touch` command.

```sis
io.makeFile("notes.txt");
```

**Returns:** `true`

---

### `readFile(path)`

Reads the entire contents of a file and returns them as a string.

```sis
pin contents = io.readFile("notes.txt");
```

**Returns:** The file contents as a string.

**Throws:** If the file does not exist or cannot be opened.

---

### `writeToFile(path, content)`

Overwrites a file with `content`. If the file does not exist, `writeToFile` creates it.

```sis
io.writeToFile("notes.txt", "Hello, world!");
```

**Returns:** `true`

**Throws:** If the file cannot be opened for writing.

---

### `appendToFile(path, content)`

Appends `content` to the end of a file.

```sis
io.appendToFile("log.txt", "New entry\n");
```

**Returns:** `true`

**Throws:** If the file cannot be opened.

---

### `doesFileExist(path)`

Checks whether a file or directory exists at `path`.

```sis
if (io.doesFileExist("config.txt")) {
    // ...
}
```

**Returns:** `true` if the path exists, `false` otherwise.

---

### `deleteFile(path)`

Deletes the file at `path`.

```sis
io.deleteFile("temp.txt");
```

**Returns:** `true` if the file was deleted, `false` if it did not exist.

---

### `mkdir(path)`

Creates a directory at `path`, including any missing parent directories.

```sis
io.mkdir("data/logs/2025");
```

**Returns:** `true` if the directory was created, `false` if it already existed.

---

### `listDir(path)`

Returns an array of path strings for every entry in the directory at `path`.

```sis
pin entries = io.listDir("./src");
for (pin i = 0; i < len(entries); i += 1) {
    print(entries[i]);
}
```

**Returns:** An array of strings.

**Throws:** If `path` does not exist or is not a directory.

---

### `isFile(path)`

Checks whether `path` points to a regular file.

**Returns:** `true` if `path` is a regular file, `false` otherwise.

---

### `isDir(path)`

Checks whether `path` points to a directory.

**Returns:** `true` if `path` is a directory, `false` otherwise.

---

## File Class

`File` opens a file handle that supports reading, writing, and line-by-line iteration. The file must already exist.

```sis
pin f = new io.File("notes.txt");
```

**Throws:** If the file does not exist or cannot be opened.

---

### `f.read()`

Reads the entire file contents from the beginning.

```sis
pin text = f.read();
```

**Returns:** The file contents as a string.

---

### `f.write(content)`

Overwrites the file with `content`, starting from the beginning.

```sis
f.write("Overwritten content");
```

**Returns:** `true`

---

### `f.append(content)`

Appends `content` to the end of the file.

```sis
f.append("More text\n");
```

**Returns:** `true`

---

### `f.hasLines()`

Checks whether unread lines remain in the file.

```sis
while (f.hasLines()) {
    print(f.readLine());
}
```

**Returns:** `true` if at least one line remains, `false` otherwise.

---

### `f.readLine()`

Returns the next unread line, without a trailing newline character. Call `hasLines()` before calling `readLine()` to avoid reading past the end of the file.

```sis
while (f.hasLines()) {
    pin line = f.readLine();
    print(line);
}
```

**Returns:** The next line as a string.

---

### `f.close()`

Closes the file handle. The handle is also closed automatically when the `File` instance is garbage collected.

```sis
f.close();
```

**Returns:** `true`

---

## Examples

**Read a file line by line:**

```sis
include "io";

pin f = new io.File("data.txt");
while (f.hasLines()) {
    print(f.readLine());
}
f.close();
```

**Write and verify:**

```sis
include "io";

io.writeToFile("out.txt", "done");
print(io.doesFileExist("out.txt")); // true
```
