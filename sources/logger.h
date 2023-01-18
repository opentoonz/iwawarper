#pragma once

#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>
#include <string>

#define ENABLE_LOG
#undef ENABLE_LOG  // comment this line to enable log

#ifdef ENABLE_LOG
#include <Windows.h>
#endif

class Logger {
public:
#ifdef ENABLE_LOG
  static void Initialize(const std::string& _fileName) {
    Logger::fileName = _fileName;
    file             = fopen(_fileName.data(), "w");
    if (!file) exit(0);
  }
  static void Write(const std::string& log) {
    tm* newTime;
    __time64_t longTime;
    _time64(&longTime);
    newTime = _localtime64(&longTime);

    fprintf(file, "[ %02d��%02d�� - %02d:%02d:%02d ]: %s\n",
            newTime->tm_mon + 1, newTime->tm_mday, newTime->tm_hour,
            newTime->tm_min, newTime->tm_sec, log.c_str());
    fflush(file);
  }
#else
  static void Initialize(const std::string& /*_fileName*/) {}
  static void Write(const std::string& /*log*/) {}
#endif

protected:
  static std::string fileName;
  static FILE* file;
};

#endif
