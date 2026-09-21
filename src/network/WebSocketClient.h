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
    std::uint64_t connectionGeneration() const;
    bool sendInputSnapshot(std::uint32_t sequence, std::uint16_t buttons);

private:
    bool registerSocket(int fileDescriptor);
    void closeSocket(int fileDescriptor);
    bool sendOnSocket(int fileDescriptor, const std::string& frame);

    std::atomic<bool> stopRequested{false};
    std::atomic<ConnectionState> state{ConnectionState::Connecting};
    std::atomic<std::uint64_t> generation{0};
    std::mutex socketMutex;
    std::mutex sendMutex;
    int activeSocket = -1;
};

}
