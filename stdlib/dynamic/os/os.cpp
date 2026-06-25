#include <SisDynamicLibMacros.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int s_argc = 0;
static const char** s_argv = nullptr;

extern "C" void sis_set_args(int argc, const char** argv) {
  s_argc = argc;
  s_argv = argv;
}

FN_SIGNATURE(fnEnv, args) {
  if (args.size() != 1) throw std::runtime_error("env(): expected 1 argument [key], got " + std::to_string(args.size()));
  std::string key = requireStr(args[0], "env");
  const char* val = std::getenv(key.c_str());
  if (val == nullptr) return {};
  return {std::string(val)};
}

FN_SIGNATURE(fnArgs, args) {
  std::vector<eval::Value> out;
  out.reserve(static_cast<size_t>(s_argc));
  for (int i = 0; i < s_argc; ++i)
    out.emplace_back(std::string(s_argv[i]));
  return {std::make_shared<eval::InternalArray>(std::move(out))};
}

FN_SIGNATURE(fnCwd, args) { return {fs::current_path().string()}; }

FN_SIGNATURE(fnExit, args) {
  if (args.size() > 1) throw std::runtime_error("exit(): expected 0 or 1 arguments, got " + std::to_string(args.size()));
  int code = 0;
  if (args.size() == 1) code = static_cast<int>(requireNum(args[0], "exit"));
  std::exit(code);
}

FN_SIGNATURE(fnExec, args) {
  if (args.size() != 1) throw std::runtime_error("exec(): expected 1 argument [command], got " + std::to_string(args.size()));
  std::string cmd = requireStr(args[0], "exec");

#if defined(_WIN32)
  FILE* pipe = _popen(cmd.c_str(), "r");
#else
  std::string shell_cmd = cmd;
  const char* path = std::getenv("PATH");
  if (path != nullptr) shell_cmd = "PATH=" + std::string(path) + " " + cmd;
  FILE* pipe = popen(shell_cmd.c_str(), "r");
#endif
  if (pipe == nullptr) throw std::runtime_error("exec(): failed to run command: " + cmd);

  std::string result;
  std::array<char, 256> buf{};
  while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr)
    result += buf.data();

#if defined(_WIN32)
  _pclose(pipe);
#else
  pclose(pipe);
#endif

  return {result};
}

FN_SIGNATURE(fnPlatform, args) {
#if defined(_WIN32) || defined(_WIN64)
  return {std::string("windows")};
#elif defined(__APPLE__) && defined(__MACH__)
  return {std::string("macos")};
#elif defined(__linux__)
  return {std::string("linux")};
#elif defined(__FreeBSD__)
  return {std::string("freebsd")};
#elif defined(__unix__)
  return {std::string("unix")};
#else
  return {std::string("unknown")};
#endif
}

SIS_MODULE_INIT(reg) {
  reg->defineFn("env", fnEnv);
  reg->defineFn("args", fnArgs);
  reg->defineFn("cwd", fnCwd);
  reg->defineFn("exit", fnExit);
  reg->defineFn("exec", fnExec);
  reg->defineFn("platform", fnPlatform);
}
