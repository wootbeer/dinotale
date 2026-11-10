#pragma once
// Stand-in for DXVK's logger (BattleTanx GA port): the shader translator's
// messages are dropped.
#include <cstdint>
#include <string>
namespace dxvk {
  enum class LogLevel : uint32_t { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, None = 5 };
  class Logger {
  public:
    static void trace(const std::string&) { }
    static void debug(const std::string&) { }
    static void info (const std::string&) { }
    static void warn (const std::string&) { }
    static void err  (const std::string&) { }
    static void log  (LogLevel, const std::string&) { }
    static LogLevel logLevel() { return LogLevel::None; }
  };
}
