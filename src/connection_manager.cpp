#include "connection_manager.h"
#include "connection.h"
#include "log.h"
#include "socket_watcher.h"
#include "timer.h"

namespace curlion {
    
static inline LoggerProxy WriteManagerLog(void* manager_idenditifier) {
    return Log() << "Manager(" << manager_idenditifier << "): ";
}
    

ConnectionManager::ConnectionManager(const std::shared_ptr<SocketWatcher>& socket_watcher,
                                     const std::shared_ptr<Timer>& timer) :
    socket_watcher_(socket_watcher),
    timer_(timer) {
    
    multi_handle_ = curl_multi_init();
    curl_multi_setopt(multi_handle_, CURLMOPT_TIMERFUNCTION, CurlTimerCallback);
    curl_multi_setopt(multi_handle_, CURLMOPT_TIMERDATA, this);
    curl_multi_setopt(multi_handle_, CURLMOPT_SOCKETFUNCTION, CurlSocketCallback);
    curl_multi_setopt(multi_handle_, CURLMOPT_SOCKETDATA, this);
}


ConnectionManager::~ConnectionManager() {
    
    
}


void ConnectionManager::StartConnection(const std::shared_ptr<Connection>& connection) {
    
    CURL* easy_handle = connection->handle_;
    
    auto iterator = running_connections_.find(easy_handle);
    if (iterator != running_connections_.end()) {
        WriteManagerLog(this) << "Try to start an already running connection(" << connection.get() << "). Ignored.";
        return;
    }
    
    WriteManagerLog(this) << "Start a connection(" << connection.get() << ").";
    
    connection->WillStart();
    
    running_connections_.insert(std::make_pair(easy_handle, connection));
    curl_multi_add_handle(multi_handle_, easy_handle);
}


void ConnectionManager::AbortConnection(const std::shared_ptr<Connection>& connection) {
    
    CURL* easy_handle = connection->handle_;
    
    auto iterator = running_connections_.find(easy_handle);
    if (iterator == running_connections_.end()) {
        WriteManagerLog(this) << "Try to abort a not running connection(" << easy_handle << "). Ignored.";
        return;
    }
    
    WriteManagerLog(this) << "Abort a connection(" << easy_handle << ").";
    
    running_connections_.erase(easy_handle);
    curl_multi_remove_handle(multi_handle_, easy_handle);
}


void ConnectionManager::SetTimer(long timeout_ms) {
    
    WriteManagerLog(this) << "Set timer for " << timeout_ms << " milliseconds.";
    
    timer_->Stop();
    
    if (timeout_ms >= 0) {
        timer_->Start(timeout_ms, std::bind(&ConnectionManager::TimerTriggered, this));
    }
}


void ConnectionManager::TimerTriggered() {
    
    WriteManagerLog(this) << "Timer triggered.";
    
    int running_count = 0;
    curl_multi_socket_action(multi_handle_, CURL_SOCKET_TIMEOUT, 0, &running_count);
    CheckFinishedConnections();
}


void ConnectionManager::WatchSocket(curl_socket_t socket, int action, void* socket_pointer) {
    
    static void* const kIsNotNewSocketTag = reinterpret_cast<void*>(1);
    
    //Ensure that StopWatching won't be called with a new socket that never watched.
    if (socket_pointer == kIsNotNewSocketTag) {
        WriteManagerLog(this) << "Socket(" << socket << ") is changed. Stop watching it.";
        socket_watcher_->StopWatching(socket);
    }
    else {
        WriteManagerLog(this) << "Socket(" << socket << ") is added.";
        curl_multi_assign(multi_handle_, socket, kIsNotNewSocketTag);
    }
    
    if (action == CURL_POLL_REMOVE) {
        WriteManagerLog(this) << "Socket(" << socket << ") is removed.";
        return;
    }
    
    SocketWatcher::Event event = SocketWatcher::Event::Read;
    switch (action) {
        case CURL_POLL_IN:
            break;
            
        case CURL_POLL_OUT:
            event = SocketWatcher::Event::Write;
            break;
            
        case CURL_POLL_INOUT:
            event = SocketWatcher::Event::ReadWrite;
            break;
    }
    
    WriteManagerLog(this) << "Watch socket(" << socket << ") for "
        << (event == SocketWatcher::Event::Read ? "read" :
           (event == SocketWatcher::Event::Write ? "write" : "read/write"))
        << " event.";
    
    socket_watcher_->Watch(socket, event, std::bind(&ConnectionManager::SocketEventTriggered,
                                                    this,
                                                    std::placeholders::_1,
                                                    std::placeholders::_2));
}


void ConnectionManager::SocketEventTriggered(curl_socket_t socket, bool can_write) {
    
    WriteManagerLog(this) << "Socket(" << socket << ") " << (can_write ? "write" : "read") << " event triggered.";
    
    int action = can_write ? CURL_CSELECT_OUT : CURL_CSELECT_IN;
    int running_count = 0;
    curl_multi_socket_action(multi_handle_, socket, action, &running_count);
    CheckFinishedConnections();
}


void ConnectionManager::CheckFinishedConnections() {
    
    while (true) {
        
        int msg_count = 0;
        CURLMsg* msg = curl_multi_info_read(multi_handle_, &msg_count);
        if (msg == nullptr) {
            break;
        }
        
        if (msg->msg == CURLMSG_DONE) {
            
            auto iterator = running_connections_.find(msg->easy_handle);
            if (iterator != running_connections_.end()) {
                
                auto connection = iterator->second;
                running_connections_.erase(iterator);
                
                WriteManagerLog(this)
                    << "Connection(" << connection.get() << ") is finished with result " << msg->data.result << '.';
                
                connection->DidFinish(msg->data.result);
            }
        }
    }
}


int ConnectionManager::CurlTimerCallback(CURLM* multi_handle, long timeout_ms, void* user_pointer) {
    
    ConnectionManager* manager = static_cast<ConnectionManager*>(user_pointer);
    manager->SetTimer(timeout_ms);
    return 0;
}


int ConnectionManager::CurlSocketCallback(CURL* easy_handle,
                                          curl_socket_t socket,
                                          int action,
                                          void* user_pointer,
                                          void* socket_pointer) {
    
    ConnectionManager* manager = static_cast<ConnectionManager*>(user_pointer);
    manager->WatchSocket(socket, action, socket_pointer);
    return 0;
}

}