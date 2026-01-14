#ifndef VISIONPIPE_DNN_ITEMS_H
#define VISIONPIPE_DNN_ITEMS_H

#include "interpreter/item_registry.h"
#include "interpreter/tensor_types.h"
#include "interpreter/ml/ml_model.h"
#include "interpreter/ml/model_registry.h"
#include "interpreter/ml/postprocessing.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

/**
 * @brief Register all DNN/ML items
 * Only called when VISIONPIPE_WITH_DNN is enabled
 */
void registerDNNItems(ItemRegistry& registry);

// ============================================================================
// Model Management Items
// ============================================================================

/**
 * @brief Load an ML model
 * 
 * Syntax:
 *   load_model("model_id", "path/to/model.onnx")
 *   load_model("model_id", "path/to/model.onnx", backend="opencv", device="cuda")
 */
class LoadModelItem : public InterpreterItem {
public:
    LoadModelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Unload an ML model
 * 
 * Syntax:
 *   unload_model("model_id")
 */
class UnloadModelItem : public InterpreterItem {
public:
    UnloadModelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief List loaded models
 * 
 * Syntax:
 *   list_models()
 */
class ListModelsItem : public InterpreterItem {
public:
    ListModelsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Get model information
 * 
 * Syntax:
 *   model_info("model_id")
 */
class ModelInfoItem : public InterpreterItem {
public:
    ModelInfoItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Inference Items
// ============================================================================

/**
 * @brief Run model inference on current frame
 * 
 * Syntax:
 *   model_infer("model_id")
 *   model_infer("model_id") -> "output_cache"
 */
class ModelInferItem : public InterpreterItem {
public:
    ModelInferItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Get specific output from last inference
 * 
 * Syntax:
 *   model_output("model_id", "layer_name")
 */
class ModelOutputItem : public InterpreterItem {
public:
    ModelOutputItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Preprocessing Items
// ============================================================================

/**
 * @brief Apply letterbox resize (maintain aspect ratio with padding)
 * 
 * Syntax:
 *   letterbox(640, 640)
 *   letterbox(640, 640, 114)  # custom padding color (gray)
 */
class LetterboxItem : public InterpreterItem {
public:
    LetterboxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Create DNN blob from image
 * 
 * Syntax:
 *   blob_from_image(1/255.0, [640, 640], [0, 0, 0], true)
 */
class BlobFromImageItem : public InterpreterItem {
public:
    BlobFromImageItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Normalize with mean and std
 * 
 * Syntax:
 *   normalize_mean_std([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
 */
class NormalizeMeanStdItem : public InterpreterItem {
public:
    NormalizeMeanStdItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Detection Post-processing Items
// ============================================================================

/**
 * @brief Decode YOLO output to detections
 * 
 * Syntax:
 *   decode_yolo("v8") -> "detections"
 *   decode_yolo("v8", 0.25) -> "detections"  # with confidence threshold
 */
class DecodeYoloItem : public InterpreterItem {
public:
    DecodeYoloItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Apply Non-Maximum Suppression
 * 
 * Syntax:
 *   nms_boxes(0.45, 0.25)  # iou_thresh, conf_thresh
 */
class NMSBoxesItem : public InterpreterItem {
public:
    NMSBoxesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Tensor Operation Items
// ============================================================================

/**
 * @brief Apply softmax activation
 * 
 * Syntax:
 *   softmax()
 *   softmax(1)  # axis
 */
class SoftmaxItem : public InterpreterItem {
public:
    SoftmaxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Apply sigmoid activation
 * 
 * Syntax:
 *   sigmoid()
 */
class SigmoidItem : public InterpreterItem {
public:
    SigmoidItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Get top-k predictions
 * 
 * Syntax:
 *   topk(5) -> "predictions"
 */
class TopKItem : public InterpreterItem {
public:
    TopKItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Drawing Items
// ============================================================================

/**
 * @brief Draw detection boxes on image
 * 
 * Syntax:
 *   draw_detections("detections_cache")
 *   draw_detections("detections_cache", labels="coco.names")
 */
class DrawDetectionsItem : public InterpreterItem {
public:
    DrawDetectionsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draw classification result on image
 * 
 * Syntax:
 *   draw_classification("predictions_cache")
 *   draw_classification("predictions_cache", labels="imagenet.txt")
 */
class DrawClassificationItem : public InterpreterItem {
public:
    DrawClassificationItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Tensor Output Conversion Items
// ============================================================================

/**
 * @brief Get raw DNN output as Vector
 * 
 * Syntax:
 *   model_output_vector("model_id", "layer_name") -> vector
 *   model_output_vector("model_id", 0) -> vector (by index)
 */
class ModelOutputVectorItem : public InterpreterItem {
public:
    ModelOutputVectorItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Get raw DNN output as Matrix
 * 
 * Syntax:
 *   model_output_matrix("model_id", "layer_name") -> matrix
 *   model_output_matrix("model_id", 0) -> matrix (by index)
 */
class ModelOutputMatrixItem : public InterpreterItem {
public:
    ModelOutputMatrixItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Get inference output blobs as a dictionary of matrices
 * 
 * Syntax:
 *   model_get_outputs("model_id") -> map of layer_name -> matrix
 */
class ModelGetOutputsItem : public InterpreterItem {
public:
    ModelGetOutputsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

} // namespace visionpipe

#endif // VISIONPIPE_DNN_ITEMS_H
