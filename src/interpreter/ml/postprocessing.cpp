#include "interpreter/ml/postprocessing.h"
#include <opencv2/imgproc.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace visionpipe {
namespace ml {

std::vector<int> Postprocessing::nms(const std::vector<cv::Rect>& boxes,
                                      const std::vector<float>& scores,
                                      float scoreThreshold,
                                      float nmsThreshold) {
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, scoreThreshold, nmsThreshold, indices);
    return indices;
}

std::vector<Detection> Postprocessing::decodeYolo(const cv::Mat& output,
                                                   cv::Size imageSize,
                                                   cv::Size inputSize,
                                                   float confThreshold,
                                                   const std::string& version) {
    if (version == "v8" || version == "v10" || version == "v11") {
        return decodeYolov8(output, imageSize, inputSize, confThreshold);
    } else if (version == "v5" || version == "v7") {
        return decodeYolov5(output, imageSize, inputSize, confThreshold);
    }
    
    // Default to v8 format
    return decodeYolov8(output, imageSize, inputSize, confThreshold);
}

std::vector<Detection> Postprocessing::decodeYolov8(const cv::Mat& output,
                                                     cv::Size imageSize,
                                                     cv::Size inputSize,
                                                     float confThreshold) {
    std::vector<Detection> detections;
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> classIds;
    
    // YOLOv8 output format: [1, 84, 8400] for COCO (4 box coords + 80 classes)
    // Need to transpose to [1, 8400, 84]
    cv::Mat data = output;
    
    // Handle different shapes
    if (data.dims == 3) {
        // Shape: [1, num_classes+4, num_detections]
        int rows = data.size[1];
        int cols = data.size[2];
        
        // Check if we need to transpose (YOLOv8 style)
        if (rows < cols) {
            // Transpose to [num_detections, num_classes+4]
            cv::Mat transposed(cols, rows, CV_32F);
            const float* srcData = data.ptr<float>();
            float* dstData = transposed.ptr<float>();
            
            for (int i = 0; i < rows; ++i) {
                for (int j = 0; j < cols; ++j) {
                    dstData[j * rows + i] = srcData[i * cols + j];
                }
            }
            data = transposed;
        } else {
            data = data.reshape(1, {rows, cols});
        }
    } else if (data.dims == 2) {
        // Already 2D, check orientation
        if (data.rows < data.cols && data.rows <= 100) {
            // Likely transposed, fix it
            cv::transpose(data, data);
        }
    }
    
    int numDetections = data.rows;
    int numClasses = data.cols - 4;  // First 4 are box coordinates
    
    if (numClasses <= 0) {
        return detections;
    }
    
    float scaleX = static_cast<float>(imageSize.width) / inputSize.width;
    float scaleY = static_cast<float>(imageSize.height) / inputSize.height;
    
    for (int i = 0; i < numDetections; ++i) {
        const float* row = data.ptr<float>(i);
        
        // Box: cx, cy, w, h
        float cx = row[0];
        float cy = row[1];
        float w = row[2];
        float h = row[3];
        
        // Find max class score
        float maxScore = 0;
        int maxClassId = 0;
        for (int c = 0; c < numClasses; ++c) {
            float score = row[4 + c];
            if (score > maxScore) {
                maxScore = score;
                maxClassId = c;
            }
        }
        
        if (maxScore < confThreshold) {
            continue;
        }
        
        // Convert to x, y, w, h and scale
        int x = static_cast<int>((cx - w / 2) * scaleX);
        int y = static_cast<int>((cy - h / 2) * scaleY);
        int boxW = static_cast<int>(w * scaleX);
        int boxH = static_cast<int>(h * scaleY);
        
        // Clamp to image bounds
        x = std::max(0, x);
        y = std::max(0, y);
        boxW = std::min(boxW, imageSize.width - x);
        boxH = std::min(boxH, imageSize.height - y);
        
        boxes.push_back(cv::Rect(x, y, boxW, boxH));
        scores.push_back(maxScore);
        classIds.push_back(maxClassId);
    }
    
    // Apply NMS
    std::vector<int> indices = nms(boxes, scores, confThreshold, 0.45f);
    
    for (int idx : indices) {
        Detection det;
        det.classId = classIds[idx];
        det.confidence = scores[idx];
        det.box = boxes[idx];
        detections.push_back(det);
    }
    
    return detections;
}

