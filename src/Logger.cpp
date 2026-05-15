#include "Logger.h"
#include "Timestamp.h"
#include <iostream>

using namespace mymuduo;
Logger& Logger::getInstance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

void Logger::log(std::string_view msg) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    switch (logLevel_) {
    case INFO: std::cout << "[INFO]"; break;
    case ERROR: std::cout << "[ERROR]"; break;
    case FATAL: std::cout << "[FATAL]"; break;
    case DEBUG: std::cout << "[DEBUG]"; break;
    }

    std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
}
