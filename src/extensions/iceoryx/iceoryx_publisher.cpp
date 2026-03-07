/**
 * @file iceoryx_publisher.cpp
 * @brief Concrete Iceoryx2 channel session (PIMPL impl) + channel registry.
 *
 * Targets iceoryx2-cxx v0.8.x  (linked via CMake target iceoryx2-cxx::static-lib-cxx).
 *
 * Key v0.8.x API notes used here:
 *   - loan_slice_uninit(n)  → bb::Expected<SampleMutUninit<…>, LoanError>
 *   - result.value() / std::move(result).value()  (not operator*)
 *   - iox2::assume_init(std::move(uninit))  → SampleMut   (free fn, ADL)
 *   - iox2::send(std::move(sample))         → bb::Expected<size_t, SendError>
 *   - slice.number_of_elements()  not .size()
 *   - payload_mut() / payload() returns bb::MutableSlice / bb::ImmutableSlice
 *     (store BY VALUE – they are cheap pointer+length wrappers).
 */

#ifdef VISIONPIPE_ICEORYX2_ENABLED

#include "extensions/iceoryx/iceoryx_publisher.h"

// iceoryx2-cxx v0.8 headers  (include path: include/iceoryx2/v0.8.999)
#include "iox2/node.hpp"
#include "iox2/node_name.hpp"
#include "iox2/service_name.hpp"
#include "iox2/publisher.hpp"    // pulls in sample_mut.hpp + sample_mut_uninit.hpp
#include "iox2/subscriber.hpp"   // pulls in sample.hpp

#include <opencv2/core/mat.hpp>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <optional>

namespace visionpipe {
namespace iox2_transport {

// ============================================================================
// Convenience aliases
// ============================================================================
using IpcNode       = iox2::Node<iox2::ServiceType::Ipc>;
using IpcPublisher  = iox2::Publisher<iox2::ServiceType::Ipc,
                                      iox2::bb::Slice<uint8_t>, void>;
using IpcSubscriber = iox2::Subscriber<iox2::ServiceType::Ipc,
                                       iox2::bb::Slice<uint8_t>, void>;

// ============================================================================
// Process-wide Iceoryx2 node (singleton, lazily created)
// ============================================================================

static std::mutex             g_nodeMutex;
static std::optional<IpcNode> g_node;

static IpcNode& getNode() {
    std::lock_guard<std::mutex> lk(g_nodeMutex);
    if (!g_node.has_value()) {
        auto nameResult = iox2::NodeName::create("visionpipe");
        if (!nameResult.has_value()) {
            std::cerr << "[iox2] NodeName::create failed\n";
            std::terminate();
        }
        auto nodeResult = iox2::NodeBuilder()
                              .name(std::move(nameResult).value())
                              .create<iox2::ServiceType::Ipc>();
        if (!nodeResult.has_value()) {
            std::cerr << "[iox2] NodeBuilder::create failed\n";
            std::terminate();
        }
        g_node = std::move(nodeResult).value();
    }
    return *g_node;
}

// ============================================================================
// Concrete channel session implementation
// ============================================================================

struct Iox2ChannelSessionImpl final : public Iox2ChannelSession {
    std::optional<IpcPublisher>  pub;
    std::optional<IpcSubscriber> sub;

