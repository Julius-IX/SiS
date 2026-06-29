#include <SisDynamicLibMacros.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

using std::string;

namespace fs = std::filesystem;

FN_SIGNATURE(makeFile, args) {
  if (args.size() != 1) throw std::runtime_error("makeFile(): expected 1 argument [path], got " + std::to_string(args.size()));
  std::ofstream(requireStr(args[0], "makeFile"));
  return {true};
}

FN_SIGNATURE(readFile, args) {
  if (args.size() != 1) throw std::runtime_error("readFile(): expected 1 argument [path], got " + std::to_string(args.size()));
  std::string path = requireStr(args[0], "readFile");
  if (!fs::exists(path)) throw std::runtime_error("readFile(): file does not exist, " + path);
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) throw std::runtime_error("readFile(): failed to open file, {}" + path);
  std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  file.close();

  return {contents};
}

FN_SIGNATURE(writeToFile, args) {
  if (args.size() != 2) throw std::runtime_error("writeToFile(): expected 2 arguments [path, content], got " + std::to_string(args.size()));

  std::string path = requireStr(args[0], "writeToFile");
  std::string content = requireStr(args[1], "writeToFile");

  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) throw std::runtime_error("writeToFile(): failed to open file, " + path);

  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.close();

  return {true};
}

FN_SIGNATURE(appendToFile, args) {
  if (args.size() != 2) throw std::runtime_error("appendToFile(): expected 2 arguments [path, content], got " + std::to_string(args.size()));

  std::string path = requireStr(args[0], "appendToFile");
  std::string content = requireStr(args[1], "appendToFile");

  std::ofstream file(path, std::ios::binary | std::ios::app);
  if (!file.is_open()) throw std::runtime_error("appendToFile(): failed to open file, " + path);

  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.close();

  return {true};
}

FN_SIGNATURE(doesFileExist, args) {
  if (args.size() != 1) throw std::runtime_error("doesFileExist(): expected 1 argument [path], got " + std::to_string(args.size()));

  std::string path = requireStr(args[0], "doesFileExist");

  return {fs::exists(path)};
}

FN_SIGNATURE(deleteFile, args) {
  if (args.size() != 1) throw std::runtime_error("deleteFile(): expected 1 argument [path], got " + std::to_string(args.size()));

  std::string path = requireStr(args[0], "deleteFile");

  return {fs::remove(path)};
}

FN_SIGNATURE(mkdir, args) {
  if (args.size() != 1) throw std::runtime_error("mkdir(): expected 1 argument [path], got " + std::to_string(args.size()));

  std::string path = requireStr(args[0], "mkdir");

  return {fs::create_directories(path)};
}

FN_SIGNATURE(listDir, args) {
  if (args.size() != 1) throw std::runtime_error("listDir(): expected 1 argument [path], got " + std::to_string(args.size()));

  std::string path = requireStr(args[0], "listDir");

  if (!fs::exists(path)) throw std::runtime_error("listDir(): directory does not exist, " + path);
  if (!fs::is_directory(path)) throw std::runtime_error("listDir(): not a directory, " + path);

  std::vector<eval::Value> entries;
  for (const auto& entry : fs::directory_iterator(path))
    entries.emplace_back(entry.path().string());

  return std::make_shared<eval::InternalArray>(entries);
}

FN_SIGNATURE(isFile, args) {
  if (args.size() != 1) throw std::runtime_error("isFile(): expected 1 argument [path], got " + std::to_string(args.size()));

  std::string path = requireStr(args[0], "isFile");

  return {fs::is_regular_file(path)};
}

FN_SIGNATURE(isDir, args) {
  if (args.size() != 1) throw std::runtime_error("isDir(): expected 1 argument [path], got " + std::to_string(args.size()));

  std::string path = requireStr(args[0], "isDir");

  return {fs::is_directory(path)};
}

class File {
  public:
  explicit File(std::string path)
    : m_path(std::move(path)) {
    if (!fs::exists(m_path)) throw std::runtime_error("File(): file does not exist, " + m_path);
    m_file.open(m_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!m_file.is_open()) throw std::runtime_error("File(): failed to open file, " + m_path);
  }

  ~File() {
    if (m_file.is_open()) m_file.close();
  }

  std::string read() {
    ensureOpen("read");
    m_file.clear();
    m_file.seekg(0);
    return {std::istreambuf_iterator<char>(m_file), std::istreambuf_iterator<char>()};
  }

  std::string readLine() {
    ensureLines("readLine");
    std::string line = std::move(m_lines.back());
    m_lines.pop_back();
    return line;
  }

  bool write(const std::string& content) {
    ensureOpen("write");
    m_file.clear();
    m_file.seekp(0);
    m_file.write(content.data(), static_cast<std::streamsize>(content.size()));
    m_file.flush();
    invalidateLineCache();
    return true;
  }

  bool append(const std::string& content) {
    ensureOpen("append");
    m_file.clear();
    m_file.seekp(0, std::ios::end);
    m_file.write(content.data(), static_cast<std::streamsize>(content.size()));
    m_file.flush();
    invalidateLineCache();
    return true;
  }

  bool close() {
    if (m_file.is_open()) m_file.close();
    return true;
  }

  bool hasLines() {
    ensureLines("hasLines");
    return !m_lines.empty();
  }

  private:
  std::string m_path;
  std::fstream m_file;
  std::vector<std::string> m_lines;
  bool m_lines_cached = false;

  void ensureOpen(const char* ctx) {
    if (!m_file.is_open()) throw std::runtime_error(std::string("File.") + ctx + "(): file is not open, " + m_path);
  }

