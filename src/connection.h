#pragma once

#include <functional>
#include <memory>
#include <string>
#include <curl/curl.h>

namespace curlion {

/**
 Connection used to send request to remote peer and receive its response.
 It supports variety of network protocols, such as SMTP, IMAP and HTTP etc.
 
 Using Connection is simple, just follow steps below:
 
 Firstly, after creating an instance, call setter methods to configurate
 whatever needed, typically SetUrl and SetFinishedCallback.
 
 And then call ConnectionManager::StartConnection to start it. 
 
 While the connection is running, some callback will get called, like read 
 body callback and write body callback, to gain data to send or return data 
 that received. Especially, the finished callback got called once the connection
 is done.
 
 Finally, call getter methods to get whatever wanted, typically GetResult.
 
 Connection is not thread safe.
 
 For HTTP, there is a derived class HttpConnection provides setter and getter
 methods specific to HTTP.
 
 This is a encapsulation against libcurl's easy handle.
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    /**
     Original position for seeking request body.
     */
    enum class SeekOrigin {
        /**
         Begin of request body.
         */
        Begin,
        
        /**
         Current position of request body.
         */
        Current,
        
        /**
         End of request body.
         */
        End,
    };
    
    /**
     Callback prototype for seeking request body.
     
     @param origin 
        Original position for seeking.
     
     @param offset 
        Offset related to original position.
     
     @return 
        Whether the seeking is succeeded.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             SeekOrigin origin,
             curl_off_t offset)
    > SeekBodyCallback;

    /**
     Calback prototype for reading request body.
     
     @param connection 
        The Connection instance.
     
     @param body 
        Body's buffer. Data need to be copied to here.
     
     @param expected_length 
        How many bytes are expected. Is is also the length of buffer.
     
     @param actual_length 
        Return how many bytes are copied to buffer. It could be less than expected_length.
        Return 0 means whole request body is read.
     
     @return Whether the reading is succeeded. Return false would abort the connection.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             char* body,
             std::size_t expected_length,
             std::size_t& actual_length)
    > ReadBodyCallback;
    
    /**
     Callback prototype for writing response header.
     
     @param connection 
        The Connection instance.
     
     @param header 
        Buffer contains header data.
     
     @param length 
        Buffer's length.
     
     @return 
        Whether the writing is succeeded. Return false would abort the connection.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             const char* header,
             std::size_t length)
    > WriteHeaderCallback;
    
    /**
     Callback prototype for writing response body.
     
     @param connection 
        The Connection instance.
     
     @param body
        Buffer contains body data.
     
     @param length
        Buffer's length
     
     @return 
        Whether the writing is succeeded. Return false would abort the connection.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             const char* body,
             std::size_t length)
    > WriteBodyCallback;
    
    /**
     Callback prototype for progress meter.
     
     @param connection
        The Connection instance.
     
     @param total_download
        The total number of bytes expected to be downloaded.
     
     @param current_download
        The number of bytes downloaded so far.
     
     @param total_upload
        The total number of bytes expected to be uploaded.
     
     @param current_upload
        The number of bytes uploaded so far.
     
     @return
        Return false would abort the connection.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             curl_off_t total_download,
             curl_off_t current_download,
             curl_off_t total_upload,
             curl_off_t current_upload)
    > ProgressCallback;
    
    /**
     Callback prototype for connection finished.
     
     @param connection
        The Connection instance.
     */
    typedef std::function<void(const std::shared_ptr<Connection>& connection)> FinishedCallback;
    
public:
    /**
     Construct the Connection instance.
     */
    Connection();
    
    /**
     Destruct the Connection instance.
     
     Destructing a running connection will abort it immediately.
     */
    virtual ~Connection();
    
    /**
     Set whether to print detail information about connection to stdout.
     
     The default is false.
     */
    void SetVerbose(bool verbose);
    
    /**
     Set the URL used in connection.
     */
    void SetUrl(const std::string& url);
    
    /**
     Set the proxy used in connection.
     */
    void SetProxy(const std::string& proxy);
    
    /**
     Set authenticated account for proxy.
     */
    void SetProxyAccount(const std::string& username, const std::string& password);
    
    /**
     Set whether to connect to server only, don't tranfer any data.
     
     The default is false.
     */
    void SetConnectOnly(bool connect_only);
    
    /**
     Set whether to verify the peer's SSL certificate.
     
     The default is true.
     */
    void SetVerifyCertificate(bool verify);
    
    /**
     Set whether to verify the certificate's name against host.
     
     The default is true.
     */
    void SetVerifyHost(bool verify);
    
    /**
     Set the path of file holding one or more certificates to verify the peer with.
     */
    void SetCertificateFilePath(const std::string& file_path);
    
    /**
     Set the body for request.
     
     Note that the body would be ignored once a callable read body callback is set.
     */
    void SetRequestBody(const std::string& body) {
        request_body_ = body;
    }
    
    /**
     Set whether to receive response body.
     
     This option must be set to false for those responses without body,
     otherwise the connection would be blocked.
     
     The default is true.
     */
    void SetReceiveBody(bool receive_body);
    
    /**
     Set whether to enable the progress meter.
     
     When progress meter is disalbed, the progress callback would not be called.
     
     The default is false.
     */
    void SetEnableProgress(bool enable);
    
