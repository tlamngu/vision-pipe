#ifndef VISIONPIPE_TENSOR_TYPES_H
#define VISIONPIPE_TENSOR_TYPES_H

#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/base.hpp>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace visionpipe {

/**
 * @brief Numeric vector type for VSP
 * 
 * A native 1D vector type supporting mathematical operations.
 * Internally uses cv::Mat for efficient computation.
 */
class Vector {
public:
    Vector() : _data(0, 1, CV_64F) {}
    
    explicit Vector(int size) : _data(size, 1, CV_64F, cv::Scalar(0)) {}
    
    Vector(int size, double fillValue) : _data(size, 1, CV_64F, cv::Scalar(fillValue)) {}
    
    explicit Vector(const std::vector<double>& values) 
        : _data(static_cast<int>(values.size()), 1, CV_64F) {
        for (size_t i = 0; i < values.size(); ++i) {
            _data.at<double>(static_cast<int>(i), 0) = values[i];
        }
    }
    
    explicit Vector(const cv::Mat& mat) {
        if (mat.cols == 1) {
            mat.convertTo(_data, CV_64F);
        } else if (mat.rows == 1) {
            cv::Mat transposed = mat.t();
            transposed.convertTo(_data, CV_64F);
        } else if (mat.total() > 0) {
            // Flatten to vector
            cv::Mat flat = mat.reshape(1, static_cast<int>(mat.total()));
            flat.convertTo(_data, CV_64F);
        } else {
            _data = cv::Mat(0, 1, CV_64F);
        }
    }
    
    // Size and access
    int size() const { return _data.rows; }
    bool empty() const { return _data.empty(); }
    
    double& operator[](int i) { return _data.at<double>(i, 0); }
    double operator[](int i) const { return _data.at<double>(i, 0); }
    
    double& at(int i) { 
        if (i < 0 || i >= size()) throw std::out_of_range("Vector index out of range");
        return _data.at<double>(i, 0); 
    }
    double at(int i) const { 
        if (i < 0 || i >= size()) throw std::out_of_range("Vector index out of range");
        return _data.at<double>(i, 0); 
    }
    
