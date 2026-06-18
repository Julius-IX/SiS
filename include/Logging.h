#pragma once

#include <string>
#include <unordered_map>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// Default log level override before including this header if needed.
#ifndef DEFAULT_LOG_LEVEL
#define DEFAULT_LOG_LEVEL spdlog::level::debug
#endif

// Default logger name override per build target if needed (e.g. "server", "client").
#ifndef DEFAULT_LOGGER_NAME
#define DEFAULT_LOGGER_NAME "app"
#endif

// Set to 1 to strip all logging at compile time.
#ifndef LOGGING_DISABLED
#define LOGGING_DISABLED 0
#endif

// ---------------------------------------------------------------------------
// Logger
//
// Singleton logger wrapping spdlog. Owns a default logger and an optional
// map of named loggers for subsystem-specific routing.
//
// On construction the default logger writes to stdout and stderr.
// Additional sinks (e.g. file) can be added at runtime via addSink().
// Named loggers inherit the default sinks unless given their own.
//
// Invariant: Logger::get() is the only way to obtain the instance.
// ---------------------------------------------------------------------------
class Logger {
  public:
  // Returns the singleton instance.
  // @returns The Logger instance.
  static Logger& get() {
    static Logger instance = Logger();
    return instance;
  }

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger&&) = delete;
  Logger& operator=(Logger&&) = delete;

  // Add a sink to the default logger.
  // @param sink The spdlog sink to attach.
  void addSink(const spdlog::sink_ptr& sink) { m_default->sinks().push_back(sink); }

  // Add a sink to a named logger.
  // @param name The name of the logger to add the sink to.
  // @param sink The spdlog sink to attach.
  // @returns True on success, false if the named logger does not exist.
  bool addSink(const std::string& name, const spdlog::sink_ptr& sink) {
    auto it = m_loggers.find(name);
    if (it == m_loggers.end()) return false;
    it->second->sinks().push_back(sink);
    return true;
  }

  // Add a file sink to the default logger.
  // Creates or truncates the file at the given path.
  // @param path Path to the log file.
  void addFileSink(const std::string& path) { addSink(std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true)); }

  // Add a file sink to a named logger.
  // Creates or truncates the file at the given path.
  // @param name The name of the logger to add the sink to.
  // @param path Path to the log file.
  // @returns True on success, false if the named logger does not exist.
  bool addFileSink(const std::string& name, const std::string& path) { return addSink(name, std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true)); }

  // Set the log level of the default logger.
  // @param level The spdlog level to set.
  void setLevel(spdlog::level::level_enum level) { m_default->set_level(level); }

  // Returns the default logger.
  // @returns Reference to the default spdlog logger.
  spdlog::logger& logger() { return *m_default; }

  // Register a named logger for subsystem-specific logging.
  // If no sinks are provided, the default logger's sinks are inherited.
  // If a logger with the given name already exists, this is a no-op.
  // @param name The name to register the logger under.
  // @param sinks Optional list of sinks. Inherits default sinks if empty.
  void addLogger(const std::string& name, std::vector<spdlog::sink_ptr> sinks = {}) {
    if (sinks.empty()) {
      sinks = m_default->sinks();
    }
    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(m_default->level());
    m_loggers.emplace(name, std::move(logger));
  }

  // Returns the named logger, or the default logger if the name is not found.
  // @param name The name of the logger to retrieve.
  // @returns Reference to the named spdlog logger, or the default logger.
  spdlog::logger& logger(const std::string& name) {
    auto it = m_loggers.find(name);
    if (it == m_loggers.end()) return *m_default;
    return *it->second;
  }

  // Flush the default logger, writing any buffered log entries to its sinks.
  void flush() { m_default->flush(); }

  // Flush a named logger, writing any buffered log entries to its sinks.
  // If the named logger does not exist, flushes the default logger instead.
  // @param name The name of the logger to flush.
  void flush(const std::string& name) {
    auto it = m_loggers.find(name);
    if (it == m_loggers.end()) {
      m_default->flush();
      return;
    }
    it->second->flush();
  }

  private:
  // Constructs the default logger with stdout and stderr color sinks.
  Logger() {
    std::vector<spdlog::sink_ptr> sinks = {
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
    };
    m_default = std::make_shared<spdlog::logger>(DEFAULT_LOGGER_NAME, sinks.begin(), sinks.end());
    m_default->set_level(DEFAULT_LOG_LEVEL);
  }

  std::shared_ptr<spdlog::logger> m_default;
  std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> m_loggers;
};

