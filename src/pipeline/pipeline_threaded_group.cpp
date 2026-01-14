#include "pipeline/pipeline_threaded_group.h"
#include <iostream> // For std::cerr, std::cout (debugging)
#include <algorithm> // For std::find_if or other algorithms if needed

PipelineThreadedGroup::PipelineThreadedGroup() = default;

PipelineThreadedGroup::~PipelineThreadedGroup() {
    // For a robust application, ensure pipelines are properly stopped or completed
    // before the group is destroyed. This destructor provides a basic check.
    // std::lock_guard<std::mutex> lock(_mutex); // Not strictly needed if clearAll is called first
    // for (auto& pair : _pipelines) {
    //     if (pair.second.futureResult.valid() &&
    //         pair.second.futureResult.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    //         // Consider logging or specific handling for pipelines still running at destruction.
    //         // Forcibly waiting here (pair.second.futureResult.wait()) could block indefinitely.
    //     }
    // }
}

bool PipelineThreadedGroup::addPipeline(uint16_t pipelineId, std::shared_ptr<Pipeline> pipeline) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!pipeline) {
        std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Attempted to add a null pipeline for ID " << pipelineId << std::endl;
        return false;
    }
    
    if (_pipelines.count(pipelineId)) {
        std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Pipeline with ID " << pipelineId << " already exists." << std::endl;
        return false;
    }

    PipelineInfo newInfo; 
    newInfo.ptr = pipeline;
    newInfo.state = PipelineState::IDLE;
    auto insertResult = _pipelines.emplace(pipelineId, std::move(newInfo));

    if (!insertResult.second) {
        std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Failed to emplace pipeline with ID " << pipelineId << ". It might already exist." << std::endl;
        return false;
    }
    
    _satisfiedLinkInputs[pipelineId] = {}; 

    return true;
}

bool PipelineThreadedGroup::addDataLink(const DataLink& link) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_pipelines.count(link.sourcePipelineId) || !_pipelines.count(link.targetPipelineId)) {
        std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Invalid pipeline ID in data link definition. "
                  << "Source: " << link.sourcePipelineId << ", Target: " << link.targetPipelineId << std::endl;
        return false;
    }
    _dataLinks.push_back(link);

    _satisfiedLinkInputs[link.targetPipelineId][link.sourceCacheKey] = false;
    return true;
}

bool PipelineThreadedGroup::feedData(uint16_t pipelineId, uint16_t cacheKey, const cv::Mat& data) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pipelines.find(pipelineId);
    if (it == _pipelines.end()) {
        std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Pipeline ID " << pipelineId << " not found for feedData." << std::endl;
        return false;
    }

    // Store data in the pipeline's pending input cache; it will be moved to the
    // pipeline's actual _matCache by launchPipelineTask right before running.
    it->second.pendingInputsFromLinks[cacheKey] = data.clone(); // Clone to ensure data ownership by the group

    // Note: This doesn't directly change _satisfiedLinkInputs. Data links are satisfied by
    // completed source pipelines via processEvents. feedData is for external/manual input.
    return true;
}

bool PipelineThreadedGroup::checkDataLinksSatisfied(uint16_t pipelineId) {
    // This private helper assumes the _mutex is already held by the caller.
    // Iterate over all defined data links.
    for (const auto& link : _dataLinks) {
        if (link.targetPipelineId == pipelineId) {
            // This link targets the pipeline we are checking.
            // Now, check if this specific link's input has been satisfied.
            const auto& satisfactionMapForTarget = _satisfiedLinkInputs[pipelineId];
            auto itSatisfaction = satisfactionMapForTarget.find(link.sourceCacheKey);

            if (itSatisfaction == satisfactionMapForTarget.end()) {
                // This indicates an internal inconsistency: a link is defined in _dataLinks
                // but not tracked in _satisfiedLinkInputs. This should not happen if addDataLink is correct.
                // Consider it unsatisfied for safety.
                // std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Internal inconsistency for link to target " 
                //           << pipelineId << " from source key " << link.sourceCacheKey << std::endl;
                return false;
            }
            if (!itSatisfaction->second) { // If 'second' is false, this link is not satisfied
                return false; // At least one required link is not satisfied
            }
        }
    }
    return true; // All links targeting this pipeline are satisfied, or no links target it.
}

