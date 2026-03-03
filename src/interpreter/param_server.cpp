#include "interpreter/param_server.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstring>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socklen_t = int;
  using ssize_t   = SSIZE_T;
  #define CLOSE_SOCKET closesocket
  #define SOCK_INVALID INVALID_SOCKET
  using SocketFd  = SOCKET;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <cerrno>
  #define CLOSE_SOCKET ::close
  #define SOCK_INVALID (-1)
  using SocketFd  = int;
#endif

#include <unistd.h>  // getpid()

namespace visionpipe {

// ============================================================================
// Helpers
// ============================================================================

static std::string jsonOk() { return "{\"ok\":true}\n"; }
static std::string jsonError(const std::string& msg) {
    std::string s = "{\"ok\":false,\"error\":\"";
    for (char c : msg) {
        if (c == '"')  s += "\\\"";
        else if (c == '\\') s += "\\\\";
        else s += c;
    }
    s += "\"}\n";
    return s;
}

// ============================================================================
// ParamServer
// ============================================================================

ParamServer::ParamServer(std::shared_ptr<ParameterStore> store)
    : _store(std::move(store)) {}

ParamServer::~ParamServer() {
    stop();
}

int ParamServer::start(int startPort) {
    if (_running.load()) return _port.load();

#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    SocketFd serverFd = SOCK_INVALID;
    int      boundPort = -1;

    for (int p = startPort; p < startPort + 200; ++p) {
        serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd == SOCK_INVALID) continue;

        // Allow fast re-bind
        int opt = 1;
        ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // localhost only
        addr.sin_port        = htons(static_cast<uint16_t>(p));

        if (::bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            if (::listen(serverFd, 8) == 0) {
                boundPort = p;
                break;
            }
        }

        CLOSE_SOCKET(serverFd);
        serverFd = SOCK_INVALID;
    }

    if (boundPort < 0) {
        std::cerr << "[ParamServer] Failed to bind to any port in ["
                  << startPort << ", " << startPort + 200 << ")\n";
        return -1;
    }

    _serverFd = static_cast<int>(serverFd);
    _port.store(boundPort);
    _running.store(true);

    // Subscribe to store changes so we can push events to WATCH clients
    auto self = this;
    _storeSubscriptionId = _store->subscribeAll([self](const ParamChangeEvent& evt) {
        self->pushChangeEvent(evt);
    });

    // Write port file for subprocess discovery
    writePortFile(boundPort);

    // Start accept thread
    _acceptThread = std::thread([this]() { acceptLoop(); });

    std::cout << "[VisionPipe] Param server ready on port " << boundPort << std::endl;
    return boundPort;
}

void ParamServer::stop() {
    if (!_running.exchange(false)) return;

    _store->unsubscribe(_storeSubscriptionId);

    // Close server socket to unblock accept()
    if (_serverFd >= 0) {
        CLOSE_SOCKET(static_cast<SocketFd>(_serverFd));
        _serverFd = -1;
    }

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        for (auto& c : _clients) {
            if (c->fd >= 0) {
                CLOSE_SOCKET(static_cast<SocketFd>(c->fd));
                c->fd = -1;
            }
        }
        _clients.clear();
    }

    if (_acceptThread.joinable()) {
        _acceptThread.join();
    }

    removePortFile();
    _port.store(0);
}

void ParamServer::pushChangeEvent(const ParamChangeEvent& evt) {
    if (!_running.load()) return;

    // Build JSON event message
    std::ostringstream ss;
    ss << "{\"event\":\"changed\","
       << "\"name\":\"" << evt.name << "\","
       << "\"old\":"   << evt.oldValue.toWireString() << ","
       << "\"new\":"   << evt.newValue.toWireString()
       << "}\n";
    std::string msg = ss.str();

    std::lock_guard<std::mutex> lock(_clientsMutex);
    for (auto& client : _clients) {
        if (client->fd < 0) continue;
        bool shouldSend = client->watchAll;
        if (!shouldSend) {
            for (const auto& wn : client->watchList) {
                if (wn == evt.name) { shouldSend = true; break; }
            }
        }
        if (shouldSend) {
            writeToClient(*client, msg);
        }
    }
}

void ParamServer::acceptLoop() {
    while (_running.load()) {
        sockaddr_in clientAddr{};
        socklen_t   addrLen = sizeof(clientAddr);

        SocketFd clientFd = ::accept(static_cast<SocketFd>(_serverFd),
                                     reinterpret_cast<sockaddr*>(&clientAddr),
                                     &addrLen);
        if (clientFd == SOCK_INVALID) {
            if (_running.load()) {
                std::cerr << "[ParamServer] accept() error\n";
            }
            break;
        }

        auto client = std::make_shared<Client>();
        client->fd = static_cast<int>(clientFd);

        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients.push_back(client);
        }

        // Each client handled on its own thread
        std::thread([this, client]() {
            handleClient(client);
            closeClient(client);
        }).detach();
    }
}

