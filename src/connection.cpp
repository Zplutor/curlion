#include "connection.h"
#include "socket_factory.h"
#include "log.h"

namespace curlion {

static inline LoggerProxy WriteConnectionLog(void* connection_identifier) {
    return Log() << "Connection(" << connection_identifier << "): ";
}
    
    
Connection::Connection(const std::shared_ptr<SocketFactory>& socket_factory) :
    socket_factory_(socket_factory),
    request_body_read_length_(0),
    result_(CURL_LAST) {
    
    handle_ = curl_easy_init();
        
    if (socket_factory_ != nullptr) {
        curl_easy_setopt(handle_, CURLOPT_OPENSOCKETFUNCTION, CurlOpenSocketCallback);
        curl_easy_setopt(handle_, CURLOPT_OPENSOCKETDATA, this);
        curl_easy_setopt(handle_, CURLOPT_CLOSESOCKETFUNCTION, CurlCloseSocketCallback);
        curl_easy_setopt(handle_, CURLOPT_CLOSESOCKETDATA, this);
    }
        
    curl_easy_setopt(handle_, CURLOPT_READFUNCTION, CurlReadBodyCallback);
    curl_easy_setopt(handle_, CURLOPT_READDATA, this);
    curl_easy_setopt(handle_, CURLOPT_SEEKFUNCTION, CurlSeekBodyCallback);
    curl_easy_setopt(handle_, CURLOPT_SEEKDATA, this);
    curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, CurlWriteHeaderCallback);
    curl_easy_setopt(handle_, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, CurlWriteBodyCallback);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, this);
}


Connection::~Connection() {
    
    curl_easy_cleanup(handle_);
}


void Connection::SetVerbose(bool verbose) {
    curl_easy_setopt(handle_, CURLOPT_VERBOSE, verbose);
}


void Connection::SetUrl(const std::string& url) {
    curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
}


void Connection::SetProxy(const std::string& proxy) {
    curl_easy_setopt(handle_, CURLOPT_PROXY, proxy.c_str());
}


void Connection::SetProxyAccount(const std::string& username, const std::string& password) {
    curl_easy_setopt(handle_, CURLOPT_PROXYUSERNAME, username.c_str());
    curl_easy_setopt(handle_, CURLOPT_PROXYPASSWORD, password.c_str());
}


void Connection::SetConnectOnly(bool connect_only) {
    curl_easy_setopt(handle_, CURLOPT_CONNECT_ONLY, connect_only);
}

void Connection::SetVerifyCertificate(bool verify) {
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER, verify);
}

void Connection::SetVerifyHost(bool verify) {
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYHOST, verify ? 2 : 0);
}

void Connection::SetCertificateFilePath(const std::string& file_path) {
    const char* path = file_path.empty() ? nullptr : file_path.c_str();
    curl_easy_setopt(handle_, CURLOPT_CAINFO, path);
}

void Connection::SetReceiveBody(bool receive_body) {
    curl_easy_setopt(handle_, CURLOPT_NOBODY, ! receive_body);
}


void Connection::SetConnectTimeoutInMilliseconds(long milliseconds) {
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS, milliseconds);
}

void Connection::SetIdleTimeoutInSeconds(long seconds) {
    curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_TIME, seconds);
    curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_LIMIT, seconds == 0 ? 0 : 1);
}

void Connection::SetTimeoutInMilliseconds(long milliseconds) {
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS, milliseconds);
}


void Connection::WillStart() {
    
    ResetState();
}


void Connection::ResetState() {
    
    request_body_read_length_ = 0;
    result_ = CURL_LAST;
    response_header_.clear();
    response_body_.clear();
}


void Connection::DidFinish(CURLcode result) {
    
    result_ = result;
    
    if (finished_callback_) {
        finished_callback_(*this);
    }
}


curl_socket_t Connection::OpenSocket(curlsocktype socket_type, curl_sockaddr* address) {
    
    WriteConnectionLog(this) << "Open socket for "
        << "type " << socket_type << "; "
        << "address family " << address->family << ", "
        << "socket type " << address->socktype << ", "
        << "protocol " << address->protocol << '.';
    
    curl_socket_t socket = socket_factory_->Open(socket_type, address);
    
    if (socket != CURL_SOCKET_BAD) {
        WriteConnectionLog(this) << "Socket(" << socket << ") is opened.";
    }
    else {
        WriteConnectionLog(this) << "Open socket failed.";
    }
    
    return socket;
}


bool Connection::CloseSocket(curl_socket_t socket) {
    
    WriteConnectionLog(this) << "Close socket(" << socket << ").";
    
    bool is_succeeded = socket_factory_->Close(socket);
    
    if (is_succeeded) {
        WriteConnectionLog(this) << "Socket(" << socket << ") is closed.";
    }
    else {
        WriteConnectionLog(this) << "Close socket(" << socket << ") failed.";
    }
    
    return is_succeeded;
}