void PipelineThreadedGroup::launchPipelineTask(uint16_t pipelineId, const cv::Mat& inputForRun) {
    // This private helper assumes the _mutex is already held by the caller (trigger).
    PipelineInfo& info = _pipelines[pipelineId]; // Assumes pipelineId exists, checked by trigger
    std::shared_ptr<Pipeline> pipeline_ptr = info.ptr;

    // 1. Prepare pipeline's internal cache for this specific run.
    pipeline_ptr->clearCache(); // Start with a clean cache for this run.
    for(const auto& entry : info.pendingInputsFromLinks) {
        // Data was cloned when put into pendingInputsFromLinks, so direct assignment is fine.
        pipeline_ptr->addMatToCache(entry.first, entry.second);
    }
    info.pendingInputsFromLinks.clear(); // These inputs have been consumed for this run.

    // 2. Reset satisfaction flags for all links that target this pipeline.
    // This means for the *next* run of this pipeline, all its linked data
    // will need to be provided afresh by its source pipelines.
    if (_satisfiedLinkInputs.count(pipelineId)) {
        for (auto& satisfactionEntry : _satisfiedLinkInputs[pipelineId]) {
            satisfactionEntry.second = false; // Mark as unsatisfied for the next cycle.
        }
    }

    // 3. Set state, clear previous output, and launch.
    info.state = PipelineState::RUNNING;
    info.lastRunDirectOutput.release(); // Clear previous run's direct output before new run
    cv::Mat capturedInput = inputForRun.clone(); // Clone for thread safety if inputForRun is temporary.

    info.futureResult = std::async(std::launch::async,
        [pipeline_ptr, capturedInput]() { // Capture by value/shared_ptr.
            try {
                // The pipeline's run method is executed in a separate thread.
                return pipeline_ptr->run(const_cast<cv::Mat&>(capturedInput));
            } catch (...) {
                // Propagate exceptions to be caught by future.get() in processEvents.
                std::throw_with_nested(std::runtime_error("Exception during pipeline run: " + (pipeline_ptr ? pipeline_ptr->_id : "UNKNOWN_PIPELINE_ID")));
            }
        });
    // std::cout << "PipelineThreadedGroup [" << __func__ << "]: Pipeline " << pipelineId << " (" 
    //           << (pipeline_ptr ? pipeline_ptr->_id : "N/A") << ") launched." << std::endl;
}

bool PipelineThreadedGroup::trigger(uint16_t pipelineId, const cv::Mat& initialInputForRun) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pipelines.find(pipelineId);
    if (it == _pipelines.end()) {
        std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Pipeline ID " << pipelineId << " not found for trigger." << std::endl;
        return false;
    }

    PipelineInfo& info = it->second;
    if (info.state == PipelineState::RUNNING || info.state == PipelineState::SCHEDULED) {
        // std::cout << "PipelineThreadedGroup [" << __func__ << "]: Pipeline " << pipelineId << " is already running or scheduled." << std::endl;
        return true; // Indicate it's already in a processing state, not an error.
    }

    // Allow re-triggering from ERROR state, effectively resetting it.
    if (info.state == PipelineState::ERROR) {
        info.state = PipelineState::IDLE;
    }
    
    info.lastRunInitialInput = initialInputForRun.clone();

    // Check if all data links for this pipeline are satisfied.
    if (!checkDataLinksSatisfied(pipelineId)) {
        info.state = PipelineState::PENDING_DATA;
        // std::cout << "PipelineThreadedGroup [" << __func__ << "]: Pipeline " << pipelineId << " is PENDING_DATA (links not satisfied)." << std::endl;
        return false; // Cannot trigger yet, waiting for linked data.
    }

    // If all data links are satisfied (or no links defined for it), schedule for execution.
    info.state = PipelineState::SCHEDULED;
    launchPipelineTask(pipelineId, info.lastRunInitialInput);
    return true;
}

