#include "interpreter/items/tensor_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <cmath>

namespace visionpipe {

void registerTensorItems(ItemRegistry& registry) {
    // Vector creation
    registry.add<VecItem>();
    registry.add<VecZerosItem>();
    registry.add<VecOnesItem>();
    registry.add<VecRangeItem>();
    registry.add<VecLinspaceItem>();
    
    // Matrix creation
    registry.add<MatCreateItem>();
    registry.add<MatZerosItem>();
    registry.add<MatOnesItem>();
    registry.add<MatEyeItem>();
    registry.add<MatDiagItem>();
    
    // Vector operations
    registry.add<VecLenItem>();
    registry.add<VecGetItem>();
    registry.add<VecSetItem>();
    registry.add<VecSliceItem>();
    registry.add<VecDotItem>();
    registry.add<VecNormItem>();
    registry.add<VecNormalizeItem>();
    
    // Matrix operations
    registry.add<MatShapeItem>();
    registry.add<MatGetItem>();
    registry.add<MatSetItem>();
    registry.add<MatRowItem>();
    registry.add<MatColItem>();
    registry.add<MatTransposeItem>();
    registry.add<MatInvItem>();
    registry.add<MatDetItem>();
    registry.add<MatFlattenItem>();
    registry.add<MatReshapeItem>();
    
    // Arithmetic operations
    registry.add<TensorAddItem>();
    registry.add<TensorSubItem>();
    registry.add<TensorMulItem>();
    registry.add<TensorDivItem>();
    
    // Comparison operations
    registry.add<TensorEqItem>();
    registry.add<TensorNeItem>();
    registry.add<TensorLtItem>();
    registry.add<TensorGtItem>();
    registry.add<TensorLeItem>();
    registry.add<TensorGeItem>();
    
    // Boolean operations
    registry.add<TensorAndItem>();
    registry.add<TensorOrItem>();
    registry.add<TensorNotItem>();
    registry.add<TensorAllItem>();
    registry.add<TensorAnyItem>();
    
    // Reduction operations
    registry.add<TensorSumItem>();
    registry.add<TensorMeanItem>();
    registry.add<TensorMinItem>();
    registry.add<TensorMaxItem>();
    
    // Conversion
    registry.add<ToMatrixItem>();
    registry.add<ToCvMatItem>();
    registry.add<TensorPrintItem>();
}

// ============================================================================
// Vector Creation Items
// ============================================================================

VecItem::VecItem() {
    _functionName = "vec";
    _description = "Create a vector from values or array";
    _category = "tensor";
    _params = {
        ParamDef::optional("values", BaseType::ANY, "Values as varargs or array", RuntimeValue())
    };
    _example = "vec(1, 2, 3, 4) | vec([1.0, 2.0, 3.0])";
    _returnType = "vector";
    _tags = {"vector", "create", "tensor"};
}

ExecutionResult VecItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::vector<double> values;
    
    for (const auto& arg : args) {
        if (arg.isNumeric()) {
            values.push_back(arg.asNumber());
        } else if (arg.isArray()) {
            for (const auto& elem : arg.asArray()) {
                if (elem.isNumeric()) {
                    values.push_back(elem.asNumber());
                }
            }
        }
    }
    
    Vector v(values);
    return ExecutionResult::ok(RuntimeValue::fromVector(v));
}

VecZerosItem::VecZerosItem() {
    _functionName = "vec_zeros";
    _description = "Create a zero vector";
    _category = "tensor";
    _params = {
        ParamDef::required("size", BaseType::INT, "Vector size")
    };
    _example = "vec_zeros(10)";
    _returnType = "vector";
    _tags = {"vector", "create", "zeros", "tensor"};
}

ExecutionResult VecZerosItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int size = static_cast<int>(args[0].asNumber());
    Vector v(size, 0.0);
    return ExecutionResult::ok(RuntimeValue::fromVector(v));
}

VecOnesItem::VecOnesItem() {
    _functionName = "vec_ones";
    _description = "Create a vector of ones";
    _category = "tensor";
    _params = {
        ParamDef::required("size", BaseType::INT, "Vector size")
    };
    _example = "vec_ones(10)";
    _returnType = "vector";
    _tags = {"vector", "create", "ones", "tensor"};
}

ExecutionResult VecOnesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int size = static_cast<int>(args[0].asNumber());
    Vector v(size, 1.0);
    return ExecutionResult::ok(RuntimeValue::fromVector(v));
}

