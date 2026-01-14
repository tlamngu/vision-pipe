#ifndef VISIONPIPE_ALL_ITEMS_H
#define VISIONPIPE_ALL_ITEMS_H

/**
 * @file all_items.h
 * @brief Master header that includes all VisionPipe item categories
 * 
 * This file provides a single include point for all VisionPipe interpreter items.
 * Items are organized by functional category for better maintainability.
 */

// Core item registry and base classes
#include "interpreter/item_registry.h"

// Item categories
#include "interpreter/items/video_io_items.h"
#include "interpreter/items/filter_items.h"
#include "interpreter/items/color_items.h"
#include "interpreter/items/transform_items.h"
#include "interpreter/items/morphology_items.h"
#include "interpreter/items/edge_items.h"
#include "interpreter/items/feature_items.h"
#include "interpreter/items/stereo_items.h"
#include "interpreter/items/draw_items.h"
#include "interpreter/items/display_items.h"
#include "interpreter/items/control_items.h"
#include "interpreter/items/arithmetic_items.h"
#include "interpreter/items/advanced_items.h"
#include "interpreter/items/math_eval_items.h"
#include "interpreter/items/conditional_items.h"
#include "interpreter/items/gui_enhanced_items.h"
#include "interpreter/items/tensor_items.h"

// Optional: DeepLearning/DNN items (when VISIONPIPE_WITH_DNN is enabled)
#ifdef VISIONPIPE_WITH_DNN
#include "interpreter/items/dnn_items.h"
#endif

namespace visionpipe {

/**
 * @brief Registers all built-in items from all categories
 * 
 * This function calls the registration function for each item category.
 * It should be called once during interpreter initialization.
 * 
 * @param registry The item registry to populate
 */
void registerAllItems(ItemRegistry& registry);

} // namespace visionpipe

#endif // VISIONPIPE_ALL_ITEMS_H
