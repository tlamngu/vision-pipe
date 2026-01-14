#include "interpreter/items/dnn_items.h"
#include "interpreter/ml/preprocessing.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <sstream>

namespace visionpipe {

// Color palette for detection boxes
static const std::vector<cv::Scalar> DETECTION_COLORS = {
    cv::Scalar(255, 0, 0),      // Blue
    cv::Scalar(0, 255, 0),      // Green
    cv::Scalar(0, 0, 255),      // Red
    cv::Scalar(255, 255, 0),    // Cyan
    cv::Scalar(255, 0, 255),    // Magenta
    cv::Scalar(0, 255, 255),    // Yellow
    cv::Scalar(128, 0, 255),    // Orange
    cv::Scalar(255, 128, 0),    // Light blue
    cv::Scalar(128, 255, 0),    // Light green
    cv::Scalar(0, 128, 255),    // Orange-red
};

void registerDNNItems(ItemRegistry& registry) {
    // Model management
    registry.add<LoadModelItem>();
    registry.add<UnloadModelItem>();
    registry.add<ListModelsItem>();
    registry.add<ModelInfoItem>();
    
    // Inference
    registry.add<ModelInferItem>();
    registry.add<ModelOutputItem>();
    
    // Preprocessing
    registry.add<LetterboxItem>();
    registry.add<BlobFromImageItem>();
    registry.add<NormalizeMeanStdItem>();
    
    // Detection post-processing
    registry.add<DecodeYoloItem>();
    registry.add<NMSBoxesItem>();
    
    // Tensor operations
    registry.add<SoftmaxItem>();
    registry.add<SigmoidItem>();
    registry.add<TopKItem>();
    
    // Drawing
    registry.add<DrawDetectionsItem>();
    registry.add<DrawClassificationItem>();
    
    // Tensor output conversions (for raw DNN output access)
    registry.add<ModelOutputVectorItem>();
    registry.add<ModelOutputMatrixItem>();
    registry.add<ModelGetOutputsItem>();
}

// ============================================================================
// LoadModelItem
// ============================================================================

LoadModelItem::LoadModelItem() {
    _functionName = "load_model";
    _description = "Load an ML model for inference";
    _category = "dnn";
    _params = {
        ParamDef::required("id", BaseType::STRING, "Unique identifier for the model"),
        ParamDef::required("path", BaseType::STRING, "Path to model file"),
        ParamDef::optional("backend", BaseType::STRING, "Backend: opencv, onnx, tensorrt", "opencv"),
        ParamDef::optional("device", BaseType::STRING, "Device: cpu, cuda, cuda:0, opencl", "cpu"),
        ParamDef::optional("input_width", BaseType::INT, "Input width (0 = auto)", 0),
        ParamDef::optional("input_height", BaseType::INT, "Input height (0 = auto)", 0)
    };
    _example = "load_model(\"yolo\", \"models/yolov8n.onnx\", backend=\"opencv\", device=\"cuda\")";
    _returnType = "void";
    _tags = {"model", "load", "dnn", "ml", "inference"};
}

ExecutionResult LoadModelItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.size() < 2) {
        return ExecutionResult::fail("load_model requires at least id and path arguments");
    }
    
    std::string id = args[0].asString();
    std::string path = args[1].asString();
    
    ml::ModelConfig config;
    
    if (args.size() > 2) config.backend = args[2].asString();
    if (args.size() > 3) config.device = args[3].asString();
    if (args.size() > 4) {
        int w = static_cast<int>(args[4].asNumber());
        int h = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : w;
        config.inputSize = cv::Size(w, h);
    }
    
    auto& registry = ml::ModelRegistry::instance();
    if (!registry.loadModel(id, path, config)) {
        return ExecutionResult::fail("Failed to load model: " + path);
    }
    
    return ExecutionResult::ok();
}

// ============================================================================
// UnloadModelItem
// ============================================================================

