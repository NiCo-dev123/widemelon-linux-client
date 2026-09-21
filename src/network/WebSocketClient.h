#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

#include "config/Config.h"

namespace widemelon
{

enum class ConnectionState
{
    Connecting,
    Connected,
    Failed,
    Stopped,
};

class WebSocketClient
{
public:
    WebSocketClient() = default;
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    std::string connectAndAuthenticate(const Config& config, std::chrono::seconds timeout);
    void requestStop();
    ConnectionState connectionState() const;

private:
    bool registerSocket(int fileDescriptor);
    void closeSocket(int fileDescriptor);

    std::atomic<bool> stopRequested{false};
    std::atomic<ConnectionState> state{ConnectionState::Connecting};
    std::mutex socketMutex;
    int activeSocket = -1;
};

}
