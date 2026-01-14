#ifndef VISIONPIPE_ARITHMETIC_ITEMS_H
#define VISIONPIPE_ARITHMETIC_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerArithmeticItems(ItemRegistry& registry);

// ============================================================================
// Matrix Operations
// ============================================================================

/**
 * @brief Matrix multiplication (gemm)
 * 
 * Parameters:
 * - other: Second matrix (from cache)
 * - alpha: Scale factor for product (default: 1.0)
 * - beta: Scale factor for addition (default: 0.0)
 * - gamma: Third matrix for addition (optional)
 * - flags: Transpose flags "trans_a", "trans_b", "trans_c"
 */
class GemmItem : public InterpreterItem {
public:
    GemmItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies matrix transformation to each pixel
 * 
 * Parameters:
 * - matrix_cache: Transformation matrix cache ID
 */
class TransformItem : public InterpreterItem {
public:
    TransformItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Matrix multiplication (simple)
 * 
 * Parameters:
 * - other: Second matrix (from cache)
 */
class MatMulItem : public InterpreterItem {
public:
    MatMulItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Transposes matrix
 */
class TransposeMatItem : public InterpreterItem {
public:
    TransposeMatItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Inverts matrix
 * 
 * Parameters:
 * - method: "lu", "cholesky", "svd" (default: "lu")
 */
class InvertItem : public InterpreterItem {
public:
    InvertItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates determinant
 * 
 * Returns: determinant value
 */
class DeterminantItem : public InterpreterItem {
public:
    DeterminantItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates matrix trace
 * 
 * Returns: trace value
 */
class TraceItem : public InterpreterItem {
public:
    TraceItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Performs SVD decomposition
 * 
 * Parameters:
 * - flags: "modify_a", "no_uv", "full_uv"
 * 
 * Returns: Stores U, W, Vt in cache as "{id}_u", "{id}_w", "{id}_vt"
 */
class SVDItem : public InterpreterItem {
public:
    SVDItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Solves linear system Ax = B
 * 
 * Parameters:
 * - b: Right-hand side matrix (from cache)
 * - flags: "lu", "cholesky", "svd", "qr", "normal"
 */
class SolveItem : public InterpreterItem {
public:
    SolveItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates eigenvalues and eigenvectors
 * 
 * Parameters:
 * - compute_eigenvectors: Also compute eigenvectors (default: true)
 * 
 * Returns: Stores eigenvalues and eigenvectors in cache
 */
class EigenItem : public InterpreterItem {
public:
    EigenItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates matrix norm
 * 
 * Parameters:
 * - norm_type: "l1", "l2", "inf", "hamming", "hamming2" (default: "l2")
 * - mask: Optional mask
 * 
 * Returns: norm value
 */
class NormItem : public InterpreterItem {
public:
    NormItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates PSNR (Peak Signal-to-Noise Ratio)
 * 
 * Parameters:
 * - other: Reference image (from cache)
 * - max_value: Maximum pixel value (default: 255)
 * 
 * Returns: PSNR in dB
 */
class PSNRItem : public InterpreterItem {
public:
    PSNRItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Normalizes matrix values to range
 * 
 * Parameters:
 * - alpha: Lower range boundary (default: 0)
 * - beta: Upper range boundary (default: 1)
 * - norm_type: Normalization type (default: "minmax")
 */
class NormalizeItem : public InterpreterItem {
public:
    NormalizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Reduces matrix to vector
 * 
 * Parameters:
 * - dim: Dimension to reduce (0=rows, 1=cols)
 * - rtype: Reduce operation ("sum", "avg", "max", "min")
 */
class ReduceItem : public InterpreterItem {
public:
    ReduceItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates sum of all elements
 * 
 * Returns: Sum value
 */
class SumItem : public InterpreterItem {
public:
    SumItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Counts non-zero elements
 * 
 * Returns: Count value
 */
class CountNonZeroItem : public InterpreterItem {
public:
    CountNonZeroItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates exponential of each element
 */
class ExpItem : public InterpreterItem {
public:
    ExpItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates natural logarithm of each element
 */
class NaturalLogItem : public InterpreterItem {
public:
    NaturalLogItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Raises each element to power
 * 
 * Parameters:
 * - power: Power value (default: 2.0)
 */
class PowItem : public InterpreterItem {
public:
    PowItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates square root of each element
 */
class SqrtItem : public InterpreterItem {
public:
    SqrtItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// DFT & FFT
// ============================================================================

/**
 * @brief Performs DFT (Discrete Fourier Transform)
 * 
 * Parameters:
 * - flags: "complex_output", "real_output", "inverse", "scale", "rows"
 * - nonzero_rows: Number of non-zero rows (default: 0)
 */
class DFTItem : public InterpreterItem {
public:
    DFTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Performs inverse DFT
 * 
 * Parameters:
 * - flags: "complex_output", "real_output", "rows"
 * - nonzero_rows: Number of non-zero rows
 */
class IDFTItem : public InterpreterItem {
public:
    IDFTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Performs DCT (Discrete Cosine Transform)
 * 
 * Parameters:
 * - flags: "inverse", "rows"
 */
class DCTItem : public InterpreterItem {
public:
    DCTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Performs inverse DCT
 */
class IDCTItem : public InterpreterItem {
public:
    IDCTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Multiplies two Fourier spectrums
 * 
 * Parameters:
 * - other: Second spectrum (from cache)
 * - conj: Conjugate second spectrum (default: false)
 */
class MulSpectrumsItem : public InterpreterItem {
public:
    MulSpectrumsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates optimal DFT size
 * 
 * Parameters:
 * - size: Input size
 * 
 * Returns: optimal size
 */
class GetOptimalDFTSizeItem : public InterpreterItem {
public:
    GetOptimalDFTSizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates magnitude from complex values
 * 
 * Parameters:
 * - x: X component (from cache)
 * - y: Y component (from cache)
 * 
 * Or uses current Mat if complex (2-channel)
 */
class MagnitudeItem : public InterpreterItem {
public:
    MagnitudeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates phase from complex values
 * 
 * Parameters:
 * - x: X component
 * - y: Y component
 * - angle_in_degrees: Output in degrees (default: false)
 */
class PhaseItem : public InterpreterItem {
public:
    PhaseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates magnitude and phase from x,y
 * 
 * Parameters:
 * - x: X component
 * - y: Y component
 * - angle_in_degrees: Degrees vs radians
 * 
 * Stores magnitude and angle in cache
 */
class CartToPolarItem : public InterpreterItem {
public:
    CartToPolarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates x,y from magnitude and phase
 */
class PolarToCartItem : public InterpreterItem {
public:
    PolarToCartItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Shifts zero-frequency component to center
 */
class FFTShiftItem : public InterpreterItem {
public:
    FFTShiftItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Random Operations
// ============================================================================

/**
 * @brief Fills Mat with normally distributed random values
 * 
 * Parameters:
 * - mean: Mean value (default: 0)
 * - stddev: Standard deviation (default: 1)
 */
class RandnItem : public InterpreterItem {
public:
    RandnItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fills Mat with uniformly distributed random values
 * 
 * Parameters:
 * - low: Lower bound (default: 0)
 * - high: Upper bound (default: 255)
 */
class RanduItem : public InterpreterItem {
public:
    RanduItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fills Mat with uniformly distributed random values
 * 
 * Parameters:
 * - low: Lower bound (default: 0)
 * - high: Upper bound (default: 255)
 */
class RandUniformItem : public InterpreterItem {
public:
    RandUniformItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fills Mat with normally distributed random values
 * 
 * Parameters:
 * - mean: Mean value (default: 0)
 * - stddev: Standard deviation (default: 1)
 */
class RandNormalItem : public InterpreterItem {
public:
    RandNormalItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Shuffles Mat elements
 * 
 * Parameters:
 * - iter_factor: Scaling factor for shuffle iterations (default: 1.0)
 */
class RandShuffleItem : public InterpreterItem {
public:
    RandShuffleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets random number generator seed
 * 
 * Parameters:
 * - seed: RNG seed value
 */
class SetRNGSeedItem : public InterpreterItem {
public:
    SetRNGSeedItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Lookup & Interpolation
// ============================================================================

/**
 * @brief Applies lookup table transformation
 * 
 * Parameters:
 * - lut: Lookup table (256 elements for 8-bit images)
 */
class LUTItem : public InterpreterItem {
public:
    LUTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Linear interpolation between two Mats
 * 
 * Parameters:
 * - other: Second Mat (from cache)
 * - t: Interpolation factor (0-1)
 */
class LerpItem : public InterpreterItem {
public:
    LerpItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Repeats Mat in x and y directions
 * 
 * Parameters:
 * - nx: Number of times to repeat horizontally
 * - ny: Number of times to repeat vertically
 */
class RepeatItem : public InterpreterItem {
public:
    RepeatItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Range & Fill
// ============================================================================

/**
 * @brief Fills Mat with a ramp pattern
 * 
 * Parameters:
 * - horizontal: Horizontal ramp (default: true)
 * - start: Start value (default: 0)
 * - end: End value (default: 255)
 */
class RampItem : public InterpreterItem {
public:
    RampItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Creates a gradient image
 * 
 * Parameters:
 * - start_color: Starting color [B, G, R]
 * - end_color: Ending color [B, G, R]
 * - direction: "horizontal", "vertical", "diagonal" (default: "horizontal")
 */
class GradientFillItem : public InterpreterItem {
public:
    GradientFillItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Creates a checkerboard pattern
 * 
 * Parameters:
 * - width: Image width
 * - height: Image height
 * - cell_size: Size of each checker cell (default: 50)
 * - color1: First color (default: [255, 255, 255])
 * - color2: Second color (default: [0, 0, 0])
 */
class CheckerboardItem : public InterpreterItem {
public:
    CheckerboardItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_ARITHMETIC_ITEMS_H