UnloadModelItem::UnloadModelItem() {
    _functionName = "unload_model";
    _description = "Unload a previously loaded model";
    _category = "dnn";
    _params = {
        ParamDef::required("id", BaseType::STRING, "Model identifier to unload")
    };
    _example = "unload_model(\"yolo\")";
    _returnType = "void";
    _tags = {"model", "unload", "dnn"};
}

ExecutionResult UnloadModelItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("unload_model requires model id");
    }
    
    std::string id = args[0].asString();
    auto& registry = ml::ModelRegistry::instance();
    
    if (!registry.unloadModel(id)) {
        return ExecutionResult::fail("Model not found: " + id);
    }
    
    return ExecutionResult::ok();
}

// ============================================================================
// ListModelsItem
// ============================================================================

ListModelsItem::ListModelsItem() {
    _functionName = "list_models";
    _description = "List all loaded models";
    _category = "dnn";
    _params = {};
    _example = "list_models()";
    _returnType = "array";
    _tags = {"model", "list", "dnn"};
}

ExecutionResult ListModelsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    auto& registry = ml::ModelRegistry::instance();
    auto models = registry.listModels();
    
    std::vector<RuntimeValue> result;
    for (const auto& id : models) {
        result.push_back(RuntimeValue(id));
    }
    
    std::cout << "[list_models] Loaded models: " << models.size() << std::endl;
    for (const auto& id : models) {
        auto info = registry.getModelInfo(id);
        if (info) {
            std::cout << "  - " << id << " (" << info->backend << ", " 
                      << info->path << ")" << std::endl;
        }
    }
    
    return ExecutionResult::ok(RuntimeValue(result));
}

// ============================================================================
// ModelInfoItem
// ============================================================================

ModelInfoItem::ModelInfoItem() {
    _functionName = "model_info";
    _description = "Get information about a loaded model";
    _category = "dnn";
    _params = {
        ParamDef::required("id", BaseType::STRING, "Model identifier")
    };
    _example = "model_info(\"yolo\")";
    _returnType = "string";
    _tags = {"model", "info", "dnn"};
}

ExecutionResult ModelInfoItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("model_info requires model id");
    }
    
    std::string id = args[0].asString();
    auto& registry = ml::ModelRegistry::instance();
    
    auto info = registry.getModelInfo(id);
    if (!info) {
        return ExecutionResult::fail("Model not found: " + id);
    }
    
    std::stringstream ss;
    ss << "Model: " << info->id << "\n"
       << "  Path: " << info->path << "\n"
       << "  Backend: " << info->backend << "\n"
       << "  Device: " << info->device << "\n"
       << "  Input size: " << info->inputSize.width << "x" << info->inputSize.height << "\n"
       << "  Inferences: " << info->inferenceCount << "\n"
       << "  Avg time: " << info->averageInferenceTimeMs() << " ms";
    
    std::cout << ss.str() << std::endl;
    
    return ExecutionResult::ok(RuntimeValue(ss.str()));
}

// ============================================================================
// ModelInferItem
// ============================================================================

ModelInferItem::ModelInferItem() {
    _functionName = "model_infer";
    _description = "Run inference on the current frame using a loaded model";
    _category = "dnn";
    _params = {
        ParamDef::required("id", BaseType::STRING, "Model identifier")
    };
    _example = "model_infer(\"yolo\") -> \"output\"";
    _returnType = "mat";
    _tags = {"model", "inference", "dnn", "forward"};
}

ExecutionResult ModelInferItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("model_infer requires model id");
    }
    
    std::string id = args[0].asString();
    auto& registry = ml::ModelRegistry::instance();
    
    auto result = registry.runInference(id, ctx.currentMat);
    
    if (!result.success) {
        return ExecutionResult::fail(result.error.value_or("Inference failed"));
    }
    
    // Store inference time for debugging (only when verbose)
    if (ctx.verbose) {
        std::cout << "[model_infer] " << id << " inference time: " 
                  << result.inferenceTimeMs << " ms" << std::endl;
    }
    
    // Return primary output
    cv::Mat output = result.getPrimaryOutput();
    
    return ExecutionResult::ok(output);
}