void ParamServer::handleClient(std::shared_ptr<Client> client) {
    std::string lineBuf;
    char        buf[512];

    while (_running.load() && client->fd >= 0) {
        ssize_t n = ::recv(static_cast<SocketFd>(client->fd), buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;  // closed or error

        buf[n] = '\0';
        lineBuf += buf;

        // Process complete lines
        size_t pos;
        while ((pos = lineBuf.find('\n')) != std::string::npos) {
            std::string line = lineBuf.substr(0, pos);
            lineBuf.erase(0, pos + 1);

            // Strip \r
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            std::string response = processCommand(line, *client);
            if (!response.empty()) {
                writeToClient(*client, response);
            }

            // QUIT command
            if (line == "QUIT" || line == "quit") return;
        }
    }
}

std::string ParamServer::processCommand(const std::string& line, Client& client) {
    // Tokenize
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    // Uppercase command
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "SET") {
        std::string name, rest;
        iss >> name;
        std::getline(iss, rest);
        // Trim leading space from rest
        if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
        if (name.empty() || rest.empty()) {
            return jsonError("Usage: SET <name> <value>");
        }
        _store->setFromString(name, rest);
        return jsonOk();
    }

    if (cmd == "GET") {
        std::string name;
        iss >> name;
        if (name.empty()) return jsonError("Usage: GET <name>");

        if (!_store->has(name)) {
            return jsonError("Parameter not found: " + name);
        }
        return _store->paramToJson(name) + "\n";
    }

    if (cmd == "LIST") {
        return "{\"ok\":true,\"params\":" + _store->toJson() + "}\n";
    }

    if (cmd == "WATCH") {
        std::string name;
        iss >> name;
        if (name.empty()) return jsonError("Usage: WATCH <name|*>");

        std::lock_guard<std::mutex> lock(_clientsMutex);
        (void)lock;  // suppress unused warning
        if (name == "*") {
            client.watchAll = true;
        } else {
            if (std::find(client.watchList.begin(), client.watchList.end(), name)
                    == client.watchList.end()) {
                client.watchList.push_back(name);
            }
        }
        return jsonOk();
    }

    if (cmd == "UNWATCH") {
        std::string name;
        iss >> name;
        if (name == "*") {
            client.watchAll = false;
            client.watchList.clear();
        } else {
            client.watchList.erase(
                std::remove(client.watchList.begin(), client.watchList.end(), name),
                client.watchList.end());
        }
        return jsonOk();
    }

    if (cmd == "QUIT") {
        return jsonOk();
    }

    return jsonError("Unknown command: " + cmd);
}

void ParamServer::writeToClient(Client& client, const std::string& msg) {
    if (client.fd < 0) return;
    std::lock_guard<std::mutex> lock(client.writeMutex);
    const char* data = msg.c_str();
    size_t      left = msg.size();
    while (left > 0 && client.fd >= 0) {
        ssize_t sent = ::send(static_cast<SocketFd>(client.fd), data,
                              static_cast<int>(left), 0);
        if (sent <= 0) {
            client.fd = -1;
            break;
        }
        data += sent;
        left -= static_cast<size_t>(sent);
    }
}

void ParamServer::closeClient(std::shared_ptr<Client> client) {
    if (client->fd >= 0) {
        CLOSE_SOCKET(static_cast<SocketFd>(client->fd));
        client->fd = -1;
    }
    std::lock_guard<std::mutex> lock(_clientsMutex);
    _clients.erase(
        std::remove_if(_clients.begin(), _clients.end(),
                       [&client](const std::shared_ptr<Client>& c){ return c == client; }),
        _clients.end());
}

// ============================================================================
// Port file
// ============================================================================

std::string ParamServer::portFilePath() {
    pid_t pid = ::getpid();
    return "/tmp/visionpipe-params-" + std::to_string(pid) + ".port";
}

void ParamServer::writePortFile(int p) {
    std::ofstream f(portFilePath());
    if (f) f << p << "\n";
}

void ParamServer::removePortFile() {
    ::unlink(portFilePath().c_str());
}

int ParamServer::readPortFile(int pid, int timeoutMs) {
    std::string path = "/tmp/visionpipe-params-" + std::to_string(pid) + ".port";
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream f(path);
        if (f) {
            int port = -1;
            f >> port;
            if (port > 0) return port;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return -1;
}

} // namespace visionpipe