    // Convert to std::vector
    std::vector<double> toStdVector() const {
        std::vector<double> result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = _data.at<double>(i, 0);
        }
        return result;
    }
    
    // Get underlying Mat (column vector)
    const cv::Mat& data() const { return _data; }
    cv::Mat& data() { return _data; }
    
    // Arithmetic operations
    Vector operator+(const Vector& other) const {
        Vector result;
        cv::add(_data, other._data, result._data);
        return result;
    }
    
    Vector operator-(const Vector& other) const {
        Vector result;
        cv::subtract(_data, other._data, result._data);
        return result;
    }
    
    Vector operator*(double scalar) const {
        Vector result;
        _data.convertTo(result._data, CV_64F, scalar);
        return result;
    }
    
    Vector operator/(double scalar) const {
        if (scalar == 0) throw std::runtime_error("Division by zero");
        Vector result;
        _data.convertTo(result._data, CV_64F, 1.0 / scalar);
        return result;
    }
    
    // Element-wise multiplication
    Vector elementMul(const Vector& other) const {
        Vector result;
        cv::multiply(_data, other._data, result._data);
        return result;
    }
    
    // Element-wise division
    Vector elementDiv(const Vector& other) const {
        Vector result;
        cv::divide(_data, other._data, result._data);
        return result;
    }
    
    // Dot product
    double dot(const Vector& other) const {
        return _data.dot(other._data);
    }
    
    // Norm
    double norm(int normType = cv::NORM_L2) const {
        return cv::norm(_data, normType);
    }
    
    // Normalize
    Vector normalize(int normType = cv::NORM_L2) const {
        Vector result;
        cv::normalize(_data, result._data, 1.0, 0.0, normType);
        return result;
    }
    
    // Sum
    double sum() const {
        return cv::sum(_data)[0];
    }
    
    // Mean
    double mean() const {
        return cv::mean(_data)[0];
    }
    
    // Min/Max
    double min() const {
        double minVal;
        cv::minMaxLoc(_data, &minVal, nullptr);
        return minVal;
    }
    
    double max() const {
        double maxVal;
        cv::minMaxLoc(_data, nullptr, &maxVal);
        return maxVal;
    }
    
    // Comparison operators (element-wise, return Vector of 0/1)
    Vector operator==(const Vector& other) const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) == other._data.at<double>(i, 0)) ? 1.0 : 0.0;
        }
        return result;
    }
    
    Vector operator!=(const Vector& other) const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) != other._data.at<double>(i, 0)) ? 1.0 : 0.0;
        }
        return result;
    }
    
    Vector operator<(const Vector& other) const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) < other._data.at<double>(i, 0)) ? 1.0 : 0.0;
        }
        return result;
    }
    
    Vector operator>(const Vector& other) const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) > other._data.at<double>(i, 0)) ? 1.0 : 0.0;
        }
        return result;
    }
    
    Vector operator<=(const Vector& other) const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) <= other._data.at<double>(i, 0)) ? 1.0 : 0.0;
        }
        return result;
    }
    
    Vector operator>=(const Vector& other) const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) >= other._data.at<double>(i, 0)) ? 1.0 : 0.0;
        }
        return result;
    }
    
    // Boolean operations (element-wise)
    Vector logicalAnd(const Vector& other) const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) != 0 && other._data.at<double>(i, 0) != 0) ? 1.0 : 0.0;
        }
        return result;
    }
    
    Vector logicalOr(const Vector& other) const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) != 0 || other._data.at<double>(i, 0) != 0) ? 1.0 : 0.0;
        }
        return result;
    }
    
    Vector logicalNot() const {
        Vector result(size());
        for (int i = 0; i < size(); ++i) {
            result[i] = (_data.at<double>(i, 0) == 0) ? 1.0 : 0.0;
        }
        return result;
    }
    
    // All/Any
    bool all() const {
        for (int i = 0; i < size(); ++i) {
            if (_data.at<double>(i, 0) == 0) return false;
        }
        return true;
    }
    
    bool any() const {
        for (int i = 0; i < size(); ++i) {
            if (_data.at<double>(i, 0) != 0) return true;
        }
        return false;
    }
    
    // Slice
    Vector slice(int start, int end) const {
        if (start < 0) start = 0;
        if (end > size()) end = size();
        if (start >= end) return Vector();
        
        Vector result(end - start);
        for (int i = start; i < end; ++i) {
            result[i - start] = _data.at<double>(i, 0);
        }
        return result;
    }
    
    // Debug string
    std::string toString() const {
        std::ostringstream oss;
        oss << "Vector[" << size() << "](";
        for (int i = 0; i < std::min(size(), 10); ++i) {
            if (i > 0) oss << ", ";
            oss << std::fixed << std::setprecision(4) << _data.at<double>(i, 0);
        }
        if (size() > 10) oss << ", ...";
        oss << ")";
        return oss.str();
    }
    
private:
    cv::Mat _data;  // Column vector (N x 1)
};

/**
 * @brief Numeric matrix type for VSP
 * 
 * A native 2D matrix type supporting mathematical operations.
 * Internally uses cv::Mat for efficient computation.
 */
class Matrix {
public:
    Matrix() : _data(0, 0, CV_64F) {}
    
    Matrix(int rows, int cols) : _data(rows, cols, CV_64F, cv::Scalar(0)) {}
    
    Matrix(int rows, int cols, double fillValue) : _data(rows, cols, CV_64F, cv::Scalar(fillValue)) {}
    
    explicit Matrix(const cv::Mat& mat) {
        mat.convertTo(_data, CV_64F);
    }
    
