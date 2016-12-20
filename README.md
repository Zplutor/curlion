# Curlion

## Introduction

Curlion is a C++ wrapper for libcurl. It provides an easy-to-use interface.

## Requirement

A C++11 compatible compiler is required. IDEs listed below are supported:

* XCode 6.0 or greater.
* Visual Studio 2013 or greater. 

If you use curlion in non-blocking manner, an event-driven mechanism is also required, such as `boost.asio`, `libevent` and so on.

## Usage

There are two manners to use curlion: blocking and non-blocking. Blocking manner corresponds to the easy interface of libcurl, and non-blocking manner corresponds to the multi-socket interface.

### Blocking manner

This manner is very simple, just like the code shows below:

	//Create a connection and set a target URL.	
	auto connection = std::make_shared<curlion::Connection>();
	connection->SetUrl("http://www.qq.com");
	
	//Start the connection, this call would not return until it is finished.
	connection->Start();
	
	//Get result code. 
	auto result = connection->GetResult();

	//Get response body.
	auto response_body = connection->GetResponseBody();
	
No more explaination is needed.

### Non-blocking manner

This manner is more complicated and it needs you to implement an event-driven mechanism. Curlion provides some interfaces to help simplifying the implementation.

#### Implement Timer

The first interface is `curlion::Timer`, which is used to start and stop a timer. Implement this interface like below:

	class MyTimer : public curlion::Timer {
	public:    
	    void Start(long timeout_ms, const std::function<void()>& callback) override {
	        //Start a timer which expires in timeout_ms, then call callback to notify curlion.
	    }	
    
	    void Stop() override {
	        //Stop the timer which starts in Start method.
	    }
	};

#### Implement SocketWatcher

The second interface is `curlion::SocketWatcher`, which is used to monitor events on sockets. Implement this interface like below:

	class MySocketWatcher : public curlion::SocketWatcher {
	public:
	    void Watch(curl_socket_t socket, Event event, const EventCallback& callback) override {
	        //Start a monitor to watch the socket on event. 
	        //When the event is triggered, call callback to notify curlion.
	    }
    
	    void StopWatching(curl_socket_t socket) override {
	        //Stop the monitor which starts in Watch method.
	    }
	};
	
#### Implement SocketFactory
	
An optional interface is `curlion::SocketFactory`, which is used to open and close sockets. If you need to manage sockets by yourself, implement this interface like below:

	class MySocketFactory : public curlion::SocketFactory {
	public:
	    curl_socket_t Open(curlsocktype socket_type, const curl_sockaddr* address) override {
	        //Open a socket with specified type and address.
	    }
    
	    bool Close(curl_socket_t socket) override {
	        //Close the specified socket.
	    }
	};
	
#### Use ConnectionManager

After implementing the event-driven mechanism, all of the reset are simple. Here are steps for accomplishing a HTTP request.

Firstly, create a `curlion::ConnectionManager` with instances of `curlion::Timer`, `curlion::SocketWatcher` and `curlion::SocketFactory`:

	auto timer = std::make_shared<MyTimer>();
	auto socket_watcher = std::make_shared<MySocketWatcher>();
	auto socket_factory = std::make_shared<MySocketFactory>();
    
	curlion::ConnectionManager connection_manager(socket_factory, socket_watcher, timer);

Secondly, create a `curlion::HttpConnection`, and configurate its options:

	auto connection = std::make_shared<curlion::HttpConnection>();
	connection->SetUrl("http://www.qq.com");
	connection->SetFinishedCallback([](const std::shared_ptr<Connection>& connection) {
	
            if (connection->GetResult() == CURLE_OK) {
                std::cout << connection->GetResponseBody() << std::endl;
            }
            else {
                std::cout << "Connection failed with result: " << connection->GetResult() << std::endl;
            }
	});

Finally, start the connection just with a single method call:

	connection_manager.StartConnection(connection);
	
When connection is finished, the callback set by `SetFinishedCallback` method will be called. Get results just like the code shows.

## Example

There are some examples show how to use curlion in example directory.

## Documentation

Curlion contains brief documentation in source files. A HTML version documentation can be generated by doxygen using doc/doxygen script file.

More detail documentation about libcurl can be found at [libcurl's documentation page](http://curl.haxx.se/libcurl/c/).
