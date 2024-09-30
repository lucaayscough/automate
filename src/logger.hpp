#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "utils.hpp"

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_TRACE 1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_WARN  3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_FATAL 5
#define LOG_LEVEL_USER  6

#ifndef LOG_LEVEL
#ifdef JUCE_DEBUG
#define LOG_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LEVEL LOG_LEVEL_TRACE
#endif
#endif

namespace atmt {

enum class LogLevel {
  debug = LOG_LEVEL_DEBUG,
  trace = LOG_LEVEL_TRACE,
  info  = LOG_LEVEL_INFO,
  warn  = LOG_LEVEL_WARN,
  error = LOG_LEVEL_ERROR,
  fatal = LOG_LEVEL_FATAL,
};

struct Logger : juce::Logger {
  Logger() {
    fileLogger.reset(juce::FileLogger::createDefaultAppLogger("Automate", "Log.txt", ""));
    jassert(fileLogger.get() != nullptr);
    juce::Logger::setCurrentLogger(this);

    //info("BuildId:         " + BuildInfo::gitHash + " " + BuildInfo::buildType);
    //info("CompilationDate: " + BuildInfo::compilationDate);
  }

  ~Logger() override {
    juce::Logger::setCurrentLogger(nullptr);
  }

  static juce::File getLogFile() {
    if (auto* logger = dynamic_cast<Logger*>(juce::Logger::getCurrentLogger()))
    {
      if (logger->fileLogger)
        return logger->fileLogger->getLogFile();
    }

    return {};
  }

  static juce::String getFormattedTime() {
    auto currentTime = juce::Time::getCurrentTime();
    juce::String formattedTime;

    formattedTime << "[" << currentTime.toString(true, true, true, true) << " "
                  << currentTime.getTimeZone() + "]";

    return formattedTime;
  }

  static void debug(const juce::String& message) { log(LogLevel::debug, message); }
  static void trace(const juce::String& message) { log(LogLevel::trace, message); }
  static void info (const juce::String& message) { log(LogLevel::info,  message); }
  static void warn (const juce::String& message) { log(LogLevel::warn,  message); }
  static void error(const juce::String& message) { log(LogLevel::error, message); }
  static void fatal(const juce::String& message) { log(LogLevel::fatal, message); }

  static void log(LogLevel level, juce::String message) {
    if (LOG_LEVEL > (int)level)
      return;

    if (auto* logger = dynamic_cast<Logger*>(juce::Logger::getCurrentLogger())) {
      juce::String levelString;

      switch (level) {
        case LogLevel::debug: levelString = "[DEBUG]"; break;
        case LogLevel::trace: levelString = "[TRACE]"; break;
        case LogLevel::info:  levelString = "[INFO] "; break;
        case LogLevel::warn:  levelString = "[WARN] "; break;
        case LogLevel::error: levelString = "[ERROR]"; break;
        case LogLevel::fatal: levelString = "[FATAL]"; break;
      }

      juce::String formattedMessage = levelString + " " + getFormattedTime() + " " + message;
      juce::Logger::writeToLog(formattedMessage);
    }
  }

  void logMessage(const juce::String& message) override {
    fileLogger->logMessage(message);
  }

  std::unique_ptr<juce::FileLogger> fileLogger;
};

} // namespace atmt
