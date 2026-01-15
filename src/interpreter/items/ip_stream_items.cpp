#include "interpreter/items/ip_stream_items.h"
#include <iostream>

namespace visionpipe {

#ifdef VISIONPIPE_IP_STREAM_ENABLED

void registerIPStreamItems(ItemRegistry& registry) {
    registry.add<IPStreamItem>();
    registry.add<IPStreamQualityItem>();
    registry.add<IPStreamStopItem>();
    registry.add<IPStreamInfoItem>();
    registry.add<IPStreamUrlItem>();
}

// ============================================================================
// IPStreamItem
// ============================================================================

IPStreamItem::IPStreamItem() {
    _functionName = "ip_stream";
    _description = "Stream frames over HTTP (MJPEG) to remote clients. "
                   "Access the stream from any web browser, VLC, or MJPEG viewer at http://<ip>:<port>/stream";
    _category = "ip_stream";
    _params = {
        ParamDef::optional("port", BaseType::INT, "TCP port to stream on", 8080),
        ParamDef::optional("bind_address", BaseType::STRING, 
                          "IP address to bind to. Use '0.0.0.0' for all interfaces (LAN access)", 
                          std::string("0.0.0.0")),
        ParamDef::optional("quality", BaseType::INT, "JPEG encoding quality (1-100)", 80)
    };
    _example = "ip_stream(8080) | ip_stream(9000, \"0.0.0.0\", 90)";
    _returnType = "mat";
    _tags = {"stream", "http", "mjpeg", "network", "ip", "camera", "remote"};
}

ExecutionResult IPStreamItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int port = args.size() > 0 ? static_cast<int>(args[0].asInt()) : 8080;
    std::string bindAddress = args.size() > 1 ? args[1].asString() : "0.0.0.0";
    int quality = args.size() > 2 ? static_cast<int>(args[2].asInt()) : 80;
    
    // Validate port
    if (port < 1 || port > 65535) {
        return ExecutionResult::fail("Invalid port number: " + std::to_string(port) + ". Must be 1-65535.");
    }
    
    // Validate quality
    quality = std::max<int>(1, std::min<int>(100, quality));
    
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("No frame to stream. ip_stream requires an input frame.");
    }
    
    // Get or create server
    auto* server = IPStreamManager::instance().getServer(static_cast<uint16_t>(port), bindAddress);
    if (!server) {
        return ExecutionResult::fail("Failed to start IP stream server on port " + std::to_string(port));
    }
    
    // Set quality
    server->setJpegQuality(quality);
    
    // Push frame to server
    server->pushFrame(ctx.currentMat);
    
    // Pass through the frame
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// IPStreamQualityItem
// ============================================================================

IPStreamQualityItem::IPStreamQualityItem() {
    _functionName = "ip_stream_quality";
    _description = "Set the JPEG encoding quality for an IP stream";
    _category = "ip_stream";
    _params = {
        ParamDef::required("port", BaseType::INT, "Port of the stream to configure"),
        ParamDef::required("quality", BaseType::INT, "JPEG quality 1-100")
    };
    _example = "ip_stream_quality(8080, 90)";
    _returnType = "mat";
    _tags = {"stream", "quality", "config"};
}

ExecutionResult IPStreamQualityItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int port = static_cast<int>(args[0].asInt());
    int quality = static_cast<int>(args[1].asInt());
    
    quality = std::max<int>(1, std::min<int>(100, quality));
    
    auto* server = IPStreamManager::instance().getServer(static_cast<uint16_t>(port));
    if (!server) {
        return ExecutionResult::fail("No IP stream server running on port " + std::to_string(port));
    }
    
    server->setJpegQuality(quality);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// IPStreamStopItem
// ============================================================================

IPStreamStopItem::IPStreamStopItem() {
    _functionName = "ip_stream_stop";
    _description = "Stop an IP stream server";
    _category = "ip_stream";
    _params = {
        ParamDef::required("port", BaseType::INT, "Port of the stream to stop")
    };
    _example = "ip_stream_stop(8080)";
    _returnType = "void";
    _tags = {"stream", "stop", "close"};
}

ExecutionResult IPStreamStopItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int port = static_cast<int>(args[0].asInt());
    
    IPStreamManager::instance().stopServer(static_cast<uint16_t>(port));
    
    std::cout << "[IPStream] Stopped server on port " << port << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// IPStreamInfoItem
// ============================================================================

IPStreamInfoItem::IPStreamInfoItem() {
    _functionName = "ip_stream_info";
    _description = "Get information about active IP streams";
    _category = "ip_stream";
    _params = {};
    _example = "ip_stream_info()";
    _returnType = "int";  // Returns number of active streams
    _tags = {"stream", "info", "status"};
}

ExecutionResult IPStreamInfoItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    auto ports = IPStreamManager::instance().getActivePorts();
    
    std::cout << "[IPStream] Active streams: " << ports.size() << std::endl;
    
    for (auto port : ports) {
        auto* server = IPStreamManager::instance().getServer(port);
        if (server) {
            std::cout << "  - Port " << port << ": " 
                      << server->getConnectedClients() << " clients connected"
                      << " | URL: " << server->getStreamUrl() << std::endl;
        }
    }
    
    return ExecutionResult::okWithMat(ctx.currentMat, RuntimeValue(static_cast<int64_t>(ports.size())));
}

// ============================================================================
// IPStreamUrlItem
// ============================================================================

IPStreamUrlItem::IPStreamUrlItem() {
    _functionName = "ip_stream_url";
    _description = "Get the URL for an IP stream";
    _category = "ip_stream";
    _params = {
        ParamDef::optional("port", BaseType::INT, "Port of the stream", 8080)
    };
    _example = "$url = ip_stream_url(8080)";
    _returnType = "string";
    _tags = {"stream", "url", "address"};
}

ExecutionResult IPStreamUrlItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int port = args.size() > 0 ? static_cast<int>(args[0].asInt()) : 8080;
    
    auto* server = IPStreamManager::instance().getServer(static_cast<uint16_t>(port));
    if (!server) {
        return ExecutionResult::fail("No IP stream server running on port " + std::to_string(port));
    }
    
    std::string url = server->getStreamUrl();
    
    return ExecutionResult::okWithMat(ctx.currentMat, RuntimeValue(url));
}

#else // VISIONPIPE_IP_STREAM_ENABLED not defined

void registerIPStreamItems(ItemRegistry& registry) {
    // IP Stream feature is disabled - do nothing
    // Items are not registered when the feature is not enabled
    (void)registry;  // Suppress unused parameter warning
}

#endif // VISIONPIPE_IP_STREAM_ENABLED

} // namespace visionpipe