// ============================================================================
// ModelOutputItem
// ============================================================================

ModelOutputItem::ModelOutputItem() {
    _functionName = "model_output";
    _description = "Get specific output layer from model inference";
    _category = "dnn";
    _params = {
        ParamDef::required("id", BaseType::STRING, "Model identifier"),
        ParamDef::required("layer", BaseType::STRING, "Output layer name")
    };
    _example = "model_output(\"yolo\", \"output0\")";
    _returnType = "mat";
    _tags = {"model", "output", "dnn"};
}

ExecutionResult ModelOutputItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.size() < 2) {
        return ExecutionResult::fail("model_output requires model id and layer name");
    }
    
    std::string id = args[0].asString();
    std::string layerName = args[1].asString();
    
    auto& registry = ml::ModelRegistry::instance();
    auto model = registry.getModel(id);
    
    if (!model) {
        return ExecutionResult::fail("Model not found: " + id);
    }
    
    // Note: This requires running inference first
    // For now, run inference and get specific output
    auto result = registry.runInference(id, ctx.currentMat);
    if (!result.success) {
        return ExecutionResult::fail(result.error.value_or("Inference failed"));
    }
    
    cv::Mat output = result.getOutput(layerName);
    if (output.empty()) {
        return ExecutionResult::fail("Output layer not found: " + layerName);
    }
    
    return ExecutionResult::ok(output);
}

// ============================================================================
// LetterboxItem
// ============================================================================

LetterboxItem::LetterboxItem() {
    _functionName = "letterbox";
    _description = "Resize image with letterbox (maintain aspect ratio with padding)";
    _category = "dnn";
    _params = {
        ParamDef::required("width", BaseType::INT, "Target width"),
        ParamDef::required("height", BaseType::INT, "Target height"),
        ParamDef::optional("pad_color", BaseType::INT, "Padding color (grayscale 0-255)", 114)
    };
    _example = "letterbox(640, 640)";
    _returnType = "mat";
    _tags = {"preprocess", "resize", "letterbox", "dnn"};
}

ExecutionResult LetterboxItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.size() < 2) {
        return ExecutionResult::fail("letterbox requires width and height");
    }
    
    int width = static_cast<int>(args[0].asNumber());
    int height = static_cast<int>(args[1].asNumber());
    int padColor = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 114;
    
    auto result = ml::Preprocessing::letterbox(
        ctx.currentMat, 
        cv::Size(width, height),
        cv::Scalar(padColor, padColor, padColor)
    );
    
    return ExecutionResult::ok(result.image);
}

// ============================================================================
// BlobFromImageItem
// ============================================================================

BlobFromImageItem::BlobFromImageItem() {
    _functionName = "blob_from_image";
    _description = "Create DNN blob from image for inference";
    _category = "dnn";
    _params = {
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor (default: 1/255)", 1.0/255.0),
        ParamDef::optional("width", BaseType::INT, "Target width (0 = original)", 0),
        ParamDef::optional("height", BaseType::INT, "Target height (0 = original)", 0),
        ParamDef::optional("swap_rb", BaseType::BOOL, "Swap R and B channels", true)
    };
    _example = "blob_from_image(1/255.0, 640, 640, true)";
    _returnType = "mat";
    _tags = {"preprocess", "blob", "dnn"};
}

ExecutionResult BlobFromImageItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double scale = args.size() > 0 ? args[0].asNumber() : 1.0/255.0;
    int width = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    int height = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;
    bool swapRB = args.size() > 3 ? args[3].asBool() : true;
    
    cv::Size size = (width > 0 && height > 0) ? cv::Size(width, height) : ctx.currentMat.size();
    
    cv::Mat blob = ml::Preprocessing::blobFromImage(
        ctx.currentMat, scale, size, cv::Scalar(0, 0, 0), swapRB, false
    );
    
    return ExecutionResult::ok(blob);
}

