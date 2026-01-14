#ifndef PIPELINE_THREADED_GROUP_H
#define PIPELINE_THREADED_GROUP_H

#include "pipeline/pipeline.h" // Assumes Pipeline class definition is here
#include <vector>
#include <map>
#include <cstdint>
#include <memory>
#include <future>
#include <mutex>
#include <functional> // Not directly used in public API but good for future patterns

/**
 * @class PipelineThreadedGroup
 * @brief Manages a group of pipelines, allowing them to run asynchronously and exchange data.
 *
 * This class provides a framework for orchestrating multiple image processing pipelines.
 * Pipelines can be added to the group, linked so that the output of one can be the input
 * of another, and triggered for execution. The execution is handled in separate threads
 * managed by std::async. The main application loop should periodically call `processEvents()`
 * to update pipeline states and handle data propagation.
 */
class PipelineThreadedGroup {
public:
    /**
     * @enum PipelineState
     * @brief Represents the current operational state of a pipeline within the group.
     */
    enum class PipelineState {
        IDLE,           ///< Pipeline is not running and ready to be triggered.
        PENDING_DATA,   ///< Pipeline has been triggered but is waiting for data from linked source pipelines.
        SCHEDULED,      ///< All data dependencies are met, and the pipeline is scheduled for asynchronous execution.
        RUNNING,        ///< Pipeline is actively being executed in a separate thread.
        COMPLETED,      ///< Pipeline has finished execution successfully.
        ERROR,          ///< An error occurred during the pipeline's execution.
        NOT_FOUND       ///< The requested pipeline ID does not exist in the group.
    };

    /**
     * @struct PipelineOutput
     * @brief Holds the result of a request to get a pipeline's output.
     *
     * Contains the pipeline's current status and its last direct output cv::Mat.
     * The `data` field is only valid and contains meaningful image data if the
     * `status` is `PipelineState::COMPLETED`.
     */
    struct PipelineOutput {
        PipelineState status; ///< The current state of the pipeline.
        cv::Mat data;         ///< The last direct output cv::Mat from the pipeline's `run()` method.
                              ///< Empty if status is not `COMPLETED` or if the pipeline itself returned an empty Mat.

        /**
         * @brief Default constructor. Initializes status to IDLE and data to an empty cv::Mat.
         */
        PipelineOutput() : status(PipelineState::IDLE), data() {}

        /**
         * @brief Constructor with initial values.
         * @param s The pipeline state.
         * @param m The cv::Mat data.
         */
        PipelineOutput(PipelineState s, const cv::Mat& m) : status(s), data(m) {}
    };

    /**
     * @struct PipelineInfo
     * @brief Internal structure to store information about each managed pipeline.
     */
    struct PipelineInfo {
        std::shared_ptr<Pipeline> ptr;                 ///< Shared pointer to the Pipeline object.
        PipelineState state = PipelineState::IDLE;     ///< Current state of the pipeline.
        std::future<cv::Mat> futureResult;             ///< Future object for the asynchronous pipeline execution.
        cv::Mat lastRunInitialInput;                   ///< The `cv::Mat` input provided at the last `trigger()` call.
        std::map<uint16_t, cv::Mat> pendingInputsFromLinks; ///< Data staged from completed source pipelines, ready for this pipeline's next run.
        cv::Mat lastRunDirectOutput;                   ///< Stores the direct `cv::Mat` result from the last successful `pipeline->run()`.
    };

    /**
     * @struct DataLink
     * @brief Defines a data dependency between two pipelines in the group.
     *
     * Specifies how an output (identified by a cache key) from a source pipeline
     * should be used as an input (identified by another cache key) for a target pipeline.
     */
    struct DataLink {
        uint16_t sourcePipelineId; ///< ID of the pipeline providing the data.
        uint16_t sourceCacheKey;   ///< Key in the source pipeline's internal `_matCache` to retrieve the data from.
        uint16_t targetPipelineId; ///< ID of the pipeline receiving the data.
        uint16_t targetCacheKey;   ///< Key to use when adding the data to the target pipeline's input cache for its next run.
    };

    /**
     * @brief Constructor for PipelineThreadedGroup.
     */
    PipelineThreadedGroup();

    /**
     * @brief Destructor for PipelineThreadedGroup.
     * Waits for any potentially running futures if not handled by explicit shutdown logic.
     */
    ~PipelineThreadedGroup();

    // --- Configuration ---

    /**
     * @brief Adds a pipeline to the group.
     * @param pipelineId A unique ID for this pipeline.
     * @param pipeline A shared_ptr to the Pipeline object.
     * @return True if the pipeline was added successfully, false otherwise (e.g., ID conflict, null pipeline).
     */
    bool addPipeline(uint16_t pipelineId, std::shared_ptr<Pipeline> pipeline);

    /**
     * @brief Defines a data dependency (link) between two pipelines.
     * When `sourcePipelineId` completes, data from its `sourceCacheKey` will be made
     * available to `targetPipelineId` as `targetCacheKey` for its next run.
     * @param link A DataLink struct describing the connection.
     * @return True if the link was added successfully, false otherwise (e.g., invalid pipeline IDs).
     */
    bool addDataLink(const DataLink& link);

