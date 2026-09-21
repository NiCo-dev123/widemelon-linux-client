#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "config/Config.h"
#include "protocol/WideMelonProtocol.h"

namespace widemelon
{

enum class ConnectionState
{
    Connecting,
    Connected,
    Failed,
    Stopped,
};

struct DecodedVideoFrame
{
    std::uint32_t sequence = 0;
    std::uint64_t capturedUs = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> rgb;
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
    bool latestJpegFrame(VideoJpegFrame& frame) const;
    bool latestDecodedVideoFrame(DecodedVideoFrame& frame) const;

private:
    bool registerSocket(int fileDescriptor);
    void closeSocket(int fileDescriptor);
    bool sendOnSocket(int fileDescriptor, const std::string& frame);

    std::atomic<bool> stopRequested{false};
    std::atomic<ConnectionState> state{ConnectionState::Connecting};
    std::atomic<std::uint64_t> generation{0};
    std::mutex socketMutex;
    std::mutex sendMutex;
    mutable std::mutex videoMutex;
    VideoJpegFrame latestJpeg;
    DecodedVideoFrame latestDecoded;
    bool hasJpeg = false;
    bool hasDecoded = false;
    int activeSocket = -1;
};

}