    // ── publish ───────────────────────────────────────────────────────────
    bool publish(const void* data, size_t dataBytes,
                 const IceoryxFrameMeta& meta) override {
        if (!pub.has_value()) return false;

        // Number of bytes to loan (Payload = Slice<uint8_t> → 1 element = 1 byte)
        const uint64_t sliceBytes =
            static_cast<uint64_t>(sizeof(IceoryxFrameMeta) + dataBytes);

        auto loanResult = pub->loan_slice_uninit(sliceBytes);
        if (!loanResult.has_value()) {
            std::cerr << "[iox2] loan_slice_uninit(" << sliceBytes
                      << ") failed for '" << name << "'\n";
            return false;
        }

        // SampleMutUninit — fill via payload_mut() (returns MutableSlice by value)
        auto sample_uninit = std::move(loanResult).value();
        auto slice = sample_uninit.payload_mut();           // MutableSlice<uint8_t>

        std::memcpy(slice.data(), &meta, sizeof(IceoryxFrameMeta));
        if (data && dataBytes > 0) {
            std::memcpy(slice.data() + sizeof(IceoryxFrameMeta), data, dataBytes);
        }

        // Convert uninit → init, then send (both are ADL free functions in iox2::)
        auto sample_init = iox2::assume_init(std::move(sample_uninit));
        auto sendResult  = iox2::send(std::move(sample_init));
        if (!sendResult.has_value()) {
            std::cerr << "[iox2] send() failed for '" << name << "'\n";
            return false;
        }
        return true;
    }

    // ── receive ───────────────────────────────────────────────────────────
    bool receive(cv::Mat& out) override {
        if (!sub.has_value()) return false;

        bool     got    = false;
        uint64_t newSeq = 0;

        while (true) {
            // receive() → bb::Expected<bb::Optional<Sample<…>>, ReceiveError>
            auto recvResult = sub->receive();
            if (!recvResult.has_value()) break;          // error — stop draining

            auto& sampleOpt = recvResult.value();        // bb::Optional<Sample>&
            if (!sampleOpt.has_value()) break;           // queue empty

            // payload() → ImmutableSlice<uint8_t>  (Slice<const uint8_t>)
            auto slice = sampleOpt->payload();           // MutableSlice by value

            if (slice.number_of_elements() < sizeof(IceoryxFrameMeta)) continue;

            IceoryxFrameMeta meta{};
            std::memcpy(&meta, slice.data(), sizeof(IceoryxFrameMeta));

            // Shutdown sentinel
            if (meta.seqNum == std::numeric_limits<uint64_t>::max()) {
                shutdown.store(true, std::memory_order_release);
                return false;
            }
            if (meta.dataSize == 0 ||
                slice.number_of_elements() <
                    sizeof(IceoryxFrameMeta) + meta.dataSize) {
                continue;
            }

            out.create(meta.height, meta.width, meta.type);
            if (!out.isContinuous()) out = out.clone();
            std::memcpy(out.data,
                        slice.data() + sizeof(IceoryxFrameMeta),
                        static_cast<size_t>(meta.dataSize));

            cachedWidth    = meta.width;
            cachedHeight   = meta.height;
            cachedType     = meta.type;
            cachedChannels = meta.channels;
            newSeq         = meta.seqNum;
            got = true;
        }

        if (got) {
            // Publish new frame into the session-level cache so other
            // threads that drain an empty queue still get a valid mat.
            lastReadSeq.store(newSeq, std::memory_order_release);
            std::lock_guard<std::mutex> lk(lastReadMtx);
            lastReadMat = out.clone();  // own the data (out may be stack-local)
        } else {
            // Queue was empty; return the last received frame so that
            // secondary callers in the same process turn are not starved.
            uint64_t seq = lastReadSeq.load(std::memory_order_acquire);
            if (seq > 0) {
                std::lock_guard<std::mutex> lk(lastReadMtx);
                out = lastReadMat;  // shallow copy — safe; data owned by lastReadMat
                return true;        // report as "frame available" (not "new frame")
            }
        }
        return got;
    }

