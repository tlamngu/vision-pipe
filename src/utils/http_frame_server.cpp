#ifdef VISIONPIPE_IP_STREAM_ENABLED

#include "utils/http_frame_server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
#else
    #include <ifaddrs.h>
    #include <netdb.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

namespace visionpipe {

// ============================================================================
// FrameQueue Implementation
// ============================================================================

FrameQueue::FrameQueue(size_t maxSize) : _maxSize(maxSize) {}

void FrameQueue::push(std::vector<uint8_t>&& frame) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Drop oldest frame if queue is full
    while (_queue.size() >= _maxSize) {
        _queue.pop_front();
    }
    
    _queue.push_back(std::move(frame));
    _cv.notify_all();
}

bool FrameQueue::pop(std::vector<uint8_t>& frame, int timeoutMs) {
    std::unique_lock<std::mutex> lock(_mutex);
    
    if (_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                     [this] { return !_queue.empty(); })) {
        frame = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }
    
    return false;
}

void FrameQueue::clear() {
    std::lock_guard<std::mutex> lock(_mutex);
    _queue.clear();
}

size_t FrameQueue::size() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.size();
}

// ============================================================================
// FrameEncoder Implementation
// ============================================================================

FrameEncoder::FrameEncoder(int jpegQuality) : _quality(jpegQuality) {
    _params = {cv::IMWRITE_JPEG_QUALITY, _quality};
}

std::vector<uint8_t> FrameEncoder::encode(const cv::Mat& frame) {
    std::vector<uint8_t> buffer;
    
    if (frame.empty()) {
        return buffer;
    }
    
    try {
        cv::imencode(".jpg", frame, buffer, _params);
    } catch (const cv::Exception& e) {
        std::cerr << "[IPStream] JPEG encoding error: " << e.what() << std::endl;
        buffer.clear();
    }
    
    return buffer;
}

void FrameEncoder::setQuality(int quality) {
    _quality = std::clamp(quality, 1, 100);
    _params = {cv::IMWRITE_JPEG_QUALITY, _quality};
}

// ============================================================================
// HttpFrameServer Implementation
// ============================================================================

HttpFrameServer::HttpFrameServer(uint16_t port, const std::string& bindAddress)
    : _port(port), _bindAddress(bindAddress), _encoder(80) {
    _frameQueue = std::make_unique<FrameQueue>(_maxQueueSize);
}

HttpFrameServer::~HttpFrameServer() {
    stop();
}

bool HttpFrameServer::initializeWinsock() {
#ifdef _WIN32
    if (!_winsockInitialized) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "[IPStream] WSAStartup failed: " << result << std::endl;
            return false;
        }
        _winsockInitialized = true;
    }
#endif
    return true;
}

void HttpFrameServer::cleanupWinsock() {
#ifdef _WIN32
    if (_winsockInitialized) {
        WSACleanup();
        _winsockInitialized = false;
    }
#endif
}

bool HttpFrameServer::start() {
    if (_running.load()) {
        return true;  // Already running
    }
    
    if (!initializeWinsock()) {
        return false;
    }
    
    // Create socket
    _serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_serverSocket == INVALID_SOCKET_VALUE) {
        std::cerr << "[IPStream] Failed to create socket" << std::endl;
        return false;
    }
    
    // Allow address reuse
    int optval = 1;
#ifdef _WIN32
    setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&optval), sizeof(optval));
#else
    setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