// ============================================================================
// NormalizeMeanStdItem
// ============================================================================

NormalizeMeanStdItem::NormalizeMeanStdItem() {
    _functionName = "normalize_mean_std";
    _description = "Normalize image with mean and std (ImageNet-style)";
    _category = "dnn";
    _params = {
        ParamDef::required("mean", BaseType::ARRAY, "Per-channel mean values [R, G, B]"),
        ParamDef::required("std", BaseType::ARRAY, "Per-channel std values [R, G, B]")
    };
    _example = "normalize_mean_std([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])";
    _returnType = "mat";
    _tags = {"preprocess", "normalize", "dnn"};
}

ExecutionResult NormalizeMeanStdItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.size() < 2) {
        return ExecutionResult::fail("normalize_mean_std requires mean and std arrays");
    }
    
    std::vector<double> mean, std;
    
    if (args[0].isArray()) {
        for (const auto& v : args[0].asArray()) {
            mean.push_back(v.asNumber());
        }
    }
    
    if (args[1].isArray()) {
        for (const auto& v : args[1].asArray()) {
            std.push_back(v.asNumber());
        }
    }
    
    if (mean.empty() || std.empty()) {
        return ExecutionResult::fail("Mean and std arrays cannot be empty");
    }
    
    cv::Mat result = ml::Preprocessing::normalizeMeanStd(ctx.currentMat, mean, std);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DecodeYoloItem
// ============================================================================

DecodeYoloItem::DecodeYoloItem() {
    _functionName = "decode_yolo";
    _description = "Decode YOLO model output to detections";
    _category = "dnn";
    _params = {
        ParamDef::optional("version", BaseType::STRING, "YOLO version: v5, v8, v10, v11", "v8"),
        ParamDef::optional("conf_thresh", BaseType::FLOAT, "Confidence threshold", 0.25),
        ParamDef::optional("orig_width", BaseType::INT, "Original image width", 0),
        ParamDef::optional("orig_height", BaseType::INT, "Original image height", 0),
        ParamDef::optional("input_size", BaseType::INT, "Model input size", 640)
    };
    _example = "decode_yolo(\"v8\", 0.25) -> \"detections\"";
    _returnType = "mat";
    _tags = {"postprocess", "yolo", "detection", "dnn"};
}

ExecutionResult DecodeYoloItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string version = args.size() > 0 ? args[0].asString() : "v8";
    float confThresh = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.25f;
    int origWidth = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 640;
    int origHeight = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 640;
    int inputSize = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 640;
    
    cv::Size imageSize(origWidth, origHeight);
    cv::Size modelInputSize(inputSize, inputSize);
    
    auto detections = ml::Postprocessing::decodeYolo(
        ctx.currentMat, imageSize, modelInputSize, confThresh, version
    );
    
    if (ctx.verbose) {
        std::cout << "[decode_yolo] Detected " << detections.size() << " objects" << std::endl;
    }
    
    // Store detections in a Mat format for caching
    // Format: each row = [x, y, w, h, classId, confidence]
    cv::Mat detMat(static_cast<int>(detections.size()), 6, CV_32F);
    for (size_t i = 0; i < detections.size(); ++i) {
        const auto& det = detections[i];
        detMat.at<float>(static_cast<int>(i), 0) = static_cast<float>(det.box.x);
        detMat.at<float>(static_cast<int>(i), 1) = static_cast<float>(det.box.y);
        detMat.at<float>(static_cast<int>(i), 2) = static_cast<float>(det.box.width);
        detMat.at<float>(static_cast<int>(i), 3) = static_cast<float>(det.box.height);
        detMat.at<float>(static_cast<int>(i), 4) = static_cast<float>(det.classId);
        detMat.at<float>(static_cast<int>(i), 5) = det.confidence;
    }
    
    return ExecutionResult::ok(detMat);
}

// ============================================================================
// NMSBoxesItem
// ============================================================================