    explicit Matrix(const std::vector<std::vector<double>>& values) {
        if (values.empty() || values[0].empty()) {
            _data = cv::Mat(0, 0, CV_64F);
            return;
        }
        
        int rows = static_cast<int>(values.size());
        int cols = static_cast<int>(values[0].size());
        _data = cv::Mat(rows, cols, CV_64F);
        
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < static_cast<int>(values[r].size()) && c < cols; ++c) {
                _data.at<double>(r, c) = values[r][c];
            }
        }
    }
    
    // Create identity matrix
    static Matrix identity(int size) {
        Matrix result(size, size);
        result._data = cv::Mat::eye(size, size, CV_64F);
        return result;
    }
    
    // Create diagonal matrix from vector
    static Matrix diag(const Vector& v) {
        Matrix result(v.size(), v.size(), 0.0);
        for (int i = 0; i < v.size(); ++i) {
            result._data.at<double>(i, i) = v[i];
        }
        return result;
    }
    
    // Size and access
    int rows() const { return _data.rows; }
    int cols() const { return _data.cols; }
    bool empty() const { return _data.empty(); }
    cv::Size size() const { return _data.size(); }
    
    double& operator()(int r, int c) { return _data.at<double>(r, c); }
    double operator()(int r, int c) const { return _data.at<double>(r, c); }
    
    double& at(int r, int c) { 
        if (r < 0 || r >= rows() || c < 0 || c >= cols()) 
            throw std::out_of_range("Matrix index out of range");
        return _data.at<double>(r, c); 
    }
    double at(int r, int c) const { 
        if (r < 0 || r >= rows() || c < 0 || c >= cols()) 
            throw std::out_of_range("Matrix index out of range");
        return _data.at<double>(r, c); 
    }
    
    // Get underlying Mat
    const cv::Mat& data() const { return _data; }
    cv::Mat& data() { return _data; }
    
    // Row/Column access
    Vector row(int r) const {
        std::vector<double> rowData(cols());
        for (int c = 0; c < cols(); ++c) {
            rowData[c] = _data.at<double>(r, c);
        }
        return Vector(rowData);
    }
    
    Vector col(int c) const {
        std::vector<double> colData(rows());
        for (int r = 0; r < rows(); ++r) {
            colData[r] = _data.at<double>(r, c);
        }
        return Vector(colData);
    }
    
    Vector diagonal() const {
        int minDim = std::min(rows(), cols());
        std::vector<double> diagData(minDim);
        for (int i = 0; i < minDim; ++i) {
            diagData[i] = _data.at<double>(i, i);
        }
        return Vector(diagData);
    }
    
    // Arithmetic operations
    Matrix operator+(const Matrix& other) const {
        Matrix result;
        cv::add(_data, other._data, result._data);
        return result;
    }
    
    Matrix operator-(const Matrix& other) const {
        Matrix result;
        cv::subtract(_data, other._data, result._data);
        return result;
    }
    
    Matrix operator*(const Matrix& other) const {
        Matrix result;
        cv::gemm(_data, other._data, 1.0, cv::Mat(), 0.0, result._data);
        return result;
    }
    
    Matrix operator*(double scalar) const {
        Matrix result;
        _data.convertTo(result._data, CV_64F, scalar);
        return result;
    }
    
    Matrix operator/(double scalar) const {
        if (scalar == 0) throw std::runtime_error("Division by zero");
        Matrix result;
        _data.convertTo(result._data, CV_64F, 1.0 / scalar);
        return result;
    }
    
    // Matrix-Vector multiplication
    Vector operator*(const Vector& v) const {
        cv::Mat result;
        cv::gemm(_data, v.data(), 1.0, cv::Mat(), 0.0, result);
        return Vector(result);
    }
    
    // Element-wise multiplication (Hadamard product)
    Matrix elementMul(const Matrix& other) const {
        Matrix result;
        cv::multiply(_data, other._data, result._data);
        return result;
    }
    
    // Element-wise division
    Matrix elementDiv(const Matrix& other) const {
        Matrix result;
        cv::divide(_data, other._data, result._data);
        return result;
    }
    
    // Transpose
    Matrix transpose() const {
        Matrix result;
        cv::transpose(_data, result._data);
        return result;
    }
    
    // Inverse
    Matrix inverse(int method = cv::DECOMP_LU) const {
        Matrix result;
        cv::invert(_data, result._data, method);
        return result;
    }
    
    // Determinant
    double determinant() const {
        return cv::determinant(_data);
    }
    
    // Trace
    double trace() const {
        return cv::trace(_data)[0];
    }
    
    // Norm (use NORM_L2 which is equivalent to Frobenius norm for matrices)
    double norm(int normType = cv::NORM_L2) const {
        return cv::norm(_data, normType);
    }
    
    // Sum/Mean
    double sum() const {
        return cv::sum(_data)[0];
    }
    
    double mean() const {
        return cv::mean(_data)[0];
    }
    
    // Min/Max
    double min() const {
        double minVal;
        cv::minMaxLoc(_data, &minVal, nullptr);
        return minVal;
    }
    
    double max() const {
        double maxVal;
        cv::minMaxLoc(_data, nullptr, &maxVal);
        return maxVal;
    }
    
    // Reshape
    Matrix reshape(int newRows, int newCols) const {
        if (newRows * newCols != rows() * cols()) {
            throw std::runtime_error("Cannot reshape: incompatible sizes");
        }
        Matrix result;
        result._data = _data.reshape(1, newRows).clone();
        if (result._data.cols != newCols) {
            result._data = result._data.reshape(1, newRows);
        }
        return result;
    }
    
    // Flatten to vector
    Vector flatten() const {
        return Vector(_data.reshape(1, static_cast<int>(_data.total())));
    }
    
    // Comparison operators (element-wise, return Matrix of 0/1)
    Matrix operator==(const Matrix& other) const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) == other._data.at<double>(r, c)) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    Matrix operator!=(const Matrix& other) const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) != other._data.at<double>(r, c)) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    Matrix operator<(const Matrix& other) const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) < other._data.at<double>(r, c)) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    Matrix operator>(const Matrix& other) const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) > other._data.at<double>(r, c)) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    Matrix operator<=(const Matrix& other) const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) <= other._data.at<double>(r, c)) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    Matrix operator>=(const Matrix& other) const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) >= other._data.at<double>(r, c)) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    // Boolean operations (element-wise)
    Matrix logicalAnd(const Matrix& other) const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) != 0 && other._data.at<double>(r, c) != 0) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    Matrix logicalOr(const Matrix& other) const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) != 0 || other._data.at<double>(r, c) != 0) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    Matrix logicalNot() const {
        Matrix result(rows(), cols());
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                result(r, c) = (_data.at<double>(r, c) == 0) ? 1.0 : 0.0;
            }
        }
        return result;
    }
    
    // All/Any
    bool all() const {
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                if (_data.at<double>(r, c) == 0) return false;
            }
        }
        return true;
    }
    
    bool any() const {
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < cols(); ++c) {
                if (_data.at<double>(r, c) != 0) return true;
            }
        }
        return false;
    }
    
    // Submatrix/Region
    Matrix submatrix(int rowStart, int rowEnd, int colStart, int colEnd) const {
        if (rowStart < 0) rowStart = 0;
        if (colStart < 0) colStart = 0;
        if (rowEnd > rows()) rowEnd = rows();
        if (colEnd > cols()) colEnd = cols();
        
        if (rowStart >= rowEnd || colStart >= colEnd) return Matrix();
        
        return Matrix(_data(cv::Range(rowStart, rowEnd), cv::Range(colStart, colEnd)).clone());
    }
    
    // Debug string
    std::string toString() const {
        std::ostringstream oss;
        oss << "Matrix[" << rows() << "x" << cols() << "]";
        
        if (rows() <= 5 && cols() <= 5) {
            oss << ":\n";
            for (int r = 0; r < rows(); ++r) {
                oss << "  [";
                for (int c = 0; c < cols(); ++c) {
                    if (c > 0) oss << ", ";
                    oss << std::fixed << std::setprecision(4) << _data.at<double>(r, c);
                }
                oss << "]\n";
            }
        }
        
        return oss.str();
    }
    
private:
    cv::Mat _data;
};

// Scalar * Vector
inline Vector operator*(double scalar, const Vector& v) {
    return v * scalar;
}

// Scalar * Matrix
inline Matrix operator*(double scalar, const Matrix& m) {
    return m * scalar;
}

} // namespace visionpipe

#endif // VISIONPIPE_TENSOR_TYPES_H
