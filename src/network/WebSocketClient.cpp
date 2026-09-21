#include "network/WebSocketClient.h"

#include "common/Logger.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <limits>

namespace
{

constexpr std::size_t MaximumFramePayload = 8 * 1024 * 1024;

short pollSocket(int fd, short events, int timeoutMs)
{
    pollfd value{fd, events, 0};
    return poll(&value, 1, timeoutMs) > 0 ? value.revents : 0;
}

bool sendAll(int fd, const std::string& bytes, const std::atomic<bool>& stopped)
{
    std::size_t sent = 0;
    while (sent < bytes.size() && !stopped.load())
    {
        const short events = pollSocket(fd, POLLOUT, 100);
        if (events & (POLLERR | POLLHUP | POLLNVAL)) return false;
        if (!(events & POLLOUT)) continue;
        const ssize_t count = send(fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (count <= 0) return false;
        sent += static_cast<std::size_t>(count);
    }
    return sent == bytes.size();
}

std::string clientFrame(unsigned char opcode, const std::string& payload)
{
    const std::array<unsigned char, 4> mask{0x51, 0x27, 0xA4, 0xD8};
    std::string frame(1, static_cast<char>(0x80 | opcode));
    if (payload.size() <= 125)
        frame.push_back(static_cast<char>(0x80 | payload.size()));
    else if (payload.size() <= std::numeric_limits<std::uint16_t>::max())
    {
        frame.push_back(static_cast<char>(0x80 | 126));
        frame.push_back(static_cast<char>((payload.size() >> 8) & 0xff));
        frame.push_back(static_cast<char>(payload.size() & 0xff));
    }
    else
    {
        frame.push_back(static_cast<char>(0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8)
            frame.push_back(static_cast<char>((static_cast<std::uint64_t>(payload.size()) >> shift) & 0xff));
    }
    for (unsigned char value : mask) frame.push_back(static_cast<char>(value));
    for (std::size_t i = 0; i < payload.size(); ++i)
        frame.push_back(static_cast<char>(static_cast<unsigned char>(payload[i]) ^ mask[i % mask.size()]));
    return frame;
}

enum class FrameResult { Incomplete, Ready, Invalid };

struct Frame
{
    unsigned char opcode = 0;
    bool final = false;
    std::string payload;
};

FrameResult takeFrame(std::string& bytes, Frame& frame)
{
    if (bytes.size() < 2) return FrameResult::Incomplete;
    const unsigned char first = static_cast<unsigned char>(bytes[0]);
    const unsigned char second = static_cast<unsigned char>(bytes[1]);
    std::size_t headerSize = 2;
    std::uint64_t payloadSize = second & 0x7f;
    if (payloadSize == 126)
    {
        if (bytes.size() < 4) return FrameResult::Incomplete;
        payloadSize = (static_cast<unsigned char>(bytes[2]) << 8) | static_cast<unsigned char>(bytes[3]);
        headerSize = 4;
    }
    else if (payloadSize == 127)
    {
        if (bytes.size() < 10) return FrameResult::Incomplete;
        payloadSize = 0;
        for (std::size_t i = 2; i < 10; ++i)
            payloadSize = (payloadSize << 8) | static_cast<unsigned char>(bytes[i]);
        headerSize = 10;
    }
    const bool masked = (second & 0x80) != 0;
    if (payloadSize > MaximumFramePayload || payloadSize > std::numeric_limits<std::size_t>::max())
        return FrameResult::Invalid;
    if (masked) headerSize += 4;
    const std::size_t size = static_cast<std::size_t>(payloadSize);
    if (bytes.size() < headerSize || size > bytes.size() - headerSize) return FrameResult::Incomplete;

    frame.opcode = first & 0x0f;
    frame.final = (first & 0x80) != 0;
    frame.payload.assign(bytes.data() + headerSize, size);
    if (masked)
    {
        const std::size_t maskOffset = headerSize - 4;
        for (std::size_t i = 0; i < frame.payload.size(); ++i)
            frame.payload[i] = static_cast<char>(static_cast<unsigned char>(frame.payload[i])
                ^ static_cast<unsigned char>(bytes[maskOffset + i % 4]));
    }
    bytes.erase(0, headerSize + size);
    return FrameResult::Ready;
}

bool readHttpResponse(int fd, std::string& response, const std::atomic<bool>& stopped)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    std::array<char, 1024> buffer{};
    while (!stopped.load() && std::chrono::steady_clock::now() < deadline)
    {
        const short events = pollSocket(fd, POLLIN, 100);
        if (events & (POLLERR | POLLHUP | POLLNVAL)) return false;
        if (!(events & POLLIN)) continue;
        const ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);
        if (count <= 0) return false;
        response.append(buffer.data(), static_cast<std::size_t>(count));
        if (response.find("\r\n\r\n") != std::string::npos) return true;
        if (response.size() > 16 * 1024) return false;
    }
    return false;
}

std::string jsonNumber(const std::string& message, const std::string& name)
{
    const std::size_t key = message.find('"' + name + '"');
    if (key == std::string::npos) return {};
    const std::size_t colon = message.find(':', key + name.size() + 2);
    if (colon == std::string::npos) return {};
    const std::size_t first = message.find_first_not_of(" \t\r\n", colon + 1);
    if (first == std::string::npos) return {};
    const std::size_t last = message.find_first_of(",}\r\n", first);
    return message.substr(first, last == std::string::npos ? std::string::npos : last - first);
}

bool replyToApplicationPing(int fd, const std::string& message, const std::atomic<bool>& stopped)
{
    const std::string sent = jsonNumber(message, "sent");
    if (sent.empty()) return false;
    return sendAll(fd, clientFrame(0x1, "{\"v\":2,\"type\":\"pong\",\"sent\":" + sent + '}'), stopped);
}

bool acknowledgeFrame(int fd, const std::string& frame, const std::atomic<bool>& stopped)
{
    if (frame.size() < 8) return false;
    const std::uint32_t sequence = static_cast<unsigned char>(frame[4])
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(frame[5])) << 8)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(frame[6])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(frame[7])) << 24);
    return sendAll(fd, clientFrame(0x1, "{\"v\":2,\"type\":\"frameAck\",\"seq\":"
        + std::to_string(sequence) + ",\"decodeMs\":0}"), stopped);
}

}

