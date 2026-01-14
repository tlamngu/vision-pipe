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
    
    // Optional: Register DNN/DeepLearning items when feature is enabled
#ifdef VISIONPIPE_WITH_DNN
    registerDNNItems(registry);
#endif
}

} // namespace visionpipe