std::vector<Detection> Postprocessing::decodeYolov5(const cv::Mat& output,
                                                     cv::Size imageSize,
                                                     cv::Size inputSize,
                                                     float confThreshold) {
    std::vector<Detection> detections;
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> classIds;
    
    // YOLOv5 output: [1, num_detections, 85] for COCO (cx, cy, w, h, obj_conf, 80 class scores)
    cv::Mat data = output;
    
    if (data.dims == 3) {
        data = data.reshape(1, data.size[1]);
    }
    
    int numDetections = data.rows;
    int numClasses = data.cols - 5;  // 4 box + 1 obj_conf + classes
    
    if (numClasses <= 0) {
        return detections;
    }
    
    float scaleX = static_cast<float>(imageSize.width) / inputSize.width;
    float scaleY = static_cast<float>(imageSize.height) / inputSize.height;
    
    for (int i = 0; i < numDetections; ++i) {
        const float* row = data.ptr<float>(i);
        
        float objConf = row[4];
        if (objConf < confThreshold) {
            continue;
        }
        
        // Find max class score
        float maxScore = 0;
        int maxClassId = 0;
        for (int c = 0; c < numClasses; ++c) {
            float score = row[5 + c] * objConf;
            if (score > maxScore) {
                maxScore = score;
                maxClassId = c;
            }
        }
        
        if (maxScore < confThreshold) {
            continue;
        }
        
        // Box: cx, cy, w, h
        float cx = row[0];
        float cy = row[1];
        float w = row[2];
        float h = row[3];
        
        int x = static_cast<int>((cx - w / 2) * scaleX);
        int y = static_cast<int>((cy - h / 2) * scaleY);
        int boxW = static_cast<int>(w * scaleX);
        int boxH = static_cast<int>(h * scaleY);
        
        x = std::max(0, x);
        y = std::max(0, y);
        boxW = std::min(boxW, imageSize.width - x);
        boxH = std::min(boxH, imageSize.height - y);
        
        boxes.push_back(cv::Rect(x, y, boxW, boxH));
        scores.push_back(maxScore);
        classIds.push_back(maxClassId);
    }
    
    std::vector<int> indices = nms(boxes, scores, confThreshold, 0.45f);
    
    for (int idx : indices) {
        Detection det;
        det.classId = classIds[idx];
        det.confidence = scores[idx];
        det.box = boxes[idx];
        detections.push_back(det);
    }
    
    return detections;
}

cv::Mat Postprocessing::softmax(const cv::Mat& input, int axis) {
    cv::Mat output = input.clone();
    
    if (input.dims == 1 || (input.dims == 2 && input.rows == 1)) {
        // 1D softmax
        cv::Mat flat = input.reshape(1, 1);
        double maxVal;
        cv::minMaxLoc(flat, nullptr, &maxVal);
        
        cv::Mat expMat;
        cv::exp(flat - maxVal, expMat);
        
        double sum = cv::sum(expMat)[0];
        output = expMat / sum;
    } else {
        // Apply softmax along specified axis
        // For simplicity, only support common cases
        if (axis == 1 && input.dims == 2) {
            for (int i = 0; i < input.rows; ++i) {
                cv::Mat row = output.row(i);
                double maxVal;
                cv::minMaxLoc(row, nullptr, &maxVal);
                
                cv::Mat expRow;
                cv::exp(row - maxVal, expRow);
                
                double sum = cv::sum(expRow)[0];
                expRow.copyTo(row);
                row /= sum;
            }
        }
    }
    
    return output;
}

cv::Mat Postprocessing::sigmoid(const cv::Mat& input) {
    cv::Mat output;
    cv::Mat negInput;
    input.convertTo(negInput, CV_32F, -1.0);
    cv::exp(negInput, output);
    output = 1.0 / (1.0 + output);
    return output;
}