NMSBoxesItem::NMSBoxesItem() {
    _functionName = "nms_boxes";
    _description = "Apply Non-Maximum Suppression to detections";
    _category = "dnn";
    _params = {
        ParamDef::optional("iou_thresh", BaseType::FLOAT, "IoU threshold for NMS", 0.45),
        ParamDef::optional("conf_thresh", BaseType::FLOAT, "Confidence threshold", 0.25)
    };
    _example = "nms_boxes(0.45, 0.25)";
    _returnType = "mat";
    _tags = {"postprocess", "nms", "detection", "dnn"};
}

ExecutionResult NMSBoxesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float iouThresh = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 0.45f;
    float confThresh = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.25f;
    
    // Expect currentMat to be detection matrix [N, 6] from decode_yolo
    cv::Mat detMat = ctx.currentMat;
    
    if (detMat.cols != 6 || detMat.empty()) {
        return ExecutionResult::ok(detMat);  // No detections or wrong format
    }
    
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    
    for (int i = 0; i < detMat.rows; ++i) {
        int x = static_cast<int>(detMat.at<float>(i, 0));
        int y = static_cast<int>(detMat.at<float>(i, 1));
        int w = static_cast<int>(detMat.at<float>(i, 2));
        int h = static_cast<int>(detMat.at<float>(i, 3));
        float conf = detMat.at<float>(i, 5);
        
        boxes.push_back(cv::Rect(x, y, w, h));
        scores.push_back(conf);
    }
    
    auto indices = ml::Postprocessing::nms(boxes, scores, confThresh, iouThresh);
    
    cv::Mat result(static_cast<int>(indices.size()), 6, CV_32F);
    for (size_t i = 0; i < indices.size(); ++i) {
        detMat.row(indices[i]).copyTo(result.row(static_cast<int>(i)));
    }
    
    if (ctx.verbose) {
        std::cout << "[nms_boxes] Kept " << indices.size() << " detections after NMS" << std::endl;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SoftmaxItem
// ============================================================================

SoftmaxItem::SoftmaxItem() {
    _functionName = "softmax";
    _description = "Apply softmax activation";
    _category = "dnn";
    _params = {
        ParamDef::optional("axis", BaseType::INT, "Axis for softmax", 1)
    };
    _example = "softmax()";
    _returnType = "mat";
    _tags = {"tensor", "activation", "softmax", "dnn"};
}

ExecutionResult SoftmaxItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int axis = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 1;
    
    cv::Mat result = ml::Postprocessing::softmax(ctx.currentMat, axis);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SigmoidItem
// ============================================================================

SigmoidItem::SigmoidItem() {
    _functionName = "sigmoid";
    _description = "Apply sigmoid activation";
    _category = "dnn";
    _params = {};
    _example = "sigmoid()";
    _returnType = "mat";
    _tags = {"tensor", "activation", "sigmoid", "dnn"};
}

ExecutionResult SigmoidItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat result = ml::Postprocessing::sigmoid(ctx.currentMat);
    return ExecutionResult::ok(result);
}

// ============================================================================
// TopKItem
// ============================================================================

TopKItem::TopKItem() {
    _functionName = "topk";
    _description = "Get top-k predictions from classification output";
    _category = "dnn";
    _params = {
        ParamDef::optional("k", BaseType::INT, "Number of top predictions", 5)
    };
    _example = "topk(5) -> \"predictions\"";
    _returnType = "mat";
    _tags = {"tensor", "classification", "topk", "dnn"};
}