    // ── sendShutdown ──────────────────────────────────────────────────────
    void sendShutdown() override {
        IceoryxFrameMeta sentinel{};
        sentinel.seqNum   = std::numeric_limits<uint64_t>::max();
        sentinel.dataSize = 0;
        publish(nullptr, 0, sentinel);
        shutdown.store(true, std::memory_order_release);
    }
};

// ============================================================================
// Channel registry
// ============================================================================

static std::mutex g_channelMutex;
static std::unordered_map<std::string, std::unique_ptr<Iox2ChannelSession>>
    g_channels;

static std::string svcName(const std::string& name) {
    return "/vp_" + name;
}

Iox2ChannelSession* iox2ChannelGet(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_channelMutex);
    auto it = g_channels.find(name);
    return (it != g_channels.end()) ? it->second.get() : nullptr;
}

Iox2ChannelSession* iox2ChannelCreate(const std::string& name,
                                      int width, int height, int type) {
    std::lock_guard<std::mutex> lk(g_channelMutex);

    auto it = g_channels.find(name);
    if (it != g_channels.end()) return it->second.get();

    auto impl = std::make_unique<Iox2ChannelSessionImpl>();
    impl->name         = name;
    impl->cachedWidth  = width;
    impl->cachedHeight = height;
    impl->cachedType   = type;

    // Derive channel count from cv mat type
    int numChannels          = (type >> CV_CN_SHIFT) + 1;
    impl->cachedChannels     = numChannels;

    // Max slice capacity = meta header + pixel data (at least IOX2_MAX_FRAME_BYTES)
    size_t pixBytes = (width > 0 && height > 0)
        ? static_cast<size_t>(width) * height * numChannels
              * std::max(1, CV_ELEM_SIZE1(type))
        : IOX2_MAX_FRAME_BYTES;
    impl->maxSliceLen =
        sizeof(IceoryxFrameMeta) + std::max(pixBytes, IOX2_MAX_FRAME_BYTES);

    // Build service name
    auto snameResult = iox2::ServiceName::create(svcName(name).c_str());
    if (!snameResult.has_value()) {
        std::cerr << "[iox2] ServiceName::create failed for channel '"
                  << name << "'\n";
        return nullptr;
    }
    auto sname = std::move(snameResult).value();

    // Open (or create) the publish/subscribe service
    auto serviceResult = getNode()
        .service_builder(sname)
        .template publish_subscribe<iox2::bb::Slice<uint8_t>>()
        .open_or_create();

    if (!serviceResult.has_value()) {
        std::cerr << "[iox2] service_builder::open_or_create() failed for '"
                  << name << "'\n";
        return nullptr;
    }
    auto service = std::move(serviceResult).value();

    // Publisher
    auto pubResult = service.publisher_builder()
                         .initial_max_slice_len(
                             static_cast<uint64_t>(impl->maxSliceLen))
                         .create();
    if (pubResult.has_value()) {
        impl->pub = std::move(pubResult).value();
    } else {
        std::cerr << "[iox2] Warning: failed to create publisher for '"
                  << name << "'\n";
    }

    // Subscriber
    auto subResult = service.subscriber_builder().create();
    if (subResult.has_value()) {
        impl->sub = std::move(subResult).value();
    } else {
        std::cerr << "[iox2] Warning: failed to create subscriber for '"
                  << name << "'\n";
    }

    auto* ptr = impl.get();
    g_channels.emplace(name, std::move(impl));
    return ptr;
}

void iox2ChannelRemove(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_channelMutex);
    g_channels.erase(name);
}

void iox2ChannelRemoveAll() {
    std::lock_guard<std::mutex> lk(g_channelMutex);
    g_channels.clear();
}

void iox2NodeReset() {
    // Clear channels first (while holding both locks prevents any concurrent
    // iox2FrameWrite from racing with the node teardown).
    {
        std::lock_guard<std::mutex> lk(g_channelMutex);
        g_channels.clear();
    }
    // Destroy the process-wide IpcNode so the next call to getNode() builds
    // a fresh node with a new PID-qualified name and a clean service registry.
    {
        std::lock_guard<std::mutex> lk(g_nodeMutex);
        g_node.reset();  // optional<IpcNode>::reset() → closes the iox2 node
    }
}

} // namespace iox2_transport
} // namespace visionpipe

#endif // VISIONPIPE_ICEORYX2_ENABLED