// ---------------------------------------------------------------------------
// Logging macros
//
// Use LOG_X() to log to the default logger.
// Use LOG_X_TO(name, ...) to log to a named logger.
// All macros compile to nothing when LOGGING_DISABLED is set to 1.
// ---------------------------------------------------------------------------
#if LOGGING_DISABLED
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
#define LOG_CRITICAL(...) ((void)0)
#define LOG_DEBUG_TO(name, ...) ((void)0)
#define LOG_INFO_TO(name, ...) ((void)0)
#define LOG_WARN_TO(name, ...) ((void)0)
#define LOG_ERROR_TO(name, ...) ((void)0)
#define LOG_CRITICAL_TO(name, ...) ((void)0)
#define LOG_DEBUG_FLUSH(...) ((void)0)
#define LOG_INFO_FLUSH(...) ((void)0)
#define LOG_WARN_FLUSH(...) ((void)0)
#define LOG_ERROR_FLUSH(...) ((void)0)
#define LOG_CRITICAL_FLUSH(...) ((void)0)
#define LOG_DEBUG_FLUSH_TO(name, ...) ((void)0)
#define LOG_INFO_FLUSH_TO(name, ...) ((void)0)
#define LOG_WARN_FLUSH_TO(name, ...) ((void)0)
#define LOG_ERROR_FLUSH_TO(name, ...) ((void)0)
#define LOG_CRITICAL_FLUSH_TO(name, ...) ((void)0)
#else
// Default log
#define LOG_DEBUG(...) Logger::get().logger().debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::get().logger().info(__VA_ARGS__)
#define LOG_WARN(...) Logger::get().logger().warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::get().logger().error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::get().logger().critical(__VA_ARGS__)

// Named logging
#define LOG_DEBUG_TO(name, ...) Logger::get().logger(name).debug(__VA_ARGS__)
#define LOG_INFO_TO(name, ...) Logger::get().logger(name).info(__VA_ARGS__)
#define LOG_WARN_TO(name, ...) Logger::get().logger(name).warn(__VA_ARGS__)
#define LOG_ERROR_TO(name, ...) Logger::get().logger(name).error(__VA_ARGS__)
#define LOG_CRITICAL_TO(name, ...) Logger::get().logger(name).critical(__VA_ARGS__)

// Flushing log writes the message then immediately flushes to all sinks.
#define LOG_DEBUG_FLUSH(...) \
  do { \
    Logger::get().logger().debug(__VA_ARGS__); \
    Logger::get().flush(); \
  } while (0)
#define LOG_INFO_FLUSH(...) \
  do { \
    Logger::get().logger().info(__VA_ARGS__); \
    Logger::get().flush(); \
  } while (0)
#define LOG_WARN_FLUSH(...) \
  do { \
    Logger::get().logger().warn(__VA_ARGS__); \
    Logger::get().flush(); \
  } while (0)
#define LOG_ERROR_FLUSH(...) \
  do { \
    Logger::get().logger().error(__VA_ARGS__); \
    Logger::get().flush(); \
  } while (0)
#define LOG_CRITICAL_FLUSH(...) \
  do { \
    Logger::get().logger().critical(__VA_ARGS__); \
    Logger::get().flush(); \
  } while (0)

// Named flushing log writes the message then immediately flushes the named logger.
#define LOG_DEBUG_FLUSH_TO(name, ...) \
  do { \
    Logger::get().logger(name).debug(__VA_ARGS__); \
    Logger::get().flush(name); \
  } while (0)
#define LOG_INFO_FLUSH_TO(name, ...) \
  do { \
    Logger::get().logger(name).info(__VA_ARGS__); \
    Logger::get().flush(name); \
  } while (0)
#define LOG_WARN_FLUSH_TO(name, ...) \
  do { \
    Logger::get().logger(name).warn(__VA_ARGS__); \
    Logger::get().flush(name); \
  } while (0)
#define LOG_ERROR_FLUSH_TO(name, ...) \
  do { \
    Logger::get().logger(name).error(__VA_ARGS__); \
    Logger::get().flush(name); \
  } while (0)
#define LOG_CRITICAL_FLUSH_TO(name, ...) \
  do { \
    Logger::get().logger(name).critical(__VA_ARGS__); \
    Logger::get().flush(name); \
  } while (0)
#endif