VecRangeItem::VecRangeItem() {
    _functionName = "vec_range";
    _description = "Create a vector with range of values";
    _category = "tensor";
    _params = {
        ParamDef::required("start", BaseType::FLOAT, "Start value"),
        ParamDef::required("end", BaseType::FLOAT, "End value (exclusive)"),
        ParamDef::optional("step", BaseType::FLOAT, "Step size", 1.0)
    };
    _example = "vec_range(0, 10) | vec_range(0, 10, 2)";
    _returnType = "vector";
    _tags = {"vector", "create", "range", "tensor"};
}

ExecutionResult VecRangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double start = args[0].asNumber();
    double end = args[1].asNumber();
    double step = args.size() > 2 ? args[2].asNumber() : 1.0;
    
    std::vector<double> values;
    if (step > 0) {
        for (double v = start; v < end; v += step) {
            values.push_back(v);
        }
    } else if (step < 0) {
        for (double v = start; v > end; v += step) {
            values.push_back(v);
        }
    }
    
    Vector vec(values);
    return ExecutionResult::ok(RuntimeValue::fromVector(vec));
}

VecLinspaceItem::VecLinspaceItem() {
    _functionName = "vec_linspace";
    _description = "Create a vector with linearly spaced values";
    _category = "tensor";
    _params = {
        ParamDef::required("start", BaseType::FLOAT, "Start value"),
        ParamDef::required("end", BaseType::FLOAT, "End value"),
        ParamDef::required("num", BaseType::INT, "Number of points")
    };
    _example = "vec_linspace(0, 1, 11)";
    _returnType = "vector";
    _tags = {"vector", "create", "linspace", "tensor"};
}

ExecutionResult VecLinspaceItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double start = args[0].asNumber();
    double end = args[1].asNumber();
    int num = static_cast<int>(args[2].asNumber());
    
    std::vector<double> values(num);
    if (num == 1) {
        values[0] = start;
    } else {
        double step = (end - start) / (num - 1);
        for (int i = 0; i < num; ++i) {
            values[i] = start + i * step;
        }
    }
    
    Vector vec(values);
    return ExecutionResult::ok(RuntimeValue::fromVector(vec));
}

// ============================================================================
// Matrix Creation Items
// ============================================================================

MatCreateItem::MatCreateItem() {
    _functionName = "mat_create";
    _description = "Create a matrix from nested arrays";
    _category = "tensor";
    _params = {
        ParamDef::required("data", BaseType::ARRAY, "2D array of values")
    };
    _example = "mat_create([[1, 2], [3, 4]])";
    _returnType = "matrix";
    _tags = {"matrix", "create", "tensor"};
}

ExecutionResult MatCreateItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty() || !args[0].isArray()) {
        return ExecutionResult::fail("mat_create requires a 2D array argument");
    }
    
    const auto& arr = args[0].asArray();
    if (arr.empty()) {
        return ExecutionResult::ok(RuntimeValue::fromMatrix(Matrix()));
    }
    
    std::vector<std::vector<double>> data;
    for (const auto& row : arr) {
        std::vector<double> rowData;
        if (row.isArray()) {
            for (const auto& elem : row.asArray()) {
                rowData.push_back(elem.asNumber());
            }
        } else if (row.isNumeric()) {
            rowData.push_back(row.asNumber());
        }
        data.push_back(rowData);
    }
    
    Matrix m(data);
    return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
}

MatZerosItem::MatZerosItem() {
    _functionName = "mat_zeros";
    _description = "Create a zero matrix";
    _category = "tensor";
    _params = {
        ParamDef::required("rows", BaseType::INT, "Number of rows"),
        ParamDef::required("cols", BaseType::INT, "Number of columns")
    };
    _example = "mat_zeros(3, 4)";
    _returnType = "matrix";
    _tags = {"matrix", "create", "zeros", "tensor"};
}

ExecutionResult MatZerosItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int rows = static_cast<int>(args[0].asNumber());
    int cols = static_cast<int>(args[1].asNumber());
    Matrix m(rows, cols, 0.0);
    return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
}

MatOnesItem::MatOnesItem() {
    _functionName = "mat_ones";
    _description = "Create a matrix of ones";
    _category = "tensor";
    _params = {
        ParamDef::required("rows", BaseType::INT, "Number of rows"),
        ParamDef::required("cols", BaseType::INT, "Number of columns")
    };
    _example = "mat_ones(3, 4)";
    _returnType = "matrix";
    _tags = {"matrix", "create", "ones", "tensor"};
}

