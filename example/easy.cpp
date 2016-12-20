/**
 This example shows how to send a HTTP request in blocking manner,
 which is the easiest way to use curlion.
 */

#include <iostream>
#include <curlion.h>

using namespace curlion;

static bool WriteBodyCallback(const std::shared_ptr<Connection>& connection,
                              const char* body,
                              std::size_t length);

static std::string g_body;

int main(int argc, const char * argv[]) {

    auto connection = std::make_shared<Connection>();
    connection->SetVerbose(true);
    connection->SetUrl("http://www.qq.com");
    connection->SetWriteBodyCallback(WriteBodyCallback);
    
    connection->Start();
    
    auto result = connection->GetResult();
    
    std::cout
        << "Result: " << result << std::endl
        << "Body: " << std::endl << g_body << std::endl;
}


static bool WriteBodyCallback(const std::shared_ptr<Connection>& connection,
                              const char* body,
                              std::size_t length) {
    
    g_body.append(body, length);
    return true;
}