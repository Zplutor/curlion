#pragma once

/**
 CURLPP_VERBOSE macro controls whether to print debug information to stdout.
 Enable it by changing its value to none zero.
 
 Be aware of that enabling this macro would produce much output that flood the console easily.
 So it should be used for debug purpose only. 
 
 This macro effects only when NDEBUG macro is not defined.
 */
#define CURLPP_VERBOSE 0

#if (! defined(NDEBUG)) && (CURLPP_VERBOSE)
#include <memory>
#include <iostream>
#include <sstream>
#endif

namespace curlpp {
    
#if (! defined(NDEBUG)) && (CURLPP_VERBOSE)
    
class Logger {
public:
    void Write(const std::string& log) {
        std::cout << "curlpp> " << log << std::endl;
    }
};

    
class LoggerProxy {
public:
    LoggerProxy(const std::shared_ptr<Logger>& logger) : logger_(logger), stream_(new std::ostringstream()) {
        
    }
    
    LoggerProxy(LoggerProxy&& other) : logger_(std::move(other.logger_)), stream_(std::move(other.stream_)) {
        
    }
    
    ~LoggerProxy() {
        
        if ( (logger_ != nullptr) && (stream_ != nullptr) ) {
            logger_->Write(stream_->str());
        }
    }
    
    template<typename Type>
    LoggerProxy operator<<(const Type& value) {
        *stream_ << value;
        return std::move(*this);
    }
    
private:
    std::shared_ptr<Logger> logger_;
    std::unique_ptr<std::ostringstream> stream_;
};
    

inline LoggerProxy Log() {
    return LoggerProxy(std::make_shared<Logger>());
}
    
#else
    
class LoggerProxy {
public:
    template<typename Type>
    LoggerProxy operator<<(const Type& value) {
        return *this;
    }
};
    
inline LoggerProxy Log() {
    return LoggerProxy();
}
    
#endif
    
}