ExecutionResult MatOnesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int rows = static_cast<int>(args[0].asNumber());
    int cols = static_cast<int>(args[1].asNumber());
    Matrix m(rows, cols, 1.0);
    return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
}

MatEyeItem::MatEyeItem() {
    _functionName = "mat_eye";
    _description = "Create an identity matrix";
    _category = "tensor";
    _params = {
        ParamDef::required("size", BaseType::INT, "Matrix size (NxN)")
    };
    _example = "mat_eye(4)";
    _returnType = "matrix";
    _tags = {"matrix", "create", "identity", "tensor"};
}

ExecutionResult MatEyeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int size = static_cast<int>(args[0].asNumber());
    Matrix m = Matrix::identity(size);
    return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
}

MatDiagItem::MatDiagItem() {
    _functionName = "mat_diag";
    _description = "Create a diagonal matrix from a vector";
    _category = "tensor";
    _params = {
        ParamDef::required("diagonal", BaseType::VECTOR, "Diagonal elements")
    };
    _example = "mat_diag(vec(1, 2, 3))";
    _returnType = "matrix";
    _tags = {"matrix", "create", "diagonal", "tensor"};
}

ExecutionResult MatDiagItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Vector v = args[0].asVector();
    Matrix m = Matrix::diag(v);
    return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
}

// ============================================================================
// Vector Operations Items
// ============================================================================

VecLenItem::VecLenItem() {
    _functionName = "vec_len";
    _description = "Get vector length";
    _category = "tensor";
    _params = {
        ParamDef::required("v", BaseType::VECTOR, "Input vector")
    };
    _example = "vec_len(v)";
    _returnType = "int";
    _tags = {"vector", "length", "size", "tensor"};
}

ExecutionResult VecLenItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Vector v = args[0].asVector();
    return ExecutionResult::ok(RuntimeValue(static_cast<int64_t>(v.size())));
}

VecGetItem::VecGetItem() {
    _functionName = "vec_get";
    _description = "Get vector element at index";
    _category = "tensor";
    _params = {
        ParamDef::required("v", BaseType::VECTOR, "Input vector"),
        ParamDef::required("index", BaseType::INT, "Element index")
    };
    _example = "vec_get(v, 0)";
    _returnType = "float";
    _tags = {"vector", "get", "index", "tensor"};
}

ExecutionResult VecGetItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Vector v = args[0].asVector();
    int idx = static_cast<int>(args[1].asNumber());
    
    if (idx < 0 || idx >= v.size()) {
        return ExecutionResult::fail("Vector index out of range");
    }
    
    return ExecutionResult::ok(RuntimeValue(v[idx]));
}

VecSetItem::VecSetItem() {
    _functionName = "vec_set";
    _description = "Set vector element at index";
    _category = "tensor";
    _params = {
        ParamDef::required("v", BaseType::VECTOR, "Input vector"),
        ParamDef::required("index", BaseType::INT, "Element index"),
        ParamDef::required("value", BaseType::FLOAT, "New value")
    };
    _example = "vec_set(v, 0, 5.0)";
    _returnType = "vector";
    _tags = {"vector", "set", "index", "tensor"};
}

ExecutionResult VecSetItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Vector v = args[0].asVector();
    int idx = static_cast<int>(args[1].asNumber());
    double value = args[2].asNumber();
    
    if (idx < 0 || idx >= v.size()) {
        return ExecutionResult::fail("Vector index out of range");
    }
    
    v[idx] = value;
    return ExecutionResult::ok(RuntimeValue::fromVector(v));
}

VecSliceItem::VecSliceItem() {
    _functionName = "vec_slice";
    _description = "Get vector slice";
    _category = "tensor";
    _params = {
        ParamDef::required("v", BaseType::VECTOR, "Input vector"),
        ParamDef::required("start", BaseType::INT, "Start index"),
        ParamDef::required("end", BaseType::INT, "End index (exclusive)")
    };
    _example = "vec_slice(v, 0, 5)";
    _returnType = "vector";
    _tags = {"vector", "slice", "tensor"};
}

ExecutionResult VecSliceItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Vector v = args[0].asVector();
    int start = static_cast<int>(args[1].asNumber());
    int end = static_cast<int>(args[2].asNumber());
    
    Vector result = v.slice(start, end);
    return ExecutionResult::ok(RuntimeValue::fromVector(result));
}

VecDotItem::VecDotItem() {
    _functionName = "vec_dot";
    _description = "Compute dot product of two vectors";
    _category = "tensor";
    _params = {
        ParamDef::required("v1", BaseType::VECTOR, "First vector"),
        ParamDef::required("v2", BaseType::VECTOR, "Second vector")
    };
    _example = "vec_dot(v1, v2)";
    _returnType = "float";
    _tags = {"vector", "dot", "product", "tensor"};
}

