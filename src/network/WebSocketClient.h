#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
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
    WebSocketClient();
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
    void clearVideoFrames();
#ifdef WIDEMELON_HAVE_JPEG
    struct FrameAck
    {
        std::uint32_t sequence = 0;
        double decodeMs = 0;
        bool decoded = false;
    };
    struct PendingVideoFrame
    {
        VideoJpegFrame frame;
        std::uint64_t epoch = 0;
    };
    void queueVideoFrame(VideoJpegFrame frame);
    void decodeLoop();
    bool flushFrameAcks(int fileDescriptor);
#endif

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
#ifdef WIDEMELON_HAVE_JPEG
    std::condition_variable videoCondition;
    std::optional<PendingVideoFrame> pendingJpeg;
    std::vector<FrameAck> completedAcks;
    std::thread decoderThread;
#endif
    std::uint64_t videoEpoch = 0;
    int activeSocket = -1;
};

}
