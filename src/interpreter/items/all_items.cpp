#include "interpreter/items/all_items.h"

namespace visionpipe {

void registerAllItems(ItemRegistry& registry) {
    // Register items from each category
    registerVideoIOItems(registry);
    registerFilterItems(registry);
    registerColorItems(registry);
    registerTransformItems(registry);
    registerMorphologyItems(registry);
    registerEdgeItems(registry);
    registerFeatureItems(registry);
    registerStereoItems(registry);
    registerDrawItems(registry);
    registerDisplayItems(registry);
    registerControlItems(registry);
    registerArithmeticItems(registry);
    registerAdvancedItems(registry);
    registerMathEvalItems(registry);
    registerConditionalItems(registry);
    registerGuiEnhancedItems(registry);
    registerTensorItems(registry);
    
    // Auto Exposure
    registerAutoExposureItems(registry);

    // Adaptive Auto Brightness (PID multi-axis)
    registerAdaptiveBrightnessItems(registry);

    // GPU-accelerated debayering
    registerGpuDebayerItems(registry);

    // GPU-accelerated colour transform and matrix multiplication
    registerGpuTransformItems(registry);

    // Frame sink (shared memory IPC for libvisionpipe)
    registerFrameSinkItems(registry);

    // Discovery: discover_sinks, sink_properties, discover_capture, capture_capabilities
    registerDiscoveryItems(registry);

    // Horizon lock / video stabilization
    registerStabilizationItems(registry);

    // GPU-accelerated stabilization (OpenCL warp kernel)
    registerGpuStabilizationItems(registry);

    // Shared-memory IPC items for exec_fork multiprocessing
    registerShmItems(registry);

    // Optional: Register IP Stream items when feature is enabled
    // This function is a no-op when VISIONPIPE_IP_STREAM_ENABLED is not defined
    registerIPStreamItems(registry);
    
    // Optional: Register DNN/DeepLearning items when feature is enabled
#ifdef VISIONPIPE_WITH_DNN
    registerDNNItems(registry);
#endif

    // Optional: Register LibCamera items when feature is enabled
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
    registerLibCameraItems(registry);
#endif

    // Optional: Register V4L2 native items when feature is enabled
#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
    registerV4L2Items(registry);
#endif

    // Optional: Register FastCV accelerated items when feature is enabled
#ifdef VISIONPIPE_FASTCV_ENABLED
    registerFastCVItems(registry);
#endif

    // Optional: Register Iceoryx2 explicit publish/subscribe items
#ifdef VISIONPIPE_ICEORYX2_ENABLED
    registerIceoryxPublishItems(registry);
#endif
}

} // namespace visionpipe