ExecutionResult VecDotItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Vector v1 = args[0].asVector();
    Vector v2 = args[1].asVector();
    
    double result = v1.dot(v2);
    return ExecutionResult::ok(RuntimeValue(result));
}

VecNormItem::VecNormItem() {
    _functionName = "vec_norm";
    _description = "Compute vector norm";
    _category = "tensor";
    _params = {
        ParamDef::required("v", BaseType::VECTOR, "Input vector"),
        ParamDef::optional("type", BaseType::STRING, "Norm type: l1, l2, inf", "l2")
    };
    _example = "vec_norm(v) | vec_norm(v, \"l1\")";
    _returnType = "float";
    _tags = {"vector", "norm", "tensor"};
}

ExecutionResult VecNormItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Vector v = args[0].asVector();
    std::string normType = args.size() > 1 ? args[1].asString() : "l2";
    
    int cvNorm = cv::NORM_L2;
    if (normType == "l1") cvNorm = cv::NORM_L1;
    else if (normType == "inf") cvNorm = cv::NORM_INF;
    
    double result = v.norm(cvNorm);
    return ExecutionResult::ok(RuntimeValue(result));
}

VecNormalizeItem::VecNormalizeItem() {
    _functionName = "vec_normalize";
    _description = "Normalize vector to unit length";
    _category = "tensor";
    _params = {
        ParamDef::required("v", BaseType::VECTOR, "Input vector")
    };
    _example = "vec_normalize(v)";
    _returnType = "vector";
    _tags = {"vector", "normalize", "tensor"};
}

ExecutionResult VecNormalizeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Vector v = args[0].asVector();
    Vector result = v.normalize();
    return ExecutionResult::ok(RuntimeValue::fromVector(result));
}

// ============================================================================
// Matrix Operations Items
// ============================================================================

MatShapeItem::MatShapeItem() {
    _functionName = "mat_shape";
    _description = "Get matrix dimensions [rows, cols]";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix")
    };
    _example = "mat_shape(m)";
    _returnType = "array";
    _tags = {"matrix", "shape", "size", "tensor"};
}

ExecutionResult MatShapeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    std::vector<RuntimeValue> shape = {
        RuntimeValue(static_cast<int64_t>(m.rows())),
        RuntimeValue(static_cast<int64_t>(m.cols()))
    };
    return ExecutionResult::ok(RuntimeValue(shape));
}

MatGetItem::MatGetItem() {
    _functionName = "mat_get";
    _description = "Get matrix element at (row, col)";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix"),
        ParamDef::required("row", BaseType::INT, "Row index"),
        ParamDef::required("col", BaseType::INT, "Column index")
    };
    _example = "mat_get(m, 0, 0)";
    _returnType = "float";
    _tags = {"matrix", "get", "index", "tensor"};
}

ExecutionResult MatGetItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    int row = static_cast<int>(args[1].asNumber());
    int col = static_cast<int>(args[2].asNumber());
    
    if (row < 0 || row >= m.rows() || col < 0 || col >= m.cols()) {
        return ExecutionResult::fail("Matrix index out of range");
    }
    
    return ExecutionResult::ok(RuntimeValue(m(row, col)));
}

MatSetItem::MatSetItem() {
    _functionName = "mat_set";
    _description = "Set matrix element at (row, col)";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix"),
        ParamDef::required("row", BaseType::INT, "Row index"),
        ParamDef::required("col", BaseType::INT, "Column index"),
        ParamDef::required("value", BaseType::FLOAT, "New value")
    };
    _example = "mat_set(m, 0, 0, 5.0)";
    _returnType = "matrix";
    _tags = {"matrix", "set", "index", "tensor"};
}

ExecutionResult MatSetItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    int row = static_cast<int>(args[1].asNumber());
    int col = static_cast<int>(args[2].asNumber());
    double value = args[3].asNumber();
    
    if (row < 0 || row >= m.rows() || col < 0 || col >= m.cols()) {
        return ExecutionResult::fail("Matrix index out of range");
    }
    
    m(row, col) = value;
    return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
}

MatRowItem::MatRowItem() {
    _functionName = "mat_row";
    _description = "Get matrix row as vector";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix"),
        ParamDef::required("row", BaseType::INT, "Row index")
    };
    _example = "mat_row(m, 0)";
    _returnType = "vector";
    _tags = {"matrix", "row", "tensor"};
}