namespace widemelon
{

WebSocketClient::~WebSocketClient()
{
    requestStop();
}

void WebSocketClient::requestStop()
{
    stopRequested.store(true);
    std::lock_guard<std::mutex> lock(socketMutex);
    if (activeSocket >= 0) shutdown(activeSocket, SHUT_RDWR);
}

ConnectionState WebSocketClient::connectionState() const
{
    return state.load();
}

bool WebSocketClient::registerSocket(int fileDescriptor)
{
    std::lock_guard<std::mutex> lock(socketMutex);
    if (stopRequested.load()) return false;
    activeSocket = fileDescriptor;
    return true;
}

void WebSocketClient::closeSocket(int fileDescriptor)
{
    {
        std::lock_guard<std::mutex> lock(socketMutex);
        if (activeSocket == fileDescriptor) activeSocket = -1;
    }
    close(fileDescriptor);
}

std::string WebSocketClient::connectAndAuthenticate(const Config& config, std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    const std::string port = std::to_string(config.port);
    int retryDelayMs = 250;
    while (!stopRequested.load() && std::chrono::steady_clock::now() < deadline)
    {
        Logger::info("Attempting TCP connection to " + config.host + ':' + port);
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        if (getaddrinfo(config.host.c_str(), port.c_str(), &hints, &result) != 0)
        {
            Logger::error("Cannot resolve configured host");
            state.store(ConnectionState::Failed);
            return "Invalid host";
        }
        const int fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        freeaddrinfo(result);
        if (fd < 0)
        {
            Logger::error("Cannot create TCP socket");
            continue;
        }
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        if (!registerSocket(fd))
        {
            close(fd);
            break;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(config.port);
        inet_pton(AF_INET, config.host.c_str(), &address.sin_addr);
        bool connected = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
        if (!connected && errno == EINPROGRESS)
        {
            const short events = pollSocket(fd, POLLOUT, 1000);
            if (events & POLLOUT)
            {
                int error = 0;
                socklen_t length = sizeof(error);
                connected = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0;
            }
        }

        bool authenticated = false;
        if (connected && !stopRequested.load())
        {
            Logger::info("TCP connection established; sending WebSocket upgrade request");
            const std::string host = config.host + ':' + port;
            const std::string request = "GET /bridge HTTP/1.1\r\nHost: " + host + "\r\nOrigin: http://" + host
                + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
            std::string response;
            if (sendAll(fd, request, stopRequested) && readHttpResponse(fd, response, stopRequested)
                && response.rfind("HTTP/1.1 101", 0) == 0
                && response.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos)
            {
                Logger::info("WebSocket upgrade accepted; sending auth message");
                const std::string auth = "{\"v\":2,\"type\":\"auth\",\"credential\":\"" + config.pairingCode + "\"}";
                if (sendAll(fd, clientFrame(0x1, auth), stopRequested))
                {
                    std::string inbound;
                    const auto authDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
                    while (!stopRequested.load() && std::chrono::steady_clock::now() < authDeadline && !authenticated)
                    {
                        const short events = pollSocket(fd, POLLIN, 100);
                        if (events & (POLLERR | POLLHUP | POLLNVAL)) break;
                        if (events & POLLIN)
                        {
                            std::array<char, 4096> buffer{};
                            const ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);
                            if (count <= 0) break;
                            inbound.append(buffer.data(), static_cast<std::size_t>(count));
                        }
                        Frame frame;
                        while (takeFrame(inbound, frame) == FrameResult::Ready)
                        {
                            if (!frame.final)
                            {
                                Logger::error("Fragmented WebSocket frames are not supported");
                                break;
                            }
                            if (frame.opcode == 0x9)
                            {
                                Logger::info("Received WebSocket ping; sending pong");
                                if (!sendAll(fd, clientFrame(0xA, frame.payload), stopRequested)) break;
                            }
                            if (frame.opcode == 0x1 && frame.payload.find("\"type\":\"ping\"") != std::string::npos)
                            {
                                Logger::info("Received WideMelon ping message; sending pong");
                                if (!replyToApplicationPing(fd, frame.payload, stopRequested)) break;
                            }
                            if (frame.opcode == 0x1 && frame.payload.find("\"type\":\"hello\"") != std::string::npos)
                            {
                                Logger::info("Received WebSocket message: hello; authentication succeeded");
                                authenticated = true;
                                break;
                            }
                        }
                    }
                }
                else Logger::error("Failed to send WebSocket auth message");
            }
            else Logger::error("WebSocket upgrade was rejected or timed out");
        }
        else if (!stopRequested.load()) Logger::error("TCP connection attempt failed");

        if (authenticated)
        {
            state.store(ConnectionState::Connected);
            Logger::info("WebSocket connection is active");
            std::string inbound;
            std::string disconnectReason = "Server closed the WebSocket connection";
            while (!stopRequested.load())
            {
                const short events = pollSocket(fd, POLLIN, 100);
                if (events & (POLLERR | POLLHUP | POLLNVAL)) break;
                if (!(events & POLLIN)) continue;
                std::array<char, 4096> buffer{};
                const ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);
                if (count <= 0) break;
                inbound.append(buffer.data(), static_cast<std::size_t>(count));
                Frame frame;
                FrameResult result = FrameResult::Incomplete;
                bool processingFailed = false;
                while ((result = takeFrame(inbound, frame)) == FrameResult::Ready)
                {
                    if (!frame.final)
                    {
                        disconnectReason = "Fragmented WebSocket frames are not supported";
                        Logger::error(disconnectReason);
                        break;
                    }
                    if (frame.opcode == 0x8)
                    {
                        Logger::info("Received WebSocket close message");
                        sendAll(fd, clientFrame(0x8, frame.payload), stopRequested);
                        break;
                    }
                    if (frame.opcode == 0x9)
                    {
                        Logger::info("Received WebSocket ping; sending pong");
                        if (!sendAll(fd, clientFrame(0xA, frame.payload), stopRequested))
                        {
                            disconnectReason = "Could not send WebSocket pong";
                            processingFailed = true;
                            break;
                        }
                    }
                    else if (frame.opcode == 0x1 && frame.payload.find("\"type\":\"ping\"") != std::string::npos)
                    {
                        Logger::info("Received WideMelon ping message; sending pong");
                        if (!replyToApplicationPing(fd, frame.payload, stopRequested))
                        {
                            disconnectReason = "Could not send WideMelon pong";
                            processingFailed = true;
                            break;
                        }
                    }
                    else if (frame.opcode == 0x2)
                    {
                        if (!acknowledgeFrame(fd, frame.payload, stopRequested))
                        {
                            disconnectReason = "Could not acknowledge video frame";
                            processingFailed = true;
                            break;
                        }
                    }
                    else if (frame.opcode == 0x1)
                        Logger::info("Received WebSocket text message");
                }
                if (result == FrameResult::Invalid)
                {
                    disconnectReason = "Invalid WebSocket frame received";
                    Logger::error(disconnectReason);
                    break;
                }
                if (processingFailed || frame.opcode == 0x8 || !frame.final) break;
            }
            closeSocket(fd);
            if (stopRequested.load())
            {
                state.store(ConnectionState::Stopped);
                Logger::info("Network connection stopped at application exit");
                return "Cancelled";
            }
            state.store(ConnectionState::Failed);
            Logger::error("WebSocket disconnected: " + disconnectReason);
            return disconnectReason;
        }

        closeSocket(fd);
        if (stopRequested.load()) break;
        Logger::info("Connection attempt failed; retrying in " + std::to_string(retryDelayMs) + " ms");
        usleep(static_cast<useconds_t>(retryDelayMs) * 1000);
        retryDelayMs = std::min(retryDelayMs * 2, 4000);
    }
    if (stopRequested.load())
    {
        state.store(ConnectionState::Stopped);
        Logger::info("Network connection stopped at application exit");
        return "Cancelled";
    }
    state.store(ConnectionState::Failed);
    Logger::error("Connection timed out after " + std::to_string(timeout.count()) + " seconds");
    return "Timed out after " + std::to_string(timeout.count()) + " seconds";
}

}
