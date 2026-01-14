#ifndef VISIONPIPE_TENSOR_ITEMS_H
#define VISIONPIPE_TENSOR_ITEMS_H

#include "interpreter/item_registry.h"
#include "interpreter/tensor_types.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

/**
 * @brief Register all tensor (Vector/Matrix) items
 * Core feature - always available
 */
void registerTensorItems(ItemRegistry& registry);

// ============================================================================
// Vector Creation Items
// ============================================================================

/**
 * @brief Create a vector from values
 * 
 * Syntax:
 *   vec(1, 2, 3, 4, 5)
 *   vec([1, 2, 3, 4, 5])
 */
class VecItem : public InterpreterItem {
public:
    VecItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Create a vector of zeros
 * 
 * Syntax:
 *   vec_zeros(10)
 */
class VecZerosItem : public InterpreterItem {
public:
    VecZerosItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Create a vector of ones
 * 
 * Syntax:
 *   vec_ones(10)
 */
class VecOnesItem : public InterpreterItem {
public:
    VecOnesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Create a vector with range
 * 
 * Syntax:
 *   vec_range(0, 10)        # 0 to 9
 *   vec_range(0, 10, 2)     # 0, 2, 4, 6, 8
 */
class VecRangeItem : public InterpreterItem {
public:
    VecRangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Create a linspace vector
 * 
 * Syntax:
 *   vec_linspace(0, 1, 11)  # 11 points from 0 to 1
 */
class VecLinspaceItem : public InterpreterItem {
public:
    VecLinspaceItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Matrix Creation Items
// ============================================================================

/**
 * @brief Create a matrix from nested arrays
 * 
 * Syntax:
 *   mat([[1, 2], [3, 4]])
 */
class MatCreateItem : public InterpreterItem {
public:
    MatCreateItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Create a zero matrix
 * 
 * Syntax:
 *   mat_zeros(3, 4)
 */
class MatZerosItem : public InterpreterItem {
public:
    MatZerosItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Create a ones matrix
 * 
 * Syntax:
 *   mat_ones(3, 4)
 */
class MatOnesItem : public InterpreterItem {
public:
    MatOnesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Create an identity matrix
 * 
 * Syntax:
 *   mat_eye(4)
 */
class MatEyeItem : public InterpreterItem {
public:
    MatEyeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Create a diagonal matrix from vector
 * 
 * Syntax:
 *   mat_diag(vec(1, 2, 3))
 */
class MatDiagItem : public InterpreterItem {
public:
    MatDiagItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Vector Operations Items
// ============================================================================

/**
 * @brief Get vector length/size
 * 
 * Syntax:
 *   vec_len(v)
 */
class VecLenItem : public InterpreterItem {
public:
    VecLenItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Get vector element at index
 * 
 * Syntax:
 *   vec_get(v, 0)
 */
class VecGetItem : public InterpreterItem {
public:
    VecGetItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Set vector element at index
 * 
 * Syntax:
 *   vec_set(v, 0, 5.0)
 */
class VecSetItem : public InterpreterItem {
public:
    VecSetItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Slice vector
 * 
 * Syntax:
 *   vec_slice(v, 0, 5)
 */
class VecSliceItem : public InterpreterItem {
public:
    VecSliceItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Dot product of two vectors
 * 
 * Syntax:
 *   vec_dot(v1, v2)
 */
class VecDotItem : public InterpreterItem {
public:
    VecDotItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Vector norm
 * 
 * Syntax:
 *   vec_norm(v)
 *   vec_norm(v, "l1")
 */
class VecNormItem : public InterpreterItem {
public:
    VecNormItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Normalize vector
 * 
 * Syntax:
 *   vec_normalize(v)
 */
class VecNormalizeItem : public InterpreterItem {
public:
    VecNormalizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Matrix Operations Items
// ============================================================================

/**
 * @brief Get matrix dimensions
 * 
 * Syntax:
 *   mat_shape(m)  # returns [rows, cols]
 */
class MatShapeItem : public InterpreterItem {
public:
    MatShapeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Get matrix element
 * 
 * Syntax:
 *   mat_get(m, row, col)
 */
class MatGetItem : public InterpreterItem {
public:
    MatGetItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Set matrix element
 * 
 * Syntax:
 *   mat_set(m, row, col, value)
 */
class MatSetItem : public InterpreterItem {
public:
    MatSetItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Get matrix row as vector
 * 
 * Syntax:
 *   mat_row(m, 0)
 */
class MatRowItem : public InterpreterItem {
public:
    MatRowItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Get matrix column as vector
 * 
 * Syntax:
 *   mat_col(m, 0)
 */
class MatColItem : public InterpreterItem {
public:
    MatColItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Matrix transpose
 * 
 * Syntax:
 *   mat_transpose(m)
 *   mat_t(m)
 */
class MatTransposeItem : public InterpreterItem {
public:
    MatTransposeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Matrix inverse
 * 
 * Syntax:
 *   mat_inv(m)
 */
class MatInvItem : public InterpreterItem {
public:
    MatInvItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Matrix determinant
 * 
 * Syntax:
 *   mat_det(m)
 */
class MatDetItem : public InterpreterItem {
public:
    MatDetItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Flatten matrix to vector
 * 
 * Syntax:
 *   mat_flatten(m)
 */
class MatFlattenItem : public InterpreterItem {
public:
    MatFlattenItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Reshape matrix
 * 
 * Syntax:
 *   mat_reshape(m, rows, cols)
 */
class MatReshapeItem : public InterpreterItem {
public:
    MatReshapeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Arithmetic Operation Items (work with Vector, Matrix, and scalars)
// ============================================================================

/**
 * @brief Add two tensors or scalar
 * 
 * Syntax:
 *   tensor_add(a, b)
 */
class TensorAddItem : public InterpreterItem {
public:
    TensorAddItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Subtract two tensors or scalar
 * 
 * Syntax:
 *   tensor_sub(a, b)
 */
class TensorSubItem : public InterpreterItem {
public:
    TensorSubItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Multiply tensors (matrix mul or element-wise)
 * 
 * Syntax:
 *   tensor_mul(a, b)
 *   tensor_mul(a, b, "elementwise")
 */
class TensorMulItem : public InterpreterItem {
public:
    TensorMulItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Divide tensors (element-wise)
 * 
 * Syntax:
 *   tensor_div(a, b)
 */
class TensorDivItem : public InterpreterItem {
public:
    TensorDivItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Comparison and Boolean Operation Items
// ============================================================================

/**
 * @brief Element-wise equality
 * 
 * Syntax:
 *   tensor_eq(a, b)
 */
class TensorEqItem : public InterpreterItem {
public:
    TensorEqItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Element-wise not equal
 * 
 * Syntax:
 *   tensor_ne(a, b)
 */
class TensorNeItem : public InterpreterItem {
public:
    TensorNeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Element-wise less than
 * 
 * Syntax:
 *   tensor_lt(a, b)
 */
class TensorLtItem : public InterpreterItem {
public:
    TensorLtItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Element-wise greater than
 * 
 * Syntax:
 *   tensor_gt(a, b)
 */
class TensorGtItem : public InterpreterItem {
public:
    TensorGtItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Element-wise less than or equal
 * 
 * Syntax:
 *   tensor_le(a, b)
 */
class TensorLeItem : public InterpreterItem {
public:
    TensorLeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Element-wise greater than or equal
 * 
 * Syntax:
 *   tensor_ge(a, b)
 */
class TensorGeItem : public InterpreterItem {
public:
    TensorGeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Logical AND
 * 
 * Syntax:
 *   tensor_and(a, b)
 */
class TensorAndItem : public InterpreterItem {
public:
    TensorAndItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Logical OR
 * 
 * Syntax:
 *   tensor_or(a, b)
 */
class TensorOrItem : public InterpreterItem {
public:
    TensorOrItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Logical NOT
 * 
 * Syntax:
 *   tensor_not(a)
 */
class TensorNotItem : public InterpreterItem {
public:
    TensorNotItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Check if all elements are true
 * 
 * Syntax:
 *   tensor_all(a)
 */
class TensorAllItem : public InterpreterItem {
public:
    TensorAllItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Check if any element is true
 * 
 * Syntax:
 *   tensor_any(a)
 */
class TensorAnyItem : public InterpreterItem {
public:
    TensorAnyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Reduction/Statistics Items
// ============================================================================

/**
 * @brief Sum of elements
 * 
 * Syntax:
 *   tensor_sum(a)
 */
class TensorSumItem : public InterpreterItem {
public:
    TensorSumItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Mean of elements
 * 
 * Syntax:
 *   tensor_mean(a)
 */
class TensorMeanItem : public InterpreterItem {
public:
    TensorMeanItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Min of elements
 * 
 * Syntax:
 *   tensor_min(a)
 */
class TensorMinItem : public InterpreterItem {
public:
    TensorMinItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Max of elements
 * 
 * Syntax:
 *   tensor_max(a)
 */
class TensorMaxItem : public InterpreterItem {
public:
    TensorMaxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Conversion Items
// ============================================================================

/**
 * @brief Convert cv::Mat to Matrix
 * 
 * Syntax:
 *   to_matrix(mat)
 */
class ToMatrixItem : public InterpreterItem {
public:
    ToMatrixItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Convert Matrix to cv::Mat
 * 
 * Syntax:
 *   to_cvmat(matrix)
 */
class ToCvMatItem : public InterpreterItem {
public:
    ToCvMatItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Print tensor info
 * 
 * Syntax:
 *   tensor_print(a)
 */
class TensorPrintItem : public InterpreterItem {
public:
    TensorPrintItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

} // namespace visionpipe

#endif // VISIONPIPE_TENSOR_ITEMS_H