    /**
     Set timeout for the connect phase.
     
     Set to 0 to switch to the default timeout 300 seconds.
     */
    void SetConnectTimeoutInMilliseconds(long milliseconds);
    
    /**
     Set timeout for how long the connection can be idle.
     
     This option is a shortcut to SetLowSpeedTimeout, which low speed is set to 1.
     */
    void SetIdleTimeoutInSeconds(long seconds) {
        SetLowSpeedTimeout(1, seconds);
    }
    
    /**
     Set timeout for low speed transfer.
     
     If the average transfer speed is below specified speed for specified duration, the connection is 
     considered timeout.
     
     The default is no timeout. Set 0 to any one of the parameters to turn off the timeout.
     
     This option is equal to set both CURLOPT_LOW_SPEED_LIMIT and CURLOPT_LOW_SPEED_TIME options 
     to libcurl. Note that the timeout may be broken in some cases due to internal implementation 
     of libcurl.
     */
    void SetLowSpeedTimeout(long low_speed_in_bytes_per_seond, long timeout_in_seconds);
    
    /**
     Set timeout for the whole connection.
     
     The default is no timeout. Set 0 to turn off the timeout.
     */
    void SetTimeoutInMilliseconds(long milliseconds);
    
    /**
     Set callback for reading request body.
     
     If callback is callable, the request body set by SetRequestBody would be ignored.
     */
    void SetReadBodyCallback(const ReadBodyCallback& callback) {
        read_body_callback_ = callback;
    }
    
    /**
     Set callback for seeking request body.
     
     This callback is used to re-position the reading pointer while re-sending is needed.
     It should be provided along with read body callback.
     */
    void SetSeekBodyCallback(const SeekBodyCallback& callback) {
        seek_body_callback_ = callback;
    }
    
    /**
     Set callback for writing response header.
     
     If callback is callable, GetResponseHeader would return empty string.
     */
    void SetWriteHeaderCallback(const WriteHeaderCallback& callback) {
        write_header_callback_ = callback;
    }
    
    /**
     Set callback for writing response body.
     
     If callback is callable, GetResponseBody would return empty string.
     */
    void SetWriteBodyCallback(const WriteBodyCallback& callback) {
        write_body_callback_ = callback;
    }
    
    /**
     Set callback for progress meter.
     */
    void SetProgressCallback(const ProgressCallback& callback) {
        progress_callback_ = callback;
    }
    
    /**
     Set callback for connection finished.
     
     Use this callback to get informed when the connection finished.
     */
    void SetFinishedCallback(const FinishedCallback& callback) {
        finished_callback_ = callback;
    }
    
    /**
     Get the result code.
     
     An undefined value would be returned if the connection is not yet finished.
     */
    CURLcode GetResult() const {
        return result_;
    }
    
    /**
     Get the last response code.
     
     For HTTP, response code is the HTTP status code.
     
     The return value is undefined if the connection is not yet finished.
     */
    long GetResponseCode() const;
    
    /**
     Get response header.
     
     Note that empty string would be returned if a callable write header callback is set.
     Undefined string content would be returned if the connection is not yet finished.
     */
    const std::string& GetResponseHeader() const {
        return response_header_;
    }
    
    /**
     Get response body.
     
     Note that empty string would be returned if a callable write body callback is set.
     Undefined string content would be returned if the connection is not yet finished.
     */
    const std::string& GetResponseBody() const {
        return response_body_;
    }
    
    /**
     Get the underlying easy handle.
     */
    CURL* GetHandle() const {
        return handle_;
    }
    
//Methods be called from ConnectionManager.
private:
    void WillStart();
    void DidFinish(CURLcode result);
    
protected:
    virtual void ResetState();
    
private:
    static size_t CurlReadBodyCallback(char* buffer, size_t size, size_t nitems, void* instream);
    static int CurlSeekBodyCallback(void* userp, curl_off_t offset, int origin);
    static size_t CurlWriteHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    static size_t CurlWriteBodyCallback(char* ptr, size_t size, size_t nmemb, void* v);
    static int CurlProgressCallback(void *clientp,
                                    curl_off_t dltotal,
                                    curl_off_t dlnow,
                                    curl_off_t ultotal,
                                    curl_off_t ulnow);
  
    bool ReadBody(char* body, std::size_t expected_length, std::size_t& actual_length);
    bool SeekBody(SeekOrigin origin, curl_off_t offset);
    bool WriteHeader(const char* header, std::size_t length);
    bool WriteBody(const char* body, std::size_t length);
    bool Progress(curl_off_t total_download,
                  curl_off_t current_download,
                  curl_off_t total_upload,
                  curl_off_t current_upload);
    
private:
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

private:
    CURL* handle_;
    
    std::string request_body_;
    std::size_t request_body_read_length_;
    ReadBodyCallback read_body_callback_;
    SeekBodyCallback seek_body_callback_;
    WriteHeaderCallback write_header_callback_;
    WriteBodyCallback write_body_callback_;
    ProgressCallback progress_callback_;
    FinishedCallback finished_callback_;
    CURLcode result_;
    std::string response_header_;
    std::string response_body_;
    
    friend class ConnectionManager;
};

}