#endif
    
    // Bind to address
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(_port);
    
    if (_bindAddress == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, _bindAddress.c_str(), &serverAddr.sin_addr);
    }
    
    if (bind(_serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddr), 
             sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[IPStream] Failed to bind to " << _bindAddress << ":" << _port << std::endl;
        closeSocket(_serverSocket);
        _serverSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    // Listen for connections
    if (listen(_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[IPStream] Failed to listen on socket" << std::endl;
        closeSocket(_serverSocket);
        _serverSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    _running.store(true);
    _acceptThread = std::thread(&HttpFrameServer::acceptLoop, this);
    
    std::cout << "[IPStream] Server started at " << getStreamUrl() << std::endl;
    
    return true;
}

void HttpFrameServer::stop() {
    if (!_running.load()) {
        return;
    }
    
    _running.store(false);
    
    // Close all client sockets
    {
        std::lock_guard<std::mutex> lock(_clientMutex);
        for (auto socket : _activeClients) {
            closeSocket(socket);
        }
        _activeClients.clear();
    }
    
    // Close server socket to unblock accept()
    if (_serverSocket != INVALID_SOCKET_VALUE) {
        closeSocket(_serverSocket);
        _serverSocket = INVALID_SOCKET_VALUE;
    }
    
    // Wait for accept thread
    if (_acceptThread.joinable()) {
        _acceptThread.join();
    }
    
    // Wait for client threads
    for (auto& thread : _clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    _clientThreads.clear();
    
    // Clear frame queue
    _frameQueue->clear();
    
    cleanupWinsock();
    
    std::cout << "[IPStream] Server stopped" << std::endl;
}

void HttpFrameServer::acceptLoop() {
    while (_running.load()) {
        struct sockaddr_in clientAddr;
#ifdef _WIN32
        int clientAddrLen = sizeof(clientAddr);
#else
        socklen_t clientAddrLen = sizeof(clientAddr);
#endif
        
        SocketType clientSocket = accept(_serverSocket, 
                                         reinterpret_cast<struct sockaddr*>(&clientAddr),
                                         &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET_VALUE) {
            if (_running.load()) {
                // Only log if we're still supposed to be running
#ifdef _WIN32
                int err = WSAGetLastError();
                if (err != WSAEINTR && err != WSAENOTSOCK) {
                    std::cerr << "[IPStream] Accept error: " << err << std::endl;
                }
#else
                if (errno != EINTR && errno != EBADF) {
                    std::cerr << "[IPStream] Accept error: " << errno << std::endl;
                }
#endif
            }
            continue;
        }
        
        // Check max clients
        if (_connectedClients.load() >= _maxClients) {
            const char* response = "HTTP/1.1 503 Service Unavailable\r\n"
                                   "Content-Type: text/plain\r\n\r\n"
                                   "Too many clients connected";
            send(clientSocket, response, static_cast<int>(strlen(response)), 0);
            closeSocket(clientSocket);
            continue;
        }
        
        // Get client IP for logging
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "[IPStream] Client connected from " << clientIP << std::endl;
        
        // Add to active clients
        {
            std::lock_guard<std::mutex> lock(_clientMutex);
            _activeClients.push_back(clientSocket);
        }
        
        // Start client handler thread
        _clientThreads.emplace_back(&HttpFrameServer::clientHandler, this, clientSocket);
    }
}

void HttpFrameServer::clientHandler(SocketType clientSocket) {
    _connectedClients++;
    
    // Read HTTP request (we just need to consume it)
    char buffer[4096];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) {
        goto cleanup;
    }
    
    buffer[bytesRead] = '\0';
    
    // Check if it's a request for the stream
    // We accept any GET request and serve the stream
    if (strncmp(buffer, "GET", 3) != 0) {
        const char* response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        send(clientSocket, response, static_cast<int>(strlen(response)), 0);
        goto cleanup;
    }
    
    // Send MJPEG headers
    if (!sendHttpHeaders(clientSocket)) {
        goto cleanup;
    }
    
    // Stream frames to client
    while (_running.load()) {
        std::vector<uint8_t> jpegData;
        
        if (_frameQueue->pop(jpegData, 500)) {
            if (!sendFrame(clientSocket, jpegData)) {
                break;  // Client disconnected
            }
        }
    }
    
cleanup:
    // Remove from active clients
    {
        std::lock_guard<std::mutex> lock(_clientMutex);
        auto it = std::find(_activeClients.begin(), _activeClients.end(), clientSocket);
        if (it != _activeClients.end()) {
            _activeClients.erase(it);
        }
    }
    
    closeSocket(clientSocket);
    _connectedClients--;
    
    std::cout << "[IPStream] Client disconnected" << std::endl;
}

bool HttpFrameServer::sendHttpHeaders(SocketType socket) {
    std::string headers = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    int sent = send(socket, headers.c_str(), static_cast<int>(headers.length()), 0);
    return sent == static_cast<int>(headers.length());
}

bool HttpFrameServer::sendFrame(SocketType socket, const std::vector<uint8_t>& jpegData) {
    if (jpegData.empty()) {
        return true;
    }
    
    // Build frame header
    std::ostringstream header;
    header << "--frame\r\n"
           << "Content-Type: image/jpeg\r\n"
           << "Content-Length: " << jpegData.size() << "\r\n"
           << "\r\n";
    
    std::string headerStr = header.str();
    
    // Send header
    int sent = send(socket, headerStr.c_str(), static_cast<int>(headerStr.length()), 0);
    if (sent != static_cast<int>(headerStr.length())) {
        return false;
    }
    
    // Send JPEG data
    size_t totalSent = 0;
    const size_t chunkSize = 65536;
    while (totalSent < jpegData.size()) {
        size_t remaining = jpegData.size() - totalSent;
        int toSend = static_cast<int>(std::min<size_t>(remaining, chunkSize));
        sent = send(socket, reinterpret_cast<const char*>(jpegData.data() + totalSent), toSend, 0);
        
        if (sent <= 0) {
            return false;
        }
        
        totalSent += sent;
    }
    
    // Send trailing CRLF
    const char* crlf = "\r\n";
    sent = send(socket, crlf, 2, 0);
    
    return sent == 2;
}

void HttpFrameServer::closeSocket(SocketType socket) {
    if (socket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        shutdown(socket, SD_BOTH);
        closesocket(socket);
#else
        shutdown(socket, SHUT_RDWR);
        close(socket);
#endif
    }
}

void HttpFrameServer::pushFrame(const cv::Mat& frame) {
    if (!_running.load() || frame.empty()) {
        return;
    }
    
    // Only encode and push if there are connected clients
    if (_connectedClients.load() > 0) {
        auto encoded = _encoder.encode(frame);
        if (!encoded.empty()) {
            _frameQueue->push(std::move(encoded));
        }
    }
}

void HttpFrameServer::setJpegQuality(int quality) {
    _encoder.setQuality(quality);
}

void HttpFrameServer::setMaxQueueSize(size_t size) {
    _maxQueueSize = size;
    // Note: This only affects new queue creation
}

int HttpFrameServer::getConnectedClients() const {
    return _connectedClients.load();
}

std::string HttpFrameServer::getStreamUrl() const {
    std::ostringstream url;
    
    if (_bindAddress == "0.0.0.0") {
        // Get local IP address for display
#ifdef _WIN32
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            struct addrinfo hints = {}, *result = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            
            if (getaddrinfo(hostname, nullptr, &hints, &result) == 0 && result) {
                char ip[INET_ADDRSTRLEN];
                struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
                inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
                url << "http://" << ip << ":" << _port << "/stream";
                freeaddrinfo(result);
                return url.str();
            }
        }
#else
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == 0) {
            for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                    // Skip loopback
                    if (strcmp(ifa->ifa_name, "lo") != 0) {
                        char ip[INET_ADDRSTRLEN];
                        struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
                        url << "http://" << ip << ":" << _port << "/stream";
                        freeifaddrs(ifaddr);
                        return url.str();
                    }
                }
            }
            freeifaddrs(ifaddr);
        }
