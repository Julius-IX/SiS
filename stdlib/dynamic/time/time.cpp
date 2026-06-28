#include <SisDynamicLibMacros.h>

#include <chrono>
#include <ctime>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using std::string;

FN_SIGNATURE(fnNow, args) {
  using namespace std::chrono;
  double ts = duration<double>(system_clock::now().time_since_epoch()).count();
  return {ts};
}

FN_SIGNATURE(fnSleep, args) {
  if (args.size() != 1) throw std::runtime_error("sleep(): expected 1 argument [seconds], got " + std::to_string(args.size()));
  double secs = requireNum(args[0], "sleep");
  if (secs < 0.0) throw std::runtime_error("sleep(): seconds must be non-negative");
  std::this_thread::sleep_for(std::chrono::duration<double>(secs));
  return {};
}

FN_SIGNATURE(fnClock, args) { return {static_cast<double>(std::clock()) / static_cast<double>(CLOCKS_PER_SEC)}; }

class NativeDateTime {
  public:
  // default to now if no timestamp given
  explicit NativeDateTime(double timestamp)
    : m_timestamp(timestamp) {
    auto t = static_cast<time_t>(timestamp);
    m_tm = *std::localtime(&t);
  }

  int year() const { return m_tm.tm_year + 1900; }
  [[nodiscard]] int month() const { return m_tm.tm_mon + 1; }
  [[nodiscard]] int day() const { return m_tm.tm_mday; }
  [[nodiscard]] int hour() const { return m_tm.tm_hour; }
  [[nodiscard]] int minute() const { return m_tm.tm_min; }
  [[nodiscard]] int second() const { return m_tm.tm_sec; }

  [[nodiscard]] std::string format(const std::string& pattern) const {
    char buf[256];
    if (std::strftime(buf, sizeof(buf), pattern.c_str(), &m_tm) == 0) throw std::runtime_error("DateTime.format(): pattern too long or invalid");
    return string(buf);
  }

  [[nodiscard]] double toTimestamp() const { return m_timestamp; }

  private:
  double m_timestamp;
  std::tm m_tm;
};

SIS_MODULE_INIT(reg) {
  reg->defineFn("now", fnNow,
      "@brief Returns the current Unix timestamp.\n"
      "@return The number of seconds since the Unix epoch as a floating-point number."
  );
  reg->defineFn("sleep", fnSleep,
      "@brief Suspends execution for the given number of seconds.\n"
      "@param seconds Duration to sleep. Must be non-negative.\n"
      "@throws If seconds is negative."
  );
  reg->defineFn("clock", fnClock,
      "@brief Returns the processor time consumed by the program.\n"
      "@return CPU time used since program start, in seconds.\n"
      "@note Useful for benchmarking. Not wall-clock time."
  );

  // clang-format off
  SIS_NATIVE_CLASS_BEGIN(reg, "DateTime", NativeDateTime, "@brief Represents a point in time, decomposed into local date and time components.")
    .docs("@brief Constructs a DateTime from a Unix timestamp.\n"
          "@param timestamp Optional Unix timestamp in seconds. Defaults to the current time if omitted.")
    .constructor([](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) {
      double ts;
      if (args.empty()) {
        using namespace std::chrono;
        ts = duration<double>(system_clock::now().time_since_epoch()).count();
      } else {
        ts = requireNum(args[0], "DateTime()");
      }
      SIS_NATIVE_CTOR(NativeDateTime, inst, native_var, ts);
    })
    .docs("@brief Returns the year component of the date.\n"
          "@return The full four-digit year (e.g. 2025).")
    .NATIVE_METHOD("year", inst, args, { return {static_cast<double>(SIS_GET_NATIVE(NativeDateTime, inst)->year())}; })
    .docs("@brief Returns the month component of the date.\n"
          "@return The month as an integer in the range [1, 12].")
    .NATIVE_METHOD("month", inst, args, { return {static_cast<double>(SIS_GET_NATIVE(NativeDateTime, inst)->month())}; })
    .docs("@brief Returns the day of the month.\n"
          "@return The day as an integer in the range [1, 31].")
    .NATIVE_METHOD("day", inst, args, { return {static_cast<double>(SIS_GET_NATIVE(NativeDateTime, inst)->day())}; })
    .docs("@brief Returns the hour component of the time.\n"
          "@return The hour as an integer in the range [0, 23].")
    .NATIVE_METHOD("hour", inst, args, { return {static_cast<double>(SIS_GET_NATIVE(NativeDateTime, inst)->hour())}; })
    .docs("@brief Returns the minute component of the time.\n"
          "@return The minute as an integer in the range [0, 59].")
    .NATIVE_METHOD("minute", inst, args, { return {static_cast<double>(SIS_GET_NATIVE(NativeDateTime, inst)->minute())}; })
    .docs("@brief Returns the second component of the time.\n"
          "@return The second as an integer in the range [0, 60].")
    .NATIVE_METHOD("second", inst, args, { return {static_cast<double>(SIS_GET_NATIVE(NativeDateTime, inst)->second())}; })
    .docs("@brief Formats the date and time using a strftime-compatible pattern.\n"
          "@param pattern A strftime format string (e.g. \"%Y-%m-%d %H:%M:%S\").\n"
          "@return The formatted date/time string.\n"
          "@throws If the pattern is too long or invalid.")
    .NATIVE_METHOD("format", inst, args, {
      if (args.size() != 1) throw std::runtime_error("DateTime.format(): expected 1 argument [pattern]");
      return {SIS_GET_NATIVE(NativeDateTime, inst)->format(requireStr(args[0], "DateTime.format"))};
    })
    .docs("@brief Returns the original Unix timestamp this DateTime was constructed from.\n"
          "@return Seconds since the Unix epoch as a floating-point number.")
    .NATIVE_METHOD("to_timestamp", inst, args, { return {SIS_GET_NATIVE(NativeDateTime, inst)->toTimestamp()}; })
  SIS_NATIVE_CLASS_END();
  // clang-format on
}
