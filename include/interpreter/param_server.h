#ifndef VISIONPIPE_PARAM_SERVER_H
#define VISIONPIPE_PARAM_SERVER_H

/**
 * @file param_server.h
 * @brief TCP parameter server for VisionPipe runtime control
 *
 * Starts a lightweight TCP server on an auto-selected port that accepts
 * text-line commands to read/write/watch runtime parameters.
 *
 * Line-based wire protocol (UTF-8, newline-terminated):
 *
 *   Client -> Server:
 *     SET <name> <value>\n           Set a parameter value
 *     GET <name>\n                   Get a parameter value
 *     LIST\n                         List all parameters
 *     WATCH <name>\n                 Subscribe to change events for a param
 *     WATCH *\n                      Subscribe to all param changes
 *     UNWATCH <name>\n               Unsubscribe
 *     UNWATCH *\n                    Unsubscribe all
 *     QUIT\n                         Disconnect
 *
 *   Server -> Client (JSON + newline):
 *     {"ok":true}\n
 *     {"ok":true,"name":"brightness","value":75}\n
 *     {"ok":true,"params":{"brightness":75,"gain":1.0}}\n
 *     {"ok":false,"error":"Parameter not found: foo"}\n
 *
 *   Server -> Client (pushed events for WATCH subscribers):
 *     {"event":"changed","name":"brightness","old":50,"new":75}\n
 *
 * Port discovery:
 *   The server writes its port to  /tmp/visionpipe-params-<pid>.port
 *   on startup so that libvisionpipe Session can locate it automatically.
 */

#include "interpreter/param_store.h"

#include <atomic>
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include <mutex>
#include <functional>

namespace visionpipe {

/**
 * @brief TCP server exposing ParameterStore over a simple text protocol
 */
class ParamServer {
public:
    explicit ParamServer(std::shared_ptr<ParameterStore> store);
    ~ParamServer();

    // Non-copyable
    ParamServer(const ParamServer&) = delete;
    ParamServer& operator=(const ParamServer&) = delete;

    /**
     * @brief Start the server.
     *
     * Binds to the first available port in [startPort, startPort+200).
     * Spawns an accept-loop thread. Returns immediately.
     *
     * @param startPort First port to try (default: 8765)
     * @return Bound port, or -1 on failure
     */
    int start(int startPort = 8765);

    /**
     * @brief Stop the server and wait for threads to finish
     */
    void stop();

    /** @return Bound port (0 if not started) */
    int port() const { return _port.load(); }

    /** @return Whether the server is running */
    bool isRunning() const { return _running.load(); }

    /**
     * @brief Path to the port-file written for subprocess discovery.
     *        Format: /tmp/visionpipe-params-<pid>.port
     */
    static std::string portFilePath();

    /**
     * @brief Read the port from the port file for a given PID.
     *        Retries up to timeoutMs milliseconds.
     * @return port on success, -1 on timeout/error
     */
    static int readPortFile(int pid, int timeoutMs = 5000);

    /**
     * @brief Push a change event to all WATCH subscribers.
     *        Called internally via ParameterStore subscription.
     */
    void pushChangeEvent(const ParamChangeEvent& evt);

private:
    std::shared_ptr<ParameterStore> _store;

    int               _serverFd   = -1;
    std::atomic<int>  _port{0};
    std::atomic<bool> _running{false};

    std::thread _acceptThread;

    // Connected clients
    struct Client {
        int         fd          = -1;
        bool        watchAll    = false;
        std::vector<std::string> watchList;  // param names being watched
        std::mutex  writeMutex;
    };

    std::vector<std::shared_ptr<Client>> _clients;
    std::mutex                           _clientsMutex;

    uint64_t _storeSubscriptionId = 0;

    void acceptLoop();
    void handleClient(std::shared_ptr<Client> client);
    std::string processCommand(const std::string& line, Client& client);
    void writeToClient(Client& client, const std::string& msg);
    void closeClient(std::shared_ptr<Client> client);

    void writePortFile(int p);
    void removePortFile();
};

} // namespace visionpipe

#endif // VISIONPIPE_PARAM_SERVER_H
