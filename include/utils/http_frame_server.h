#ifndef VISIONPIPE_HTTP_FRAME_SERVER_H
#define VISIONPIPE_HTTP_FRAME_SERVER_H

#ifdef VISIONPIPE_IP_STREAM_ENABLED

#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <deque>
#include <functional>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketType = SOCKET;
    constexpr SocketType INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using SocketType = int;
    constexpr SocketType INVALID_SOCKET_VALUE = -1;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace visionpipe {

/**
 * @brief Thread-safe circular buffer for encoded frames
 */
class FrameQueue {
public:
    explicit FrameQueue(size_t maxSize = 30);
    
    void push(std::vector<uint8_t>&& frame);
    bool pop(std::vector<uint8_t>& frame, int timeoutMs = 100);
    void clear();
    size_t size() const;
    
private:
    std::deque<std::vector<uint8_t>> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    size_t _maxSize;
};

/**
 * @brief MJPEG Frame Encoder
 * 
 * Encodes cv::Mat frames to JPEG format for HTTP streaming.
 */
class FrameEncoder {
public:
    explicit FrameEncoder(int jpegQuality = 80);
    
    std::vector<uint8_t> encode(const cv::Mat& frame);
    void setQuality(int quality);
    int getQuality() const { return _quality; }
    
private:
    int _quality;
    std::vector<int> _params;
};

/**
 * @brief HTTP server for streaming MJPEG video over HTTP
 * 
 * This server allows remote devices to view video streams via standard HTTP.
 * Access the stream at: http://<ip>:<port>/stream
 * 
 * The server binds to all interfaces (0.0.0.0) by default for LAN access.
 * 
 * Example usage:
 *   HttpFrameServer server(8080);
 *   server.start();
 *   // In your frame loop:
 *   server.pushFrame(frame);
 *   // When done:
 *   server.stop();
 */
class HttpFrameServer {
public:
    /**
     * @brief Construct a new HTTP Frame Server
     * 
     * @param port TCP port to listen on (default: 8080)
     * @param bindAddress IP address to bind to (default: "0.0.0.0" for all interfaces)
     */
    explicit HttpFrameServer(uint16_t port = 8080, const std::string& bindAddress = "0.0.0.0");
    ~HttpFrameServer();
    
    // Non-copyable, non-movable
    HttpFrameServer(const HttpFrameServer&) = delete;
    HttpFrameServer& operator=(const HttpFrameServer&) = delete;
    HttpFrameServer(HttpFrameServer&&) = delete;
    HttpFrameServer& operator=(HttpFrameServer&&) = delete;
    
    /**
     * @brief Start the HTTP server
     * @return true if server started successfully
     */
    bool start();
    
    /**
     * @brief Stop the HTTP server and disconnect all clients
     */
    void stop();
    
    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return _running.load(); }
    
    /**
     * @brief Push a frame to be streamed to connected clients
     * 
     * The frame will be JPEG-encoded and queued for streaming.
     * If the queue is full, the oldest frame will be dropped.
     * 
     * @param frame The cv::Mat frame to stream (will be cloned internally)
     */
    void pushFrame(const cv::Mat& frame);
    
    // Configuration
    void setJpegQuality(int quality);
    int getJpegQuality() const { return _encoder.getQuality(); }
    
    void setMaxQueueSize(size_t size);
    void setMaxClients(int count) { _maxClients = count; }
    
    // Statistics
    int getConnectedClients() const;
    uint16_t getPort() const { return _port; }
    std::string getBindAddress() const { return _bindAddress; }
    std::string getStreamUrl() const;
    
private:
    void acceptLoop();
    void clientHandler(SocketType clientSocket);
    bool sendHttpHeaders(SocketType socket);
    bool sendFrame(SocketType socket, const std::vector<uint8_t>& jpegData);
    void closeSocket(SocketType socket);
    bool initializeWinsock();
    void cleanupWinsock();
    
    uint16_t _port;
    std::string _bindAddress;
    SocketType _serverSocket = INVALID_SOCKET_VALUE;
    
    std::atomic<bool> _running{false};
    std::thread _acceptThread;
    
    std::vector<std::thread> _clientThreads;
    std::atomic<int> _connectedClients{0};
    int _maxClients = 10;
    
    FrameEncoder _encoder;
    std::unique_ptr<FrameQueue> _frameQueue;
    size_t _maxQueueSize = 30;
    
    std::mutex _clientMutex;
    std::vector<SocketType> _activeClients;
    
#ifdef _WIN32
    bool _winsockInitialized = false;
#endif
};

/**
 * @brief Global server manager for managing multiple IP stream servers
 * 
 * This singleton manages server instances across the application,
 * allowing multiple streams on different ports.
 */
class IPStreamManager {
public:
    static IPStreamManager& instance();
    
    /**
     * @brief Get or create a server on the specified port
     * 
     * @param port TCP port
     * @param bindAddress IP address to bind to
     * @return Pointer to the server (managed by the manager)
     */
    HttpFrameServer* getServer(uint16_t port, const std::string& bindAddress = "0.0.0.0");
    
    /**
     * @brief Stop and remove a server on the specified port
     */
    void stopServer(uint16_t port);
    
    /**
     * @brief Stop all servers
     */
    void stopAll();
    
    /**
     * @brief Get all active server ports
     */
    std::vector<uint16_t> getActivePorts() const;
    
private:
    IPStreamManager() = default;
    ~IPStreamManager();
    
    mutable std::mutex _mutex;
    std::unordered_map<uint16_t, std::unique_ptr<HttpFrameServer>> _servers;
};

} // namespace visionpipe

#endif // VISIONPIPE_IP_STREAM_ENABLED

#endif // VISIONPIPE_HTTP_FRAME_SERVER_H
