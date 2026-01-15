#ifndef VISIONPIPE_IP_STREAM_ITEMS_H
#define VISIONPIPE_IP_STREAM_ITEMS_H

#include "interpreter/item_registry.h"

#ifdef VISIONPIPE_IP_STREAM_ENABLED
#include "utils/http_frame_server.h"
#endif

namespace visionpipe {

/**
 * @brief Register all IP streaming related items
 * 
 * This function is only active when VISIONPIPE_IP_STREAM is enabled.
 * If the feature is disabled, this function does nothing.
 */
void registerIPStreamItems(ItemRegistry& registry);

#ifdef VISIONPIPE_IP_STREAM_ENABLED

// ============================================================================
// IP Stream Items
// ============================================================================

/**
 * @brief Stream frames over HTTP (MJPEG) to remote clients
 * 
 * This item starts an HTTP server that streams frames to connected clients.
 * The stream can be viewed in any web browser, VLC, or other MJPEG-compatible viewer.
 * 
 * By default, the server binds to all interfaces (0.0.0.0) allowing LAN access.
 * 
 * Parameters:
 * - port: TCP port to stream on (default: 8080)
 * - bind_address: IP address to bind to (default: "0.0.0.0" for all interfaces)
 * - quality: JPEG quality 1-100 (default: 80)
 * 
 * Example:
 *   ip_stream(8080)                           # Stream on port 8080
 *   ip_stream(9000, "0.0.0.0", 90)             # Port 9000, LAN access, 90% quality
 *   ip_stream(8080, "192.168.1.100")           # Bind to specific IP
 * 
 * Access the stream at: http://<your-ip>:<port>/stream
 */
class IPStreamItem : public InterpreterItem {
public:
    IPStreamItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Configure the JPEG quality for an IP stream
 * 
 * Parameters:
 * - port: Port of the stream to configure
 * - quality: JPEG quality 1-100
 * 
 * Example:
 *   ip_stream_quality(8080, 90)   # Set quality to 90% for stream on port 8080
 */
class IPStreamQualityItem : public InterpreterItem {
public:
    IPStreamQualityItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Stop an IP stream server
 * 
 * Parameters:
 * - port: Port of the stream to stop
 * 
 * Example:
 *   ip_stream_stop(8080)   # Stop the stream on port 8080
 */
class IPStreamStopItem : public InterpreterItem {
public:
    IPStreamStopItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Get information about active IP streams
 * 
 * Returns the number of active streams and connected clients.
 * 
 * Example:
 *   ip_stream_info()   # Print info about all active streams
 */
class IPStreamInfoItem : public InterpreterItem {
public:
    IPStreamInfoItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Get the URL for an IP stream
 * 
 * Parameters:
 * - port: Port of the stream (default: 8080)
 * 
 * Returns: String URL of the stream
 * 
 * Example:
 *   $url = ip_stream_url(8080)   # Get URL for stream on port 8080
 */
class IPStreamUrlItem : public InterpreterItem {
public:
    IPStreamUrlItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

#endif // VISIONPIPE_IP_STREAM_ENABLED

} // namespace visionpipe

#endif // VISIONPIPE_IP_STREAM_ITEMS_H