ExecutionResult TopKItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int k = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    
    auto predictions = ml::Postprocessing::topK(ctx.currentMat, k);
    
    // Return as Mat [k, 2] with [classId, confidence]
    cv::Mat result(static_cast<int>(predictions.size()), 2, CV_32F);
    for (size_t i = 0; i < predictions.size(); ++i) {
        result.at<float>(static_cast<int>(i), 0) = static_cast<float>(predictions[i].classId);
        result.at<float>(static_cast<int>(i), 1) = predictions[i].confidence;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DrawDetectionsItem
// ============================================================================

DrawDetectionsItem::DrawDetectionsItem() {
    _functionName = "draw_detections";
    _description = "Draw detection boxes on the current frame";
    _category = "dnn";
    _params = {
        ParamDef::required("detections", BaseType::STRING, "Cache ID of detections matrix"),
        ParamDef::optional("labels", BaseType::STRING, "Path to labels file", ""),
        ParamDef::optional("show_conf", BaseType::BOOL, "Show confidence scores", true),
        ParamDef::optional("thickness", BaseType::INT, "Box line thickness", 2)
    };
    _example = "draw_detections(\"detections\", labels=\"coco.names\")";
    _returnType = "mat";
    _tags = {"draw", "detection", "visualization", "dnn"};
}

ExecutionResult DrawDetectionsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("draw_detections requires detections cache id");
    }
    
    std::string detectionsId = args[0].asString();
    std::string labelsPath = args.size() > 1 ? args[1].asString() : "";
    bool showConf = args.size() > 2 ? args[2].asBool() : true;
    int thickness = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 2;
    
    // Get detections from cache
    if (!ctx.cacheManager) {
        return ExecutionResult::fail("Cache manager not available");
    }
    
    cv::Mat detMat = ctx.cacheManager->get(detectionsId);
    if (detMat.empty()) {
        return ExecutionResult::ok(ctx.currentMat);  // No detections
    }
    
    // Load labels if provided
    std::vector<std::string> labels;
    if (!labelsPath.empty()) {
        labels = ml::Postprocessing::loadLabels(labelsPath);
    }
    
    cv::Mat result = ctx.currentMat.clone();
    
    for (int i = 0; i < detMat.rows; ++i) {
        int x = static_cast<int>(detMat.at<float>(i, 0));
        int y = static_cast<int>(detMat.at<float>(i, 1));
        int w = static_cast<int>(detMat.at<float>(i, 2));
        int h = static_cast<int>(detMat.at<float>(i, 3));
        int classId = static_cast<int>(detMat.at<float>(i, 4));
        float conf = detMat.at<float>(i, 5);
        
        cv::Rect box(x, y, w, h);
        cv::Scalar color = DETECTION_COLORS[classId % DETECTION_COLORS.size()];
        
        // Draw box
        cv::rectangle(result, box, color, thickness);
        
        // Prepare label
        std::string label;
        if (classId < static_cast<int>(labels.size())) {
            label = labels[classId];
        } else {
            label = "class_" + std::to_string(classId);
        }
        
        if (showConf) {
            char confStr[16];
            snprintf(confStr, sizeof(confStr), " %.2f", conf);
            label += confStr;
        }
        
        // Draw label background
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        cv::rectangle(result, 
            cv::Point(x, y - textSize.height - 4),
            cv::Point(x + textSize.width + 4, y),
            color, cv::FILLED);
        
        // Draw label text
        cv::putText(result, label, cv::Point(x + 2, y - 2), 
            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DrawClassificationItem
// ============================================================================

DrawClassificationItem::DrawClassificationItem() {
    _functionName = "draw_classification";
    _description = "Draw classification results on the current frame";
    _category = "dnn";
    _params = {
        ParamDef::required("predictions", BaseType::STRING, "Cache ID of predictions matrix"),
        ParamDef::optional("labels", BaseType::STRING, "Path to labels file", ""),
        ParamDef::optional("position", BaseType::STRING, "Text position: top-left, top-right, bottom-left, bottom-right", "top-left")
    };
    _example = "draw_classification(\"predictions\", labels=\"imagenet.txt\")";
    _returnType = "mat";
    _tags = {"draw", "classification", "visualization", "dnn"};
}

ExecutionResult DrawClassificationItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("draw_classification requires predictions cache id");
    }
    
    std::string predictionsId = args[0].asString();
    std::string labelsPath = args.size() > 1 ? args[1].asString() : "";
    std::string position = args.size() > 2 ? args[2].asString() : "top-left";
    
    if (!ctx.cacheManager) {
        return ExecutionResult::fail("Cache manager not available");
    }
    
    cv::Mat predMat = ctx.cacheManager->get(predictionsId);
    if (predMat.empty()) {
        return ExecutionResult::ok(ctx.currentMat);
    }
    
    // Load labels
    std::vector<std::string> labels;
    if (!labelsPath.empty()) {
        labels = ml::Postprocessing::loadLabels(labelsPath);
    }
    
    cv::Mat result = ctx.currentMat.clone();
    
    int y = 30;
    int x = 10;
    
    if (position == "top-right") {
        x = result.cols - 200;
    } else if (position == "bottom-left") {
        y = result.rows - 30 * predMat.rows;
    } else if (position == "bottom-right") {
        x = result.cols - 200;
        y = result.rows - 30 * predMat.rows;
    }
    
    for (int i = 0; i < predMat.rows && i < 5; ++i) {
        int classId = static_cast<int>(predMat.at<float>(i, 0));
        float conf = predMat.at<float>(i, 1);
        
        std::string label;
        if (classId < static_cast<int>(labels.size())) {
            label = labels[classId];
        } else {
            label = "class_" + std::to_string(classId);
        }
        
        char text[256];
        snprintf(text, sizeof(text), "%s: %.1f%%", label.c_str(), conf * 100);
        
        cv::putText(result, text, cv::Point(x, y), 
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
        
        y += 25;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ModelOutputVectorItem - Get DNN output as Vector type
// ============================================================================

ModelOutputVectorItem::ModelOutputVectorItem() {
    _functionName = "model_output_vector";
    _description = "Get raw DNN output as Vector (1D tensor)";
    _category = "dnn";
    _params = {
        ParamDef::required("id", BaseType::STRING, "Model identifier"),
        ParamDef::optional("layer", BaseType::ANY, "Layer name or index", 0)
    };
    _example = "model_output_vector(\"classifier\", 0) -> my_vector";
    _returnType = "vector";
    _tags = {"model", "output", "vector", "tensor", "dnn"};
}

ExecutionResult ModelOutputVectorItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("model_output_vector requires model id");
    }
    
    std::string id = args[0].asString();
    
    auto& registry = ml::ModelRegistry::instance();
    auto model = registry.getModel(id);
    if (!model) {
        return ExecutionResult::fail("Model not found: " + id);
    }
    
    // Run inference if needed
    auto result = registry.runInference(id, ctx.currentMat);
    if (!result.success) {
        return ExecutionResult::fail(result.error.value_or("Inference failed"));
    }
    
    // Get output by name or index
    cv::Mat output;
    if (args.size() > 1) {
        if (args[1].isString()) {
            output = result.getOutput(args[1].asString());
        } else {
            int idx = static_cast<int>(args[1].asNumber());
            if (idx < static_cast<int>(result.outputs.size())) {
                output = result.outputs[idx];
            }
        }
    } else {
        // Get first output
        if (!result.outputs.empty()) {
            output = result.outputs[0];
        }
    }
    
    if (output.empty()) {
        return ExecutionResult::fail("Output not found");
    }
    
    // Flatten to 1D and convert to Vector
    cv::Mat flat = output.reshape(1, 1);  // Reshape to 1 row
    if (flat.type() != CV_64F) {
        flat.convertTo(flat, CV_64F);
    }
    
    Vector vec(flat);
    return ExecutionResult::ok(RuntimeValue::fromVector(vec));
}

// ============================================================================
// ModelOutputMatrixItem - Get DNN output as Matrix type
// ============================================================================

ModelOutputMatrixItem::ModelOutputMatrixItem() {
    _functionName = "model_output_matrix";
    _description = "Get raw DNN output as Matrix (2D tensor)";
    _category = "dnn";
    _params = {
        ParamDef::required("id", BaseType::STRING, "Model identifier"),
        ParamDef::optional("layer", BaseType::ANY, "Layer name or index", 0)
    };
    _example = "model_output_matrix(\"detector\", \"output0\") -> my_matrix";
    _returnType = "matrix";
    _tags = {"model", "output", "matrix", "tensor", "dnn"};
}

ExecutionResult ModelOutputMatrixItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("model_output_matrix requires model id");
    }
    
    std::string id = args[0].asString();
    
    auto& registry = ml::ModelRegistry::instance();
    auto model = registry.getModel(id);
    if (!model) {
        return ExecutionResult::fail("Model not found: " + id);
    }
    
    // Run inference if needed
    auto result = registry.runInference(id, ctx.currentMat);
    if (!result.success) {
        return ExecutionResult::fail(result.error.value_or("Inference failed"));
    }
    
    // Get output by name or index
    cv::Mat output;
    if (args.size() > 1) {
        if (args[1].isString()) {
            output = result.getOutput(args[1].asString());
        } else {
            int idx = static_cast<int>(args[1].asNumber());
            if (idx < static_cast<int>(result.outputs.size())) {
                output = result.outputs[idx];
            }
        }
    } else {
        // Get first output
        if (!result.outputs.empty()) {
            output = result.outputs[0];
        }
    }
    
    if (output.empty()) {
        return ExecutionResult::fail("Output not found");
    }
    
    // Reshape to 2D if needed (keep original rows/cols or make Nx1)
    cv::Mat mat2d;
    if (output.dims > 2) {
        // Flatten multi-dim to 2D
        int total = output.total();
        mat2d = output.reshape(1, 1);  // 1 row
    } else {
        mat2d = output;
    }
    
    if (mat2d.type() != CV_64F) {
        mat2d.convertTo(mat2d, CV_64F);
    }
    
    Matrix matrix(mat2d);
    return ExecutionResult::ok(RuntimeValue::fromMatrix(matrix));
}