ExecutionResult MatRowItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    int row = static_cast<int>(args[1].asNumber());
    
    if (row < 0 || row >= m.rows()) {
        return ExecutionResult::fail("Row index out of range");
    }
    
    Vector result = m.row(row);
    return ExecutionResult::ok(RuntimeValue::fromVector(result));
}

MatColItem::MatColItem() {
    _functionName = "mat_col";
    _description = "Get matrix column as vector";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix"),
        ParamDef::required("col", BaseType::INT, "Column index")
    };
    _example = "mat_col(m, 0)";
    _returnType = "vector";
    _tags = {"matrix", "column", "tensor"};
}

ExecutionResult MatColItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    int col = static_cast<int>(args[1].asNumber());
    
    if (col < 0 || col >= m.cols()) {
        return ExecutionResult::fail("Column index out of range");
    }
    
    Vector result = m.col(col);
    return ExecutionResult::ok(RuntimeValue::fromVector(result));
}

MatTransposeItem::MatTransposeItem() {
    _functionName = "mat_transpose";
    _description = "Transpose matrix";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix")
    };
    _example = "mat_transpose(m)";
    _returnType = "matrix";
    _tags = {"matrix", "transpose", "tensor"};
}

ExecutionResult MatTransposeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    Matrix result = m.transpose();
    return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
}

MatInvItem::MatInvItem() {
    _functionName = "mat_inv";
    _description = "Compute matrix inverse";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix")
    };
    _example = "mat_inv(m)";
    _returnType = "matrix";
    _tags = {"matrix", "inverse", "tensor"};
}

ExecutionResult MatInvItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    Matrix result = m.inverse();
    return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
}

MatDetItem::MatDetItem() {
    _functionName = "mat_det";
    _description = "Compute matrix determinant";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix")
    };
    _example = "mat_det(m)";
    _returnType = "float";
    _tags = {"matrix", "determinant", "tensor"};
}

ExecutionResult MatDetItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    double result = m.determinant();
    return ExecutionResult::ok(RuntimeValue(result));
}

MatFlattenItem::MatFlattenItem() {
    _functionName = "mat_flatten";
    _description = "Flatten matrix to vector";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix")
    };
    _example = "mat_flatten(m)";
    _returnType = "vector";
    _tags = {"matrix", "flatten", "tensor"};
}

ExecutionResult MatFlattenItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    Vector result = m.flatten();
    return ExecutionResult::ok(RuntimeValue::fromVector(result));
}

MatReshapeItem::MatReshapeItem() {
    _functionName = "mat_reshape";
    _description = "Reshape matrix to new dimensions";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix"),
        ParamDef::required("rows", BaseType::INT, "New rows"),
        ParamDef::required("cols", BaseType::INT, "New columns")
    };
    _example = "mat_reshape(m, 2, 6)";
    _returnType = "matrix";
    _tags = {"matrix", "reshape", "tensor"};
}

ExecutionResult MatReshapeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    int rows = static_cast<int>(args[1].asNumber());
    int cols = static_cast<int>(args[2].asNumber());
    
    try {
        Matrix result = m.reshape(rows, cols);
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    } catch (const std::exception& e) {
        return ExecutionResult::fail(e.what());
    }
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

TensorAddItem::TensorAddItem() {
    _functionName = "tensor_add";
    _description = "Add two tensors or scalar";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_add(v1, v2) | tensor_add(m, 5.0)";
    _returnType = "any";
    _tags = {"tensor", "add", "arithmetic"};
}

ExecutionResult TensorAddItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector() + b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix() + b.asMatrix();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isVector() && b.isNumeric()) {
        Vector v = a.asVector();
        double s = b.asNumber();
        for (int i = 0; i < v.size(); ++i) v[i] += s;
        return ExecutionResult::ok(RuntimeValue::fromVector(v));
    }
    
    if (a.isMatrix() && b.isNumeric()) {
        Matrix m = a.asMatrix();
        double s = b.asNumber();
        for (int r = 0; r < m.rows(); ++r) {
            for (int c = 0; c < m.cols(); ++c) {
                m(r, c) += s;
            }
        }
        return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() + b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot add incompatible types");
}

TensorSubItem::TensorSubItem() {
    _functionName = "tensor_sub";
    _description = "Subtract two tensors or scalar";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_sub(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "subtract", "arithmetic"};
}