cv::Mat Postprocessing::argmax(const cv::Mat& input, int axis) {
    cv::Mat output;
    
    if (axis == 1 && input.dims == 2) {
        output = cv::Mat(input.rows, 1, CV_32S);
        for (int i = 0; i < input.rows; ++i) {
            cv::Point maxLoc;
            cv::minMaxLoc(input.row(i), nullptr, nullptr, nullptr, &maxLoc);
            output.at<int>(i, 0) = maxLoc.x;
        }
    } else if (axis == 0 && input.dims == 2) {
        output = cv::Mat(1, input.cols, CV_32S);
        for (int j = 0; j < input.cols; ++j) {
            cv::Point maxLoc;
            cv::minMaxLoc(input.col(j), nullptr, nullptr, nullptr, &maxLoc);
            output.at<int>(0, j) = maxLoc.y;
        }
    }
    
    return output;
}

std::vector<Classification> Postprocessing::topK(const cv::Mat& input, int k) {
    std::vector<Classification> results;
    
    cv::Mat flat = input.reshape(1, 1);
    if (flat.type() != CV_32F) {
        flat.convertTo(flat, CV_32F);
    }
    
    int numClasses = flat.cols;
    k = std::min(k, numClasses);
    
    // Create index-score pairs
    std::vector<std::pair<int, float>> indexedScores;
    for (int i = 0; i < numClasses; ++i) {
        indexedScores.push_back({i, flat.at<float>(0, i)});
    }
    
    // Partial sort for top-k
    std::partial_sort(indexedScores.begin(), indexedScores.begin() + k, 
                      indexedScores.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (int i = 0; i < k; ++i) {
        Classification cls;
        cls.classId = indexedScores[i].first;
        cls.confidence = indexedScores[i].second;
        results.push_back(cls);
    }
    
    return results;
}

void Postprocessing::scaleBoxes(std::vector<Detection>& detections,
                                 cv::Size fromSize, cv::Size toSize) {
    float scaleX = static_cast<float>(toSize.width) / fromSize.width;
    float scaleY = static_cast<float>(toSize.height) / fromSize.height;
    
    for (auto& det : detections) {
        det.box.x = static_cast<int>(det.box.x * scaleX);
        det.box.y = static_cast<int>(det.box.y * scaleY);
        det.box.width = static_cast<int>(det.box.width * scaleX);
        det.box.height = static_cast<int>(det.box.height * scaleY);
    }
}

void Postprocessing::rescaleBoxesLetterbox(std::vector<Detection>& detections,
                                            cv::Size inputSize, cv::Size imageSize,
                                            float scale, int padX, int padY) {
    for (auto& det : detections) {
        // Remove padding offset
        det.box.x -= padX;
        det.box.y -= padY;
        
        // Rescale
        det.box.x = static_cast<int>(det.box.x / scale);
        det.box.y = static_cast<int>(det.box.y / scale);
        det.box.width = static_cast<int>(det.box.width / scale);
        det.box.height = static_cast<int>(det.box.height / scale);
        
        // Clamp to image bounds
        det.box.x = std::max(0, det.box.x);
        det.box.y = std::max(0, det.box.y);
        det.box.width = std::min(det.box.width, imageSize.width - det.box.x);
        det.box.height = std::min(det.box.height, imageSize.height - det.box.y);
    }
}

std::vector<std::string> Postprocessing::loadLabels(const std::string& path) {
    std::vector<std::string> labels;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return labels;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Remove trailing whitespace/newline
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        if (!line.empty()) {
            labels.push_back(line);
        }
    }
    
    return labels;
}

void Postprocessing::applyLabels(std::vector<Detection>& detections,
                                  const std::vector<std::string>& labels) {
    for (auto& det : detections) {
        if (det.classId >= 0 && det.classId < static_cast<int>(labels.size())) {
            det.label = labels[det.classId];
        } else {
            det.label = "class_" + std::to_string(det.classId);
        }
    }
}

} // namespace ml
} // namespace visionpipe
