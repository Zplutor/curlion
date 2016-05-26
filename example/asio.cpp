#include <map>
#include <memory>
#include <iostream>
#include <boost/asio.hpp>
#include <curlion.h>

using namespace curlion;

/**
 The curlion::Timer implementation with boost::aio::deadline_timer.
 */
class AsioTimer : public Timer {
public:
    explicit AsioTimer(boost::asio::io_service& io_service) : timer_(io_service) {
        
    }
    
    void Start(long timeout_ms, const std::function<void()>& callback) override {
       
        timer_.expires_from_now(boost::posix_time::milliseconds(timeout_ms));
        timer_.async_wait(std::bind(&AsioTimer::TimerTriggered, this, callback, std::placeholders::_1));
    }
    
    void Stop() override {
        
        timer_.cancel();
    }
    
private:
    void TimerTriggered(const std::function<void()>& callback, const boost::system::error_code& error) {
        
        if (error == boost::asio::error::operation_aborted) {
            return;
        }
        
        callback();
    }
    
private:
    boost::asio::deadline_timer timer_;
};


/**
 An event watcher for a single socket.
 
 This class simulates a continual watching on the socket, with one-shot watching method of asio.
 */
class AsioSingleSocketWatcher : public std::enable_shared_from_this<AsioSingleSocketWatcher> {
public:
    AsioSingleSocketWatcher(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                            SocketWatcher::Event event,
                            const SocketWatcher::EventCallback& callback) :
        socket_(socket),
        event_(event),
        callback_(callback),
        is_stopped_(false) {
        
        
    }
    
    void Start() {
        
        Watch();
    }
    
    void Stop() {

        //Stop may be called inside an event callback, so a flag variable
        //is needed for stopping the watching.
        is_stopped_ = true;
        
        //The socket may be closed before calling Stop, check for
        //preventing exception.
        if (socket_->is_open()) {
            socket_->cancel();
        }
    }
    
private:
    void Watch() {
        
        bool watch_read_event = (event_ == SocketWatcher::Event::Read) || (event_ == SocketWatcher::Event::ReadWrite);
        bool watch_write_event = (event_ == SocketWatcher::Event::Write) || (event_ == SocketWatcher::Event::ReadWrite);
        
        if (watch_read_event) {
            
            auto bridge_callback = std::bind(&AsioSingleSocketWatcher::EventTriggered,
                                             this->shared_from_this(),
                                             false,
                                             std::placeholders::_1,
                                             std::placeholders::_2);
            
            socket_->async_read_some(boost::asio::null_buffers(), bridge_callback);
        }
        
        if (watch_write_event) {
            
            auto bridge_callback = std::bind(&AsioSingleSocketWatcher::EventTriggered,
                                             this->shared_from_this(),
                                             true,
                                             std::placeholders::_1,
                                             std::placeholders::_2);
            
            socket_->async_write_some(boost::asio::null_buffers(), bridge_callback);
        }
    }
    
    void EventTriggered(bool can_write,
                        const boost::system::error_code& error,
                        std::size_t size) {
        
        if (error == boost::asio::error::operation_aborted) {
            return;
        }
        
        callback_(socket_->native_handle(), can_write);
     
        //End watching once stopped.
        if (! is_stopped_) {
            Watch();
        }
    }
    
private:
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    SocketWatcher::Event event_;
    SocketWatcher::EventCallback callback_;
    bool is_stopped_;
};


/**
 The curlion::SocketFactory and curlion::SocketWatcher implementation with boost::asio::ip::tcp::socket.
 */
class AsioSocketManager : public SocketFactory, public SocketWatcher {
public:
    explicit AsioSocketManager(boost::asio::io_service& io_service) : io_service_(io_service) {
        
    }
    
    
    curl_socket_t Open(curlsocktype socket_type, const curl_sockaddr* address) override {
        
        if ( (socket_type != CURLSOCKTYPE_IPCXN) || (address->family != AF_INET) ) {
            return CURL_SOCKET_BAD;
        }
        
        auto asio_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_service_);
        
        boost::system::error_code error;
        asio_socket->open(boost::asio::ip::tcp::v4(), error);
        
        if (error) {
            return CURL_SOCKET_BAD;
        }
        
        sockets_.insert(std::make_pair(asio_socket->native_handle(), asio_socket));
        return asio_socket->native_handle();
    }
    
    
    bool Close(curl_socket_t socket) override {
        
        auto iterator = sockets_.find(socket);
        if (iterator == sockets_.end()) {
            return false;
        }
        
        auto asio_socket = iterator->second;
        sockets_.erase(iterator);
        
        boost::system::error_code error;
        asio_socket->close(error);
        return ! error;
    }
    
    
    void Watch(curl_socket_t socket, Event event, const EventCallback& callback) override {
        
        auto iterator = sockets_.find(socket);
        if (iterator == sockets_.end()){
            return;
        }
        
        auto watcher = std::make_shared<AsioSingleSocketWatcher>(iterator->second, event, callback);
        watcher->Start();
        watchers_.insert(std::make_pair(socket, watcher));
    }
    
    
    void StopWatching(curl_socket_t socket) override {
        
        auto iterator = watchers_.find(socket);
        if (iterator == watchers_.end()) {
            return;
        }
        
        auto watcher = iterator->second;
        watcher->Stop();
        watchers_.erase(iterator);
    }
    
private:
    std::map<curl_socket_t, std::shared_ptr<boost::asio::ip::tcp::socket>> sockets_;
    std::map<curl_socket_t, std::shared_ptr<AsioSingleSocketWatcher>> watchers_;
    boost::asio::io_service& io_service_;
};


int main(int argc, const char * argv[]) {
    
    boost::asio::io_service io_service;
    auto work = std::make_shared<boost::asio::io_service::work>(io_service);
    
    auto timer = std::make_shared<AsioTimer>(io_service);
    auto socket_manager = std::make_shared<AsioSocketManager>(io_service);
    
    ConnectionManager connection_manager(socket_manager, timer);
    
    auto connection = std::make_shared<HttpConnection>(socket_manager);
    connection->SetUrl("http://www.bing.com");
    connection->SetVerbose(true);
    connection->SetFinishedCallback([ &work ](const std::shared_ptr<Connection>& connection) {
        if (connection->GetResult() == CURLE_OK) {
            std::cout
                << connection->GetResponseCode() << std::endl
                << connection->GetResponseHeader() << std::endl
                << connection->GetResponseBody() << std::endl;
        }
        else {
            std::cout << "Connection failed with result: " << connection->GetResult() << std::endl;
        }
        work.reset();
    });
    
    connection_manager.StartConnection(connection);
    
    io_service.run();
    
    return 0;
}