ExecutionResult TensorSubItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector() - b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix() - b.asMatrix();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isVector() && b.isNumeric()) {
        Vector v = a.asVector();
        double s = b.asNumber();
        for (int i = 0; i < v.size(); ++i) v[i] -= s;
        return ExecutionResult::ok(RuntimeValue::fromVector(v));
    }
    
    if (a.isMatrix() && b.isNumeric()) {
        Matrix m = a.asMatrix();
        double s = b.asNumber();
        for (int r = 0; r < m.rows(); ++r) {
            for (int c = 0; c < m.cols(); ++c) {
                m(r, c) -= s;
            }
        }
        return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() - b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot subtract incompatible types");
}

TensorMulItem::TensorMulItem() {
    _functionName = "tensor_mul";
    _description = "Multiply tensors (matrix mul or element-wise)";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand"),
        ParamDef::optional("mode", BaseType::STRING, "Mode: matmul, elementwise", "matmul")
    };
    _example = "tensor_mul(m1, m2) | tensor_mul(v, 2.0)";
    _returnType = "any";
    _tags = {"tensor", "multiply", "arithmetic"};
}

ExecutionResult TensorMulItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    std::string mode = args.size() > 2 ? args[2].asString() : "matmul";
    
    if (a.isMatrix() && b.isMatrix()) {
        if (mode == "elementwise") {
            Matrix result = a.asMatrix().elementMul(b.asMatrix());
            return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
        } else {
            Matrix result = a.asMatrix() * b.asMatrix();
            return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
        }
    }
    
    if (a.isMatrix() && b.isVector()) {
        Vector result = a.asMatrix() * b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isVector() && b.isVector()) {
        if (mode == "elementwise") {
            Vector result = a.asVector().elementMul(b.asVector());
            return ExecutionResult::ok(RuntimeValue::fromVector(result));
        } else {
            // Dot product
            double result = a.asVector().dot(b.asVector());
            return ExecutionResult::ok(RuntimeValue(result));
        }
    }
    
    if ((a.isVector() || a.isMatrix()) && b.isNumeric()) {
        double s = b.asNumber();
        if (a.isVector()) {
            return ExecutionResult::ok(RuntimeValue::fromVector(a.asVector() * s));
        } else {
            return ExecutionResult::ok(RuntimeValue::fromMatrix(a.asMatrix() * s));
        }
    }
    
    if (a.isNumeric() && (b.isVector() || b.isMatrix())) {
        double s = a.asNumber();
        if (b.isVector()) {
            return ExecutionResult::ok(RuntimeValue::fromVector(b.asVector() * s));
        } else {
            return ExecutionResult::ok(RuntimeValue::fromMatrix(b.asMatrix() * s));
        }
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() * b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot multiply incompatible types");
}

TensorDivItem::TensorDivItem() {
    _functionName = "tensor_div";
    _description = "Divide tensors (element-wise)";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_div(v, 2.0)";
    _returnType = "any";
    _tags = {"tensor", "divide", "arithmetic"};
}

ExecutionResult TensorDivItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector().elementDiv(b.asVector());
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix().elementDiv(b.asMatrix());
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isVector() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue::fromVector(a.asVector() / b.asNumber()));
    }
    
    if (a.isMatrix() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue::fromMatrix(a.asMatrix() / b.asNumber()));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        if (b.asNumber() == 0) return ExecutionResult::fail("Division by zero");
        return ExecutionResult::ok(RuntimeValue(a.asNumber() / b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot divide incompatible types");
}

// ============================================================================
// Comparison Operations
// ============================================================================

TensorEqItem::TensorEqItem() {
    _functionName = "tensor_eq";
    _description = "Element-wise equality comparison";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_eq(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "compare", "equal"};
}

ExecutionResult TensorEqItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector() == b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix() == b.asMatrix();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() == b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compare incompatible types");
}

TensorNeItem::TensorNeItem() {
    _functionName = "tensor_ne";
    _description = "Element-wise not-equal comparison";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_ne(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "compare", "not-equal"};
}

ExecutionResult TensorNeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector() != b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix() != b.asMatrix();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() != b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compare incompatible types");
}

TensorLtItem::TensorLtItem() {
    _functionName = "tensor_lt";
    _description = "Element-wise less-than comparison";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_lt(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "compare", "less-than"};
}

ExecutionResult TensorLtItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector() < b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix() < b.asMatrix();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() < b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compare incompatible types");
}

TensorGtItem::TensorGtItem() {
    _functionName = "tensor_gt";
    _description = "Element-wise greater-than comparison";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_gt(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "compare", "greater-than"};
}

