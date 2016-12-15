#pragma once

#include <memory>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "error.h"

namespace curlion {

class HttpForm {
public:
    class File {
    public:
        File() { }
        File(const std::string& path) : path(path) { }
        
        std::string path;
        std::string name;
        std::string content_type;
    };
    
    class Part {
    public:
        Part() { }
        Part(const std::string& name, const std::string& content) : name(name), content(content) { }
        
        std::string name;
        std::string content;
        std::vector<std::shared_ptr<File>> files;
    };
    
public:
    ~HttpForm();
    
    std::error_condition AddPart(const std::shared_ptr<Part>& part);
    
    curl_httppost* GetHandle() const {
        return handle_;
    }
    
private:
    curl_httppost* handle_ = nullptr;
    curl_httppost* handle_last_ = nullptr;
    
    std::vector<std::shared_ptr<Part>> parts_;
};
    
}