std::vector<uint16_t> PipelineThreadedGroup::processEvents() {
    std::vector<uint16_t> stateChangedPipelines;
    std::unique_lock<std::mutex> lock(_mutex); // Use unique_lock if condition variables are added later.

    for (auto& pair : _pipelines) {
        uint16_t id = pair.first;
        PipelineInfo& info = pair.second;

        // --- 1. Check for completion of RUNNING or SCHEDULED pipelines ---
        if (info.state == PipelineState::RUNNING || info.state == PipelineState::SCHEDULED) {
            if (info.futureResult.valid()) {
                auto status = info.futureResult.wait_for(std::chrono::seconds(0)); // Non-blocking check.
                if (status == std::future_status::ready) {
                    try {
                        cv::Mat pipelineRunResult = info.futureResult.get(); // Retrieve result; rethrows exceptions from async task.
                        info.lastRunDirectOutput = pipelineRunResult.clone(); // Store the direct output

                        info.state = PipelineState::COMPLETED;
                        stateChangedPipelines.push_back(id);
                        // std::cout << "PipelineThreadedGroup [" << __func__ << "]: Pipeline " << id << " (" 
                        //           << (info.ptr ? info.ptr->_id : "N/A") << ") COMPLETED." << std::endl;

                        // --- 2. Data Propagation for COMPLETED pipelines ---
                        // If this pipeline (id) is a source in any DataLink, propagate its output.
                        for (const auto& link : _dataLinks) {
                            if (link.sourcePipelineId == id) {
                                cv::Mat outputDataFromCache; // Data for linking still comes from the source's *internal* cache
                                if (info.ptr && info.ptr->getMatFromCache(link.sourceCacheKey, outputDataFromCache)) {
                                    auto targetIt = _pipelines.find(link.targetPipelineId);
                                    if (targetIt != _pipelines.end()){
                                        PipelineInfo& targetPipelineInfo = targetIt->second;
                                        // Add data to the target's *pending* input cache for its *next* run.
                                        targetPipelineInfo.pendingInputsFromLinks[link.targetCacheKey] = outputDataFromCache.clone();
                                        // Mark this specific input dependency for the target as now satisfied.
                                        _satisfiedLinkInputs[link.targetPipelineId][link.sourceCacheKey] = true;
                                        // std::cout << "PipelineThreadedGroup [" << __func__ << "]: Data from " << id << " (key " << link.sourceCacheKey
                                        //           << ") propagated to " << link.targetPipelineId << " (key " << link.targetCacheKey << ")." << std::endl;
                                    }
                                } else {
                                    if(info.ptr) { // Check ptr to avoid dereferencing null if pipeline was removed oddly
                                        std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Warning - Source pipeline " << id
                                                  << " completed, but data for link sourceCacheKey " << link.sourceCacheKey 
                                                  << " not found in its cache." << std::endl;
                                    }
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Pipeline " << id << " (" 
                                  << (info.ptr ? info.ptr->_id : "N/A") << ") transitioned to ERROR: " << e.what() << std::endl;
                        // Attempt to log nested exceptions if any
                        try { std::rethrow_if_nested(e); }
                        catch (const std::exception& nested_e) { std::cerr << "  Nested exception: " << nested_e.what() << std::endl; }
                        
                        info.state = PipelineState::ERROR;
                        info.lastRunDirectOutput.release(); // Clear output on error
                        stateChangedPipelines.push_back(id);
                    }
                    info.futureResult = {}; // Invalidate the future as its result has been processed.
                }
            } else if (info.state == PipelineState::SCHEDULED && !info.futureResult.valid()) { 
                // This case implies it was SCHEDULED, but launchPipelineTask was somehow skipped or future was cleared prematurely.
                // This indicates a potential logic error in the state machine or usage.
                std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Pipeline " << id 
                          << " was SCHEDULED but its future is invalid. Transitioning to ERROR." << std::endl;
                info.state = PipelineState::ERROR; 
                stateChangedPipelines.push_back(id);
            }
        }

        // --- 3. Check PENDING_DATA pipelines ---
        // If a pipeline was waiting for data, and data might have just been propagated,
        // check if its dependencies are now met. If so, transition it to IDLE.
        // The user can then decide to trigger it in their main loop (e.g., if it's a processing pipeline).
        if (info.state == PipelineState::PENDING_DATA) {
            if (checkDataLinksSatisfied(id)) {
                 info.state = PipelineState::IDLE; // Data is now available, ready to be triggered.
                //  std::cout << "PipelineThreadedGroup [" << __func__ << "]: Pipeline " << id 
                //            << " was PENDING_DATA, now IDLE (data available)." << std::endl;
                 stateChangedPipelines.push_back(id); // Inform user it's ready for potential trigger.
            }
        }
    }
    return stateChangedPipelines;
}

PipelineThreadedGroup::PipelineState PipelineThreadedGroup::getStatus(uint16_t pipelineId) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pipelines.find(pipelineId);
    if (it != _pipelines.end()) {
        return it->second.state;
    }
    // std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Pipeline ID " << pipelineId << " not found for getStatus." << std::endl;
    return PipelineThreadedGroup::PipelineState::NOT_FOUND;
}

PipelineThreadedGroup::PipelineOutput PipelineThreadedGroup::getOutput(uint16_t pipelineId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pipelines.find(pipelineId);
    if (it == _pipelines.end()) {
        // std::cerr << "PipelineThreadedGroup [" << __func__ << "]: Pipeline ID " << pipelineId << " not found for getOutput." << std::endl;
        return {PipelineThreadedGroup::PipelineState::NOT_FOUND, cv::Mat()};
    }

    const PipelineInfo& info = it->second; // Use const reference as we are only reading
    if (info.state == PipelineState::COMPLETED) {
        // Return a clone so the caller gets their own Mat header instance.
        // The data block itself is reference-counted by OpenCV, so this is efficient.
        return {info.state, info.lastRunDirectOutput.clone()};
    }
    // For any other state, the `data` field of PipelineOutput will be an empty cv::Mat by default.
    return {info.state, cv::Mat()};
}

std::shared_ptr<Pipeline> PipelineThreadedGroup::getPipeline(uint16_t pipelineId) {
    std::lock_guard<std::mutex> lock(_mutex); 
    auto it = _pipelines.find(pipelineId);
    if (it != _pipelines.end()) {
        return it->second.ptr;
    }
    return nullptr;
}

void PipelineThreadedGroup::clearAllPipelines() {
    std::lock_guard<std::mutex> lock(_mutex);
    // This is a hard clear. For applications requiring graceful shutdown of active pipelines,
    // a more sophisticated mechanism (e.g., signaling stop and joining threads) would be needed
    // prior to calling this.
    _pipelines.clear();
    _dataLinks.clear();
    _satisfiedLinkInputs.clear();
    // std::cout << "PipelineThreadedGroup [" << __func__ << "]: All pipelines and links cleared." << std::endl;
}

bool PipelineThreadedGroup::clearPipelineCache(uint16_t pipelineId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pipelines.find(pipelineId);
    if (it != _pipelines.end() && it->second.ptr) {
        it->second.ptr->clearCache();
        return true;
    }
    return false;
}

void PipelineThreadedGroup::clearAllPipelineCaches() {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& pair : _pipelines) {
        if (pair.second.ptr) {
            pair.second.ptr->clearCache();
        }
    }
    // std::cout << "PipelineThreadedGroup [" << __func__ << "]: All pipeline caches cleared." << std::endl;
}