ExecutionResult TensorGtItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector() > b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix() > b.asMatrix();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() > b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compare incompatible types");
}

TensorLeItem::TensorLeItem() {
    _functionName = "tensor_le";
    _description = "Element-wise less-than-or-equal comparison";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_le(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "compare", "less-equal"};
}

ExecutionResult TensorLeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector() <= b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix() <= b.asMatrix();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() <= b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compare incompatible types");
}

TensorGeItem::TensorGeItem() {
    _functionName = "tensor_ge";
    _description = "Element-wise greater-than-or-equal comparison";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_ge(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "compare", "greater-equal"};
}

ExecutionResult TensorGeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector() >= b.asVector();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix() >= b.asMatrix();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isNumeric() && b.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber() >= b.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compare incompatible types");
}

// ============================================================================
// Boolean Operations
// ============================================================================

TensorAndItem::TensorAndItem() {
    _functionName = "tensor_and";
    _description = "Element-wise logical AND";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_and(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "logic", "and"};
}

ExecutionResult TensorAndItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector().logicalAnd(b.asVector());
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix().logicalAnd(b.asMatrix());
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isBool() && b.isBool()) {
        return ExecutionResult::ok(RuntimeValue(a.asBool() && b.asBool()));
    }
    
    return ExecutionResult::fail("Cannot apply AND to incompatible types");
}

TensorOrItem::TensorOrItem() {
    _functionName = "tensor_or";
    _description = "Element-wise logical OR";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "First operand"),
        ParamDef::required("b", BaseType::ANY, "Second operand")
    };
    _example = "tensor_or(v1, v2)";
    _returnType = "any";
    _tags = {"tensor", "logic", "or"};
}

ExecutionResult TensorOrItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    const auto& b = args[1];
    
    if (a.isVector() && b.isVector()) {
        Vector result = a.asVector().logicalOr(b.asVector());
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix() && b.isMatrix()) {
        Matrix result = a.asMatrix().logicalOr(b.asMatrix());
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isBool() && b.isBool()) {
        return ExecutionResult::ok(RuntimeValue(a.asBool() || b.asBool()));
    }
    
    return ExecutionResult::fail("Cannot apply OR to incompatible types");
}

TensorNotItem::TensorNotItem() {
    _functionName = "tensor_not";
    _description = "Element-wise logical NOT";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "Operand")
    };
    _example = "tensor_not(v)";
    _returnType = "any";
    _tags = {"tensor", "logic", "not"};
}

ExecutionResult TensorNotItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    
    if (a.isVector()) {
        Vector result = a.asVector().logicalNot();
        return ExecutionResult::ok(RuntimeValue::fromVector(result));
    }
    
    if (a.isMatrix()) {
        Matrix result = a.asMatrix().logicalNot();
        return ExecutionResult::ok(RuntimeValue::fromMatrix(result));
    }
    
    if (a.isBool()) {
        return ExecutionResult::ok(RuntimeValue(!a.asBool()));
    }
    
    return ExecutionResult::fail("Cannot apply NOT to this type");
}

TensorAllItem::TensorAllItem() {
    _functionName = "tensor_all";
    _description = "Check if all elements are true (non-zero)";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "Input tensor")
    };
    _example = "tensor_all(v)";
    _returnType = "bool";
    _tags = {"tensor", "logic", "all"};
}

ExecutionResult TensorAllItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    
    if (a.isVector()) {
        return ExecutionResult::ok(RuntimeValue(a.asVector().all()));
    }
    
    if (a.isMatrix()) {
        return ExecutionResult::ok(RuntimeValue(a.asMatrix().all()));
    }
    
    if (a.isBool()) {
        return ExecutionResult::ok(RuntimeValue(a.asBool()));
    }
    
    return ExecutionResult::fail("Cannot apply all() to this type");
}

TensorAnyItem::TensorAnyItem() {
    _functionName = "tensor_any";
    _description = "Check if any element is true (non-zero)";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "Input tensor")
    };
    _example = "tensor_any(v)";
    _returnType = "bool";
    _tags = {"tensor", "logic", "any"};
}

ExecutionResult TensorAnyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    
    if (a.isVector()) {
        return ExecutionResult::ok(RuntimeValue(a.asVector().any()));
    }
    
    if (a.isMatrix()) {
        return ExecutionResult::ok(RuntimeValue(a.asMatrix().any()));
    }
    
    if (a.isBool()) {
        return ExecutionResult::ok(RuntimeValue(a.asBool()));
    }
    
    return ExecutionResult::fail("Cannot apply any() to this type");
}