// ============================================================================
// ModelGetOutputsItem - Get all DNN outputs as dictionary
// ============================================================================

ModelGetOutputsItem::ModelGetOutputsItem() {
    _functionName = "model_get_outputs";
    _description = "Get all DNN outputs as array of [name, matrix] pairs";
    _category = "dnn";
    _params = {
        ParamDef::required("id", BaseType::STRING, "Model identifier")
    };
    _example = "model_get_outputs(\"yolo\")";
    _returnType = "array";
    _tags = {"model", "output", "all", "tensor", "dnn"};
}

ExecutionResult ModelGetOutputsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("model_get_outputs requires model id");
    }
    
    std::string id = args[0].asString();
    
    auto& registry = ml::ModelRegistry::instance();
    auto model = registry.getModel(id);
    if (!model) {
        return ExecutionResult::fail("Model not found: " + id);
    }
    
    // Run inference
    auto result = registry.runInference(id, ctx.currentMat);
    if (!result.success) {
        return ExecutionResult::fail(result.error.value_or("Inference failed"));
    }
    
    // Build array of [name, matrix] pairs
    std::vector<RuntimeValue> outputs;
    for (size_t i = 0; i < result.outputs.size(); ++i) {
        const cv::Mat& mat = result.outputs[i];
        std::string name = (i < result.outputNames.size()) ? result.outputNames[i] : "output_" + std::to_string(i);
        
        cv::Mat mat2d = mat;
        if (mat.dims > 2) {
            mat2d = mat.reshape(1, 1);
        }
        if (mat2d.type() != CV_64F) {
            mat2d.convertTo(mat2d, CV_64F);
        }
        
        std::vector<RuntimeValue> pair;
        pair.push_back(RuntimeValue(name));
        pair.push_back(RuntimeValue::fromMatrix(Matrix(mat2d)));
        outputs.push_back(RuntimeValue(pair));
    }
    
    return ExecutionResult::ok(RuntimeValue(outputs));
}

} // namespace visionpipe
