#include "network/WebSocketClient.h"

#include "common/Logger.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>

namespace
{
bool waitFor(int fd, short events, int timeoutMs)
{
    pollfd value{fd, events, 0};
    return poll(&value, 1, timeoutMs) > 0 && (value.revents & events);
}

bool sendAll(int fd, const std::string& text)
{
    std::size_t sent = 0;
    while (sent < text.size())
    {
        if (!waitFor(fd, POLLOUT, 1000)) return false;
        const ssize_t count = send(fd, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
        if (count <= 0) return false;
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

std::string textFrame(const std::string& text)
{
    const std::array<unsigned char, 4> mask{0x51, 0x27, 0xA4, 0xD8};
    std::string frame;
    frame.push_back(static_cast<char>(0x81));
    frame.push_back(static_cast<char>(0x80 | text.size()));
    for (auto value : mask) frame.push_back(static_cast<char>(value));
    for (std::size_t i = 0; i < text.size(); ++i)
        frame.push_back(static_cast<char>(static_cast<unsigned char>(text[i]) ^ mask[i % mask.size()]));
    return frame;
}
}

namespace widemelon
{

std::string WebSocketClient::connectAndAuthenticate(const Config& config, std::chrono::seconds timeout)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    const std::string port = std::to_string(config.port);
    while (std::chrono::steady_clock::now() < end)
    {
        Logger::info("Attempting TCP connection to " + config.host + ':' + port);
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        if (getaddrinfo(config.host.c_str(), port.c_str(), &hints, &result) != 0)
        {
            Logger::error("Cannot resolve configured host");
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
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(config.port);
        inet_pton(AF_INET, config.host.c_str(), &address.sin_addr);
        bool connected = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
        if (!connected && errno == EINPROGRESS && waitFor(fd, POLLOUT, 1000))
        {
            int error = 0;
            socklen_t length = sizeof(error);
            connected = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0;
        }
        if (connected)
        {
            Logger::info("TCP connection established; sending WebSocket upgrade request");
            const std::string host = config.host + ':' + port;
            const std::string request = "GET /bridge HTTP/1.1\r\nHost: " + host + "\r\nOrigin: http://" + host
                + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
            std::string reply;
            std::array<char, 1024> buffer{};
            if (sendAll(fd, request) && waitFor(fd, POLLIN, 1000))
            {
                const ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);
                if (count > 0) reply.assign(buffer.data(), static_cast<std::size_t>(count));
            }
            if (reply.rfind("HTTP/1.1 101", 0) == 0
                && reply.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos)
            {
                Logger::info("WebSocket upgrade accepted; sending auth message");
                const std::string auth = "{\"v\":2,\"type\":\"auth\",\"credential\":\"" + config.pairingCode + "\"}";
                if (sendAll(fd, textFrame(auth)) && waitFor(fd, POLLIN, 1000))
                {
                    const ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);
                    if (count > 0 && std::string(buffer.data(), static_cast<std::size_t>(count)).find("\"type\":\"hello\"") != std::string::npos)
                    {
                        Logger::info("Received WebSocket message: hello; authentication succeeded");
                        Logger::info("WebSocket disconnected by current one-shot client implementation");
                        close(fd);
                        return {};
                    }
                    Logger::error("WebSocket authentication reply did not contain hello");
                }
                else Logger::error("Failed to send auth message or receive authentication reply");
            }
            else Logger::error("WebSocket upgrade was rejected or timed out");
        }
        else Logger::error("TCP connection attempt failed");
        Logger::info("WebSocket disconnected; retrying connection");
        close(fd);
        usleep(250000);
    }
    Logger::error("Connection timed out after " + std::to_string(timeout.count()) + " seconds");
    return "Timed out after " + std::to_string(timeout.count()) + " seconds";
}

}