#endif
        // Fallback to localhost
        url << "http://localhost:" << _port << "/stream";
    } else {
        url << "http://" << _bindAddress << ":" << _port << "/stream";
    }
    
    return url.str();
}

// ============================================================================
// IPStreamManager Implementation
// ============================================================================

IPStreamManager& IPStreamManager::instance() {
    static IPStreamManager instance;
    return instance;
}

IPStreamManager::~IPStreamManager() {
    stopAll();
}

HttpFrameServer* IPStreamManager::getServer(uint16_t port, const std::string& bindAddress) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _servers.find(port);
    if (it != _servers.end()) {
        return it->second.get();
    }
    
    // Create new server
    auto server = std::make_unique<HttpFrameServer>(port, bindAddress);
    if (!server->start()) {
        return nullptr;
    }
    std::cout << "getServer() returned, server=" << server << std::endl;
    
    HttpFrameServer* ptr = server.get();
    _servers[port] = std::move(server);
    return ptr;
}

void IPStreamManager::stopServer(uint16_t port) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _servers.find(port);
    if (it != _servers.end()) {
        it->second->stop();
        _servers.erase(it);
    }
}

void IPStreamManager::stopAll() {
    std::lock_guard<std::mutex> lock(_mutex);
    
    for (auto& [port, server] : _servers) {
        server->stop();
    }
    _servers.clear();
}

std::vector<uint16_t> IPStreamManager::getActivePorts() const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::vector<uint16_t> ports;
    ports.reserve(_servers.size());
    
    for (const auto& [port, server] : _servers) {
        if (server->isRunning()) {
            ports.push_back(port);
        }
    }
    
    return ports;
}

} // namespace visionpipe

#endif // VISIONPIPE_IP_STREAM_ENABLED