bool Connection::ReadBody(char* body, std::size_t expected_length, std::size_t& actual_length) {
    
    WriteConnectionLog(this) << "Read body to buffer with size " << expected_length << '.';
    
    bool is_succeeded = false;
    
    if (read_body_callback_) {
        is_succeeded = read_body_callback_(*this, body, expected_length, actual_length);
    }
    else {
    
        std::size_t remain_length = request_body_.length() - request_body_read_length_;
        actual_length = std::min(remain_length, expected_length);
        
        std::memcpy(body, request_body_.data() + request_body_read_length_, actual_length);
        request_body_read_length_ += actual_length;
        
        is_succeeded = true;
    }
    
    if (is_succeeded) {
        WriteConnectionLog(this) << "Read body done with size " << actual_length << '.';
    }
    else {
        WriteConnectionLog(this) << "Read body failed.";
    }
    
    return is_succeeded;
}


bool Connection::SeekBody(SeekOrigin origin, curl_off_t offset) {
 
    WriteConnectionLog(this)
        << "Seek body from "
        << (origin == SeekOrigin::End ? "end" :
           (origin == SeekOrigin::Current ? "current" : "begin"))
        << " to offset " << offset << '.';

    bool is_succeeded = false;
    
    if (read_body_callback_) {
        
        if (seek_body_callback_) {
            is_succeeded = seek_body_callback_(*this, origin, offset);
        }
    }
    else {

        std::size_t original_position = 0;
        switch (origin) {
            case SeekOrigin::Begin:
                break;
            case SeekOrigin::Current:
                original_position = request_body_read_length_;
                break;
            case SeekOrigin::End:
                original_position = request_body_.length();
                break;
            default:
                //Shouldn't reach here.
                break;
        }
        
        std::size_t new_read_length = original_position + offset;
        if (new_read_length <= request_body_.length()) {
            request_body_read_length_ = new_read_length;
            is_succeeded = true;
        }
    }
    
    WriteConnectionLog(this) << "Seek body " << (is_succeeded ? "done" : "failed") << '.';
    
    return is_succeeded;
}


bool Connection::WriteHeader(const char* header, std::size_t length) {
    
    WriteConnectionLog(this) << "Write header with size " << length << '.';
    
    bool is_succeeded = false;
    
    if (write_header_callback_) {
        is_succeeded = write_header_callback_(*this, header, length);
    }
    else {
        response_header_.append(header, length);
        is_succeeded = true;
    }
    
    WriteConnectionLog(this) << "Write header " << (is_succeeded ? "done" : "failed") << '.';
    
    return is_succeeded;
}


bool Connection::WriteBody(const char* body, std::size_t length) {

    WriteConnectionLog(this) << "Write body with size " << length << '.';

    bool is_succeeded = false;

    if (write_body_callback_) {
        is_succeeded = write_body_callback_(*this, body, length);
    }
    else {
        response_body_.append(body, length);
        is_succeeded = true;
    }
    
    WriteConnectionLog(this) << "Write body " << (is_succeeded ? "done" : "failed") << '.';

    return is_succeeded;
}


curl_socket_t Connection::CurlOpenSocketCallback(void* clientp, curlsocktype socket_type, struct curl_sockaddr* address) {
    Connection* connection = static_cast<Connection*>(clientp);
    return connection->OpenSocket(socket_type, address);
}

int Connection::CurlCloseSocketCallback(void* clientp, curl_socket_t socket) {
    Connection* connection = static_cast<Connection*>(clientp);
    bool is_succeeded = connection->CloseSocket(socket);
    return is_succeeded ? 0 : 1;
}

size_t Connection::CurlReadBodyCallback(char* buffer, size_t size, size_t nitems, void* instream) {
    Connection* connection = static_cast<Connection*>(instream);
    std::size_t actual_read_length = 0;
    bool is_succeeded = connection->ReadBody(buffer, size * nitems, actual_read_length);
    return is_succeeded ? actual_read_length : CURL_READFUNC_ABORT;
}

int Connection::CurlSeekBodyCallback(void* userp, curl_off_t offset, int origin) {
    SeekOrigin seek_origin = SeekOrigin::Begin;
    switch (origin) {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            seek_origin = SeekOrigin::Current;
            break;
        case SEEK_END:
            seek_origin = SeekOrigin::End;
        default:
            return 1;
    }
    Connection* connection = static_cast<Connection*>(userp);
    bool is_succeeded = connection->SeekBody(seek_origin, offset);
    return is_succeeded ? 0 : 1;
}

size_t Connection::CurlWriteHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    std::size_t length = size * nitems;
    Connection* connection = static_cast<Connection*>(userdata);
    bool is_succeeded = connection->WriteHeader(buffer, length);
    return is_succeeded ? length : 0;
}

size_t Connection::CurlWriteBodyCallback(char* ptr, size_t size, size_t nmemb, void* v) {
    std::size_t length = size * nmemb;
    Connection* connection = static_cast<Connection*>(v);
    bool is_succeeded = connection->WriteBody(ptr, length);
    return is_succeeded ? length : 0;
}

}