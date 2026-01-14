#ifndef VISIONPIPE_ADVANCED_ITEMS_H
#define VISIONPIPE_ADVANCED_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <opencv2/video.hpp>
#include <opencv2/photo.hpp>
#include <opencv2/stitching.hpp>

namespace visionpipe {

void registerAdvancedItems(ItemRegistry& registry);

// ============================================================================
// Optical Flow
// ============================================================================

class CalcOpticalFlowPyrLKItem : public InterpreterItem {
public:
    CalcOpticalFlowPyrLKItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class CalcOpticalFlowFarnebackItem : public InterpreterItem {
public:
    CalcOpticalFlowFarnebackItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class DrawOpticalFlowItem : public InterpreterItem {
public:
    DrawOpticalFlowItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Background Subtraction
// ============================================================================

class BackgroundSubtractorMOG2Item : public InterpreterItem {
public:
    BackgroundSubtractorMOG2Item();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class BackgroundSubtractorKNNItem : public InterpreterItem {
public:
    BackgroundSubtractorKNNItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class ApplyBackgroundSubtractorItem : public InterpreterItem {
public:
    ApplyBackgroundSubtractorItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Object Tracking
// ============================================================================

class TrackerKCFItem : public InterpreterItem {
public:
    TrackerKCFItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class TrackerCSRTItem : public InterpreterItem {
public:
    TrackerCSRTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class TrackerMILItem : public InterpreterItem {
public:
    TrackerMILItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class InitTrackerItem : public InterpreterItem {
public:
    InitTrackerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class UpdateTrackerItem : public InterpreterItem {
public:
    UpdateTrackerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class MultiTrackerItem : public InterpreterItem {
public:
    MultiTrackerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Image Stitching
// ============================================================================

class StitcherItem : public InterpreterItem {
public:
    StitcherItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// HDR
// ============================================================================

class CreateMergeMertensItem : public InterpreterItem {
public:
    CreateMergeMertensItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class MergeExposuresItem : public InterpreterItem {
public:
    MergeExposuresItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class TonemapItem : public InterpreterItem {
public:
    TonemapItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class TonemapDragoItem : public InterpreterItem {
public:
    TonemapDragoItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class TonemapReinhardItem : public InterpreterItem {
public:
    TonemapReinhardItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class TonemapMantiukItem : public InterpreterItem {
public:
    TonemapMantiukItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Non-Photorealistic Rendering
// ============================================================================

class EdgePreservingFilterItem : public InterpreterItem {
public:
    EdgePreservingFilterItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class DetailEnhanceItem : public InterpreterItem {
public:
    DetailEnhanceItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class PencilSketchItem : public InterpreterItem {
public:
    PencilSketchItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class StylizationItem : public InterpreterItem {
public:
    StylizationItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Inpainting
// ============================================================================

class InpaintItem : public InterpreterItem {
public:
    InpaintItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Denoising
// ============================================================================

class FastNlMeansDenoisingItem : public InterpreterItem {
public:
    FastNlMeansDenoisingItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class FastNlMeansDenoisingColoredItem : public InterpreterItem {
public:
    FastNlMeansDenoisingColoredItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Seamless Cloning
// ============================================================================

class SeamlessCloneItem : public InterpreterItem {
public:
    SeamlessCloneItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class ColorChangeItem : public InterpreterItem {
public:
    ColorChangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class IlluminationChangeItem : public InterpreterItem {
public:
    IlluminationChangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class TextureFlattingItem : public InterpreterItem {
public:
    TextureFlattingItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_ADVANCED_ITEMS_H