  void ensureLines(const char* ctx) {
    ensureOpen(ctx);
    if (!m_lines_cached) {
      m_file.clear();
      m_file.seekg(0);
      std::string line;
      while (std::getline(m_file, line)) {
        m_lines.emplace_back(std::move(line));
      }
      std::ranges::reverse(m_lines);
      m_lines_cached = true;
    }
  }

  void invalidateLineCache() {
    m_lines.clear();
    m_lines_cached = false;
  }
};

SIS_MODULE_INIT(reg) {
  reg->defineFn("makeFile",
                makeFile,
                "@brief Creates an empty file at the given path.\n"
                "@param path Path to the file to create.\n"
                "@return true on success.\n"
                "@note Equivalent to the `touch` command. Does not fail if the file already exists.");
  reg->defineFn("readFile",
                readFile,
                "@brief Reads the entire contents of a file into a string.\n"
                "@param path Path to the file.\n"
                "@return The file contents as a string.\n"
                "@throws If the file does not exist or cannot be opened.");
  reg->defineFn("writeToFile",
                writeToFile,
                "@brief Overwrites a file with the given content.\n"
                "@param path Path to the file.\n"
                "@param content String content to write.\n"
                "@return true on success.\n"
                "@throws If the file cannot be opened for writing.");
  reg->defineFn("appendToFile",
                appendToFile,
                "@brief Appends content to the end of a file.\n"
                "@param path Path to the file.\n"
                "@param content String content to append.\n"
                "@return true on success.\n"
                "@throws If the file cannot be opened.");
  reg->defineFn("doesFileExist",
                doesFileExist,
                "@brief Checks whether a file or directory exists at the given path.\n"
                "@param path Path to check.\n"
                "@return true if the path exists, false otherwise.");
  reg->defineFn("deleteFile",
                deleteFile,
                "@brief Deletes the file at the given path.\n"
                "@param path Path to the file to delete.\n"
                "@return true if the file was deleted, false if it did not exist.");
  reg->defineFn("mkdir",
                mkdir,
                "@brief Creates a directory and any missing parent directories.\n"
                "@param path Path of the directory to create.\n"
                "@return true if the directory was created, false if it already existed.");
  reg->defineFn("listDir",
                listDir,
                "@brief Lists the entries in a directory.\n"
                "@param path Path to the directory.\n"
                "@return An array of path strings for each entry in the directory.\n"
                "@throws If the path does not exist or is not a directory.");
  reg->defineFn("isFile",
                isFile,
                "@brief Checks whether the given path points to a regular file.\n"
                "@param path Path to check.\n"
                "@return true if path is a regular file, false otherwise.");
  reg->defineFn("isDir",
                isDir,
                "@brief Checks whether the given path points to a directory.\n"
                "@param path Path to check.\n"
                "@return true if path is a directory, false otherwise.");

  // clang-format off
  SIS_NATIVE_CLASS_BEGIN(reg, "File", File, "@brief A handle to an open file supporting read, write, and line-by-line iteration.")
    .docs("@brief Opens the file at the given path for reading and writing.\n"
          "@param path Path to the file to open.\n"
          "@throws If the file does not exist or cannot be opened.")
    .constructor([](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) {
    std::string path = requireStr(args[0], "File()");
    SIS_NATIVE_CTOR(File, inst, native_var, path);
    })
    .docs("@brief Reads the entire file contents from the beginning.\n"
          "@return The file contents as a string.")
    .NATIVE_METHOD("read", inst, args, {
        if (args.size() > 1) throw std::runtime_error("File.read(): expected 0 arguments, got " + std::to_string(args.size()));
        return {SIS_GET_NATIVE(File, inst)->read()};
    })
    .docs("@brief Overwrites the file with the given content, starting from the beginning.\n"
          "@param content String content to write.\n"
          "@return true on success.")
    .NATIVE_METHOD("write", inst, args, {
      if (args.size() < 1 && args.size() < 2) throw std::runtime_error("File.write(): expected 1 argument, got " + std::to_string(args.size()));
      std::string content = requireStr(args[0], "File.write");
      return {SIS_GET_NATIVE(File, inst)->write(content)};
    })
    .docs("@brief Reads the next unread line from the file.\n"
          "@return The next line as a string, without a trailing newline.\n"
          "@note Use hasLines() to check if lines remain before calling.")
    .NATIVE_METHOD("readLine", inst, args, {
      if (args.size() > 1) throw std::runtime_error("File.readLine(): expected 0 arguments, got " + std::to_string(args.size()));
      return {SIS_GET_NATIVE(File, inst)->readLine()};
    })
    .docs("@brief Appends content to the end of the file.\n"
          "@param content String content to append.\n"
          "@return true on success.")
    .NATIVE_METHOD("append", inst, args, {
      if (args.size() < 1 && args.size() < 2) throw std::runtime_error("File.append(): expected 1 argument, got " + std::to_string(args.size()));
      std::string content = requireStr(args[0], "File.append");
      return {SIS_GET_NATIVE(File, inst)->append(content)};
    })
    .docs("@brief Closes the file handle.\n"
          "@return true on success.\n"
          "@note The file is also closed automatically when the instance is garbage collected.")
    .NATIVE_METHOD("close", inst, args, {
      if (args.size() > 1) throw std::runtime_error("File.close(): expected 0 arguments, got " + std::to_string(args.size()));
      return {SIS_GET_NATIVE(File, inst)->close()};
    })
    .docs("@brief Checks whether there are unread lines remaining in the file.\n"
          "@return true if at least one line remains, false otherwise.")
    .NATIVE_METHOD("hasLines", inst, args, {
      if (args.size() > 1) throw std::runtime_error("File.hasLines(): expected 0 arguments, got " + std::to_string(args.size()));
      return {SIS_GET_NATIVE(File, inst)->hasLines()};
    })
  SIS_NATIVE_CLASS_END();
  // clang-format on
}