    // --- User-driven Orchestration ---

    /**
     * @brief Attempts to trigger a pipeline for a single execution.
     * @param pipelineId The ID of the pipeline to trigger.
     * @param initialInputForRun The initial `cv::Mat` to pass to the pipeline's `run()` method. Can be empty.
     * @return True if successfully scheduled for execution or if already running/scheduled.
     *         False if the pipeline is `PENDING_DATA` (waiting for linked inputs) or if the ID is not found.
     */
    bool trigger(uint16_t pipelineId, const cv::Mat& initialInputForRun = cv::Mat());

    /**
     * @brief Manually provides data to a pipeline's pending input cache.
     * This can be used for pipelines that don't have all inputs satisfied by links,
     * or to provide data from sources external to the group. The data is cloned.
     * @param pipelineId The ID of the target pipeline.
     * @param cacheKey The key to use for storing this data in the pipeline's pending input cache.
     * @param data The `cv::Mat` data to feed.
     * @return True if data was fed successfully, false if pipeline ID not found.
     */
    bool feedData(uint16_t pipelineId, uint16_t cacheKey, const cv::Mat& data);

    /**
     * @brief Main processing tick for the group. Call this repeatedly in the application loop.
     *        - Checks for completion of any asynchronously running pipeline tasks.
     *        - Updates pipeline states (e.g., RUNNING -> COMPLETED/ERROR).
     *        - If a pipeline completes successfully, propagates its output data via defined DataLinks.
     *        - If a pipeline was `PENDING_DATA` and its dependencies are now met, it transitions to `IDLE`.
     * @return A list of pipeline IDs that have changed state to `COMPLETED` or `ERROR` during this tick.
     */
    std::vector<uint16_t> processEvents();

    // --- Getters ---

    /**
     * @brief Gets the current state of a specified pipeline.
     * @param pipelineId The ID of the pipeline.
     * @return The `PipelineState` of the pipeline. Returns `PipelineState::NOT_FOUND` if ID is invalid.
     */
    PipelineState getStatus(uint16_t pipelineId) const;

    /**
     * @brief Retrieves the last direct output of a pipeline and its current status.
     * The `data` field in the returned `PipelineOutput` struct will contain the image
     * if the status is `PipelineState::COMPLETED`. Otherwise, `data` will be an empty `cv::Mat`.
     * The returned `cv::Mat` is a clone of the internal stored output.
     * @param pipelineId The ID of the pipeline.
     * @return A `PipelineOutput` struct containing the status and potentially the data.
     *         If pipeline ID is not found, status will be `PipelineState::NOT_FOUND`.
     */
    PipelineOutput getOutput(uint16_t pipelineId);

    /**
     * @brief Gets a shared pointer to the underlying Pipeline object.
     * This allows access to the pipeline's internal cache (e.g., after completion) or other methods.
     * @param pipelineId The ID of the pipeline.
     * @return A `std::shared_ptr<Pipeline>` or `nullptr` if the ID is not found.
     */
    std::shared_ptr<Pipeline> getPipeline(uint16_t pipelineId);

    // --- Management ---

    /**
     * @brief Removes all pipelines and data links from the group.
     * This is a hard clear. If pipelines are running, their futures might be abandoned.
     * Ensure graceful shutdown if pipelines perform critical operations.
     */
    void clearAllPipelines();

    /**
     * @brief Clears the internal `_matCache` of a specific pipeline.
     * @param pipelineId The ID of the pipeline whose cache to clear.
     * @return True if successful, false if pipeline ID not found or pipeline pointer is null.
     */
    bool clearPipelineCache(uint16_t pipelineId);

    /**
     * @brief Clears the internal `_matCache` of all managed pipelines.
     */
    void clearAllPipelineCaches();

private:
    /**
     * @brief Internal helper to launch a pipeline's `run()` method asynchronously.
     * This method assumes the caller holds the `_mutex`.
     * @param pipelineId The ID of the pipeline to launch.
     * @param inputForRun The `cv::Mat` input for the pipeline's run.
     */
    void launchPipelineTask(uint16_t pipelineId, const cv::Mat& inputForRun);

    /**
     * @brief Internal helper to check if all data dependencies (from defined links) for a pipeline are met.
     * This method assumes the caller holds the `_mutex`.
     * @param pipelineId The ID of the pipeline to check.
     * @return True if all links targeting this pipeline are satisfied or no links target it, false otherwise.
     */
    bool checkDataLinksSatisfied(uint16_t pipelineId);

    std::map<uint16_t, PipelineInfo> _pipelines; ///< Map storing all managed pipelines and their info.
    std::vector<DataLink> _dataLinks;            ///< List of all defined data links between pipelines.
    mutable std::mutex _mutex;                   ///< Mutex to protect concurrent access to shared resources (_pipelines, _dataLinks, _satisfiedLinkInputs).

    /**
     * @brief Tracks which specific data links have provided data for a target pipeline's *next* run.
     * Key: targetPipelineId.
     * Value: map of {sourceCacheKey (from a DataLink struct) -> bool (true if satisfied)}.
     */
    std::map<uint16_t, std::map<uint16_t, bool>> _satisfiedLinkInputs;
};

#endif // PIPELINE_THREADED_GROUP_H