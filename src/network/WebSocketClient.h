#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

#include "config/Config.h"

namespace widemelon
{

class WebSocketClient
{
public:
    WebSocketClient() = default;
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    std::string connectAndAuthenticate(const Config& config, std::chrono::seconds timeout);
    void requestStop();

private:
    bool registerSocket(int fileDescriptor);
    void closeSocket(int fileDescriptor);

    std::atomic<bool> stopRequested{false};
    std::mutex socketMutex;
    int activeSocket = -1;
};

}
