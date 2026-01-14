#include "interpreter/ml/ml_model.h"

namespace visionpipe {
namespace ml {

std::vector<std::string> getAvailableBackends() {
    std::vector<std::string> backends;
    
    // OpenCV DNN is always available when DeepLearning feature is enabled
    backends.push_back("opencv");
    
#ifdef VISIONPIPE_HAS_ONNX
    backends.push_back("onnx");
#endif

#ifdef VISIONPIPE_HAS_TENSORRT
    backends.push_back("tensorrt");
#endif

#ifdef VISIONPIPE_HAS_OPENVINO
    backends.push_back("openvino");
#endif
    
    return backends;
}

bool isBackendAvailable(const std::string& backend) {
    auto backends = getAvailableBackends();
    for (const auto& b : backends) {
        if (b == backend) return true;
    }
    return false;
}

} // namespace ml
} // namespace visionpipe