// ============================================================================
// Reduction Operations
// ============================================================================

TensorSumItem::TensorSumItem() {
    _functionName = "tensor_sum";
    _description = "Sum of all elements";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "Input tensor")
    };
    _example = "tensor_sum(v)";
    _returnType = "float";
    _tags = {"tensor", "reduce", "sum"};
}

ExecutionResult TensorSumItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    
    if (a.isVector()) {
        return ExecutionResult::ok(RuntimeValue(a.asVector().sum()));
    }
    
    if (a.isMatrix()) {
        return ExecutionResult::ok(RuntimeValue(a.asMatrix().sum()));
    }
    
    if (a.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot sum this type");
}

TensorMeanItem::TensorMeanItem() {
    _functionName = "tensor_mean";
    _description = "Mean of all elements";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "Input tensor")
    };
    _example = "tensor_mean(v)";
    _returnType = "float";
    _tags = {"tensor", "reduce", "mean"};
}

ExecutionResult TensorMeanItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    
    if (a.isVector()) {
        return ExecutionResult::ok(RuntimeValue(a.asVector().mean()));
    }
    
    if (a.isMatrix()) {
        return ExecutionResult::ok(RuntimeValue(a.asMatrix().mean()));
    }
    
    if (a.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compute mean for this type");
}

TensorMinItem::TensorMinItem() {
    _functionName = "tensor_min";
    _description = "Minimum element value";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "Input tensor")
    };
    _example = "tensor_min(v)";
    _returnType = "float";
    _tags = {"tensor", "reduce", "min"};
}

ExecutionResult TensorMinItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    
    if (a.isVector()) {
        return ExecutionResult::ok(RuntimeValue(a.asVector().min()));
    }
    
    if (a.isMatrix()) {
        return ExecutionResult::ok(RuntimeValue(a.asMatrix().min()));
    }
    
    if (a.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compute min for this type");
}

TensorMaxItem::TensorMaxItem() {
    _functionName = "tensor_max";
    _description = "Maximum element value";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "Input tensor")
    };
    _example = "tensor_max(v)";
    _returnType = "float";
    _tags = {"tensor", "reduce", "max"};
}

ExecutionResult TensorMaxItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    
    if (a.isVector()) {
        return ExecutionResult::ok(RuntimeValue(a.asVector().max()));
    }
    
    if (a.isMatrix()) {
        return ExecutionResult::ok(RuntimeValue(a.asMatrix().max()));
    }
    
    if (a.isNumeric()) {
        return ExecutionResult::ok(RuntimeValue(a.asNumber()));
    }
    
    return ExecutionResult::fail("Cannot compute max for this type");
}

// ============================================================================
// Conversion Operations
// ============================================================================

ToMatrixItem::ToMatrixItem() {
    _functionName = "to_matrix";
    _description = "Convert cv::Mat to Matrix type";
    _category = "tensor";
    _params = {};
    _example = "to_matrix()";
    _returnType = "matrix";
    _tags = {"tensor", "convert", "matrix"};
}

ExecutionResult ToMatrixItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m(ctx.currentMat);
    return ExecutionResult::ok(RuntimeValue::fromMatrix(m));
}

ToCvMatItem::ToCvMatItem() {
    _functionName = "to_cvmat";
    _description = "Convert Matrix to cv::Mat";
    _category = "tensor";
    _params = {
        ParamDef::required("m", BaseType::MATRIX, "Input matrix")
    };
    _example = "to_cvmat(m)";
    _returnType = "mat";
    _tags = {"tensor", "convert", "cvmat"};
}

ExecutionResult ToCvMatItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    Matrix m = args[0].asMatrix();
    return ExecutionResult::ok(m.data().clone());
}

TensorPrintItem::TensorPrintItem() {
    _functionName = "tensor_print";
    _description = "Print tensor information";
    _category = "tensor";
    _params = {
        ParamDef::required("a", BaseType::ANY, "Input tensor")
    };
    _example = "tensor_print(v)";
    _returnType = "void";
    _tags = {"tensor", "debug", "print"};
}

ExecutionResult TensorPrintItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    const auto& a = args[0];
    
    if (a.isVector()) {
        std::cout << a.asVector().toString() << std::endl;
    } else if (a.isMatrix()) {
        std::cout << a.asMatrix().toString() << std::endl;
    } else {
        std::cout << a.debugString() << std::endl;
    }
    
    return ExecutionResult::ok();
}

} // namespace visionpipe
