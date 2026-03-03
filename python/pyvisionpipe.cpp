/**
 * @file pyvisionpipe.cpp
 * @brief Python bindings for libvisionpipe using pybind11
 *
 * Provides the `visionpipe` Python module with:
 *   - VisionPipeConfig  (dataclass-like config)
 *   - SessionState       (enum)
 *   - Session            (running pipeline session, frame access)
 *   - VisionPipe         (main entry point to launch pipelines)
 *
 * cv::Mat frames are exposed as numpy arrays (zero-copy when possible).
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

#include "libvisionpipe/libvisionpipe.h"

#include <opencv2/core/mat.hpp>
#include <stdexcept>

namespace py = pybind11;

// ============================================================================
// cv::Mat <-> numpy ndarray conversion helpers
// ============================================================================

/**
 * Convert a cv::Mat to a numpy array.
 *
 * For continuous Mats this is zero-copy (numpy references the Mat's data,
 * kept alive by capturing a shared_ptr to a Mat clone in the capsule).
 * For non-continuous Mats we copy first.
 */
static py::array mat_to_numpy(const cv::Mat& mat) {
    if (mat.empty()) {
        return py::array();
    }

    // Determine numpy dtype from cv depth
    py::dtype dtype;
    switch (mat.depth()) {
        case CV_8U:  dtype = py::dtype::of<uint8_t>();  break;
        case CV_8S:  dtype = py::dtype::of<int8_t>();   break;
        case CV_16U: dtype = py::dtype::of<uint16_t>(); break;
        case CV_16S: dtype = py::dtype::of<int16_t>();  break;
        case CV_32S: dtype = py::dtype::of<int32_t>();  break;
        case CV_32F: dtype = py::dtype::of<float>();    break;
        case CV_64F: dtype = py::dtype::of<double>();   break;
        default:
            throw std::runtime_error("Unsupported cv::Mat depth for numpy conversion");
    }

    int channels = mat.channels();

    // Build shape and strides
    std::vector<py::ssize_t> shape;
    std::vector<py::ssize_t> strides;

    size_t elemSize = mat.elemSize1(); // bytes per channel element

    if (channels == 1) {
        shape   = { mat.rows, mat.cols };
        strides = { static_cast<py::ssize_t>(mat.step[0]),
                    static_cast<py::ssize_t>(elemSize) };
    } else {
        shape   = { mat.rows, mat.cols, channels };
        strides = { static_cast<py::ssize_t>(mat.step[0]),
                    static_cast<py::ssize_t>(channels * elemSize),
                    static_cast<py::ssize_t>(elemSize) };
    }

    // Keep the Mat data alive by capturing a shared_ptr to a clone
    auto mat_ptr = std::make_shared<cv::Mat>(mat.clone());
    auto destructor = [](void* ptr) {
        delete static_cast<std::shared_ptr<cv::Mat>*>(ptr);
    };
    // Store the shared_ptr on the heap so the capsule prevents premature GC
    auto* guard = new std::shared_ptr<cv::Mat>(mat_ptr);
    py::capsule capsule(static_cast<void*>(guard), destructor);

    return py::array(dtype, shape, strides, mat_ptr->data, capsule);
}

/**
 * Convert a numpy array to cv::Mat.
 * Reserved for future use (e.g., passing frames from Python to VisionPipe).
 */
[[maybe_unused]]
static cv::Mat numpy_to_mat(const py::array& arr) {
    if (arr.size() == 0) {
        return cv::Mat();
    }

    // Determine CV type from numpy dtype
    int depth;
    if      (arr.dtype().is(py::dtype::of<uint8_t>()))  depth = CV_8U;
    else if (arr.dtype().is(py::dtype::of<int8_t>()))   depth = CV_8S;
    else if (arr.dtype().is(py::dtype::of<uint16_t>())) depth = CV_16U;
    else if (arr.dtype().is(py::dtype::of<int16_t>()))  depth = CV_16S;
    else if (arr.dtype().is(py::dtype::of<int32_t>()))  depth = CV_32S;
    else if (arr.dtype().is(py::dtype::of<float>()))    depth = CV_32F;
    else if (arr.dtype().is(py::dtype::of<double>()))   depth = CV_64F;
    else throw std::runtime_error("Unsupported numpy dtype for cv::Mat conversion");

    int ndim = arr.ndim();
    int rows = static_cast<int>(arr.shape(0));
    int cols = (ndim >= 2) ? static_cast<int>(arr.shape(1)) : 1;
    int channels = (ndim >= 3) ? static_cast<int>(arr.shape(2)) : 1;

    int type = CV_MAKETYPE(depth, channels);

    // Create Mat referencing numpy data (no copy)
    return cv::Mat(rows, cols, type, const_cast<void*>(arr.data()),
                   static_cast<size_t>(arr.strides(0)));
}

// ============================================================================
// Module definition
// ============================================================================

PYBIND11_MODULE(pyvisionpipe, m) {
    m.doc() = R"pbdoc(
        VisionPipe Python Bindings
        --------------------------

        Python interface for libvisionpipe - launch VisionPipe pipelines
        and receive frames as numpy arrays via shared memory.

        Quick start::

            import visionpipe

            vp = visionpipe.VisionPipe()
            session = vp.run("my_pipeline.vsp")

            while session.is_running():
                ok, frame = session.grab_frame("output")
                if ok:
                    # frame is a numpy array (H, W, C)
                    print(frame.shape)

            session.stop()
    )pbdoc";

    // ========================================================================
    // SessionState enum
    // ========================================================================
    py::enum_<visionpipe::SessionState>(m, "SessionState",
        "State of a VisionPipe session")
        .value("CREATED",  visionpipe::SessionState::CREATED,  "Session created, not yet started")
        .value("STARTING", visionpipe::SessionState::STARTING, "VisionPipe process is launching")
        .value("RUNNING",  visionpipe::SessionState::RUNNING,  "VisionPipe is running")
        .value("STOPPING", visionpipe::SessionState::STOPPING, "Stop requested, waiting for exit")
        .value("STOPPED",  visionpipe::SessionState::STOPPED,  "Process exited normally")
        .value("ERROR",    visionpipe::SessionState::ERROR,    "Process exited with error")
        .export_values();

    // ========================================================================
    // VisionPipeConfig
    // ========================================================================
    py::class_<visionpipe::VisionPipeConfig>(m, "VisionPipeConfig",
        "Configuration for VisionPipe execution")
        .def(py::init<>())
        .def_readwrite("executable_path", &visionpipe::VisionPipeConfig::executablePath,
            "Path to visionpipe binary (default: 'visionpipe')")
        .def_readwrite("verbose", &visionpipe::VisionPipeConfig::verbose,
            "Pass --verbose to visionpipe")
        .def_readwrite("no_display", &visionpipe::VisionPipeConfig::noDisplay,
            "Pass --no-display (default: True)")
        .def_readwrite("params", &visionpipe::VisionPipeConfig::params,
            "Script parameters dict (passed as --param key=value)")
        .def_readwrite("pipeline", &visionpipe::VisionPipeConfig::pipeline,
            "Specific pipeline to run (--pipeline name)")
        .def_readwrite("sink_timeout_ms", &visionpipe::VisionPipeConfig::sinkTimeoutMs,
            "Timeout (ms) waiting for frame_sink shm to appear (default: 10000)")
        .def("__repr__", [](const visionpipe::VisionPipeConfig& c) {
            return "<VisionPipeConfig executable='" + c.executablePath +
                   "' verbose=" + (c.verbose ? "True" : "False") +
                   " no_display=" + (c.noDisplay ? "True" : "False") + ">";
        });

    // ========================================================================
    // Session
    // ========================================================================
    py::class_<visionpipe::Session>(m, "Session", R"pbdoc(
        A running VisionPipe session.

        Provides access to frames produced by frame_sink() items in .vsp scripts.
        Frames are returned as numpy arrays.
    )pbdoc")
        // grab_frame: returns (bool, numpy_array)
        .def("grab_frame", [](visionpipe::Session& self, const std::string& sinkName) -> py::tuple {
            cv::Mat frame;
            bool ok = self.grabFrame(sinkName, frame);
            if (ok && !frame.empty()) {
                // Clone so the numpy array owns its data (not tied to shm lifecycle)
                return py::make_tuple(true, mat_to_numpy(frame));
            }
            return py::make_tuple(false, py::none());
        }, py::arg("sink_name"),
        R"pbdoc(
            Grab the latest frame from a named sink (polling mode).

            Args:
                sink_name: Name matching a frame_sink("name") in the .vsp script

            Returns:
                Tuple of (success: bool, frame: numpy.ndarray or None)
        )pbdoc")

        // on_frame: register a Python callback
        .def("on_frame", [](visionpipe::Session& self, const std::string& sinkName,
                            py::function callback) {
            self.onFrame(sinkName, [callback](const cv::Mat& frame, uint64_t seq) {
                py::gil_scoped_acquire acquire;
                py::array np_frame = mat_to_numpy(frame);
                callback(np_frame, seq);
            });
        }, py::arg("sink_name"), py::arg("callback"),
        R"pbdoc(
            Register a callback for frames from a named sink.

            The callback signature: callback(frame: numpy.ndarray, seq: int)

            Args:
                sink_name: Name matching a frame_sink("name") in the .vsp script
                callback: Python callable(frame, seq)
        )pbdoc")

        // spin: blocking dispatch loop (release GIL so other threads can run)
        .def("spin", [](visionpipe::Session& self, int pollIntervalMs) {
            py::gil_scoped_release release;
            self.spin(pollIntervalMs);
        }, py::arg("poll_interval_ms") = 1,
        "Block and dispatch frame callbacks until session ends.")

        // spin_once: non-blocking dispatch
        .def("spin_once", &visionpipe::Session::spinOnce,
            "Dispatch pending frames once (non-blocking). Returns number dispatched.")

        // stop
        .def("stop", [](visionpipe::Session& self, int timeoutMs) {
            py::gil_scoped_release release;
            self.stop(timeoutMs);
        }, py::arg("timeout_ms") = 3000,
        "Stop the VisionPipe process.")

        // is_running
        .def("is_running", &visionpipe::Session::isRunning,
            "Check if visionpipe process is still running.")

        // state
        .def_property_readonly("state", &visionpipe::Session::state,
            "Current session state (SessionState enum).")

        // session_id
        .def_property_readonly("session_id", &visionpipe::Session::sessionId,
            "Unique session identifier.")

        // exit_code
        .def_property_readonly("exit_code", &visionpipe::Session::exitCode,
            "Exit code of the visionpipe process (-1 if still running).")

        // wait_for_exit
        .def("wait_for_exit", [](visionpipe::Session& self, int timeoutMs) -> bool {
            py::gil_scoped_release release;
            return self.waitForExit(timeoutMs);
        }, py::arg("timeout_ms") = 0,
        "Wait for the process to exit. Returns True if exited within timeout.")

        // Context manager support
        .def("__enter__", [](visionpipe::Session& self) -> visionpipe::Session& {
            return self;
        })
        .def("__exit__", [](visionpipe::Session& self,
                            const py::object&, const py::object&, const py::object&) {
            py::gil_scoped_release release;
            self.stop(3000);
        })

        // ----------------------------------------------------------------
        // Runtime parameter API
        // ----------------------------------------------------------------
        .def("set_param", [](visionpipe::Session& self,
                              const std::string& name, const std::string& value) -> bool {
            return self.setParam(name, value);
        }, py::arg("name"), py::arg("value"),
        R"pbdoc(
            Set a runtime parameter in the running pipeline.

            Args:
                name:  Parameter name (must be declared in the .vsp with ``params``)
                value: String representation of the new value

            Returns:
                True on success, False if the param server is not yet ready.
        )pbdoc")

        .def("get_param", [](visionpipe::Session& self,
                              const std::string& name) -> py::object {
            std::string value;
            if (self.getParam(name, value)) return py::str(value);
            return py::none();
        }, py::arg("name"),
        "Get a runtime parameter value as a string, or None if unavailable.")

        .def("list_params", [](visionpipe::Session& self) {
            return self.listParams();
        }, "Return a dict of all declared runtime parameters and their current values.")

        .def("watch_param", [](visionpipe::Session& self,
                                const std::string& name, py::function cb) -> uint64_t {
            return self.watchParam(name,
                [cb](const std::string& n, const std::string& v) {
                    py::gil_scoped_acquire acquire;
                    cb(n, v);
                });
        }, py::arg("name"), py::arg("callback"),
        R"pbdoc(
            Subscribe to changes of a runtime parameter.

            The callback is called on a background thread whenever the parameter
            value changes inside the pipeline.

            Args:
                name:     Parameter name (or ``"*"`` to watch all).
                callback: Callable with signature ``(name: str, value: str) -> None``.

            Returns:
                Subscription ID (pass to ``unwatch_param`` to cancel).
        )pbdoc")

        .def("unwatch_param", &visionpipe::Session::unwatchParam,
            py::arg("subscription_id"),
            "Cancel a param watch subscription by its ID.")

        .def_property_readonly("param_server_port", &visionpipe::Session::paramServerPort,
            "TCP port of the pipeline's param server (0 if not yet ready).")

        .def("__repr__", [](const visionpipe::Session& self) {
            std::string stateStr;
            switch (self.state()) {
                case visionpipe::SessionState::CREATED:  stateStr = "CREATED";  break;
                case visionpipe::SessionState::STARTING: stateStr = "STARTING"; break;
                case visionpipe::SessionState::RUNNING:  stateStr = "RUNNING";  break;
                case visionpipe::SessionState::STOPPING: stateStr = "STOPPING"; break;
                case visionpipe::SessionState::STOPPED:  stateStr = "STOPPED";  break;
                case visionpipe::SessionState::ERROR:    stateStr = "ERROR";    break;
            }
            return "<Session id='" + self.sessionId() + "' state=" + stateStr + ">";
        });

    // ========================================================================
    // VisionPipe (main entry point)
    // ========================================================================
    py::class_<visionpipe::VisionPipe>(m, "VisionPipe", R"pbdoc(
        Main VisionPipe client class.

        Launches VisionPipe pipelines and returns Session objects
        for accessing frames via shared memory.

        Example::

            import visionpipe

            vp = visionpipe.VisionPipe()
            session = vp.run("camera_pipeline.vsp")

            while session.is_running():
                ok, frame = session.grab_frame("output")
                if ok:
                    print(f"Got frame: {frame.shape}")

            session.stop()
    )pbdoc")
        .def(py::init<>(), "Create a VisionPipe instance with default config.")
        .def(py::init<visionpipe::VisionPipeConfig>(), py::arg("config"),
            "Create a VisionPipe instance with custom config.")

        .def("set_executable_path", &visionpipe::VisionPipe::setExecutablePath,
            py::arg("path"),
            "Set the path to the visionpipe executable.")

        .def("set_param", &visionpipe::VisionPipe::setParam,
            py::arg("key"), py::arg("value"),
            "Set a script parameter (passed as --param key=value).")

        .def("set_verbose", &visionpipe::VisionPipe::setVerbose,
            py::arg("verbose"),
            "Enable/disable verbose mode.")

        .def("set_no_display", &visionpipe::VisionPipe::setNoDisplay,
            py::arg("no_display"),
            "Enable/disable display (default: disabled for library usage).")

        .def_property_readonly("config", &visionpipe::VisionPipe::config,
            "Current VisionPipeConfig (read-only).")

        // run: returns a Session (transfer ownership to Python)
        .def("run", [](visionpipe::VisionPipe& self, const std::string& scriptPath)
            -> std::unique_ptr<visionpipe::Session> {
            auto session = self.run(scriptPath);
            if (!session) {
                throw std::runtime_error("Failed to start VisionPipe session for: " + scriptPath);
            }
            return session;
        }, py::arg("script_path"),
        R"pbdoc(
            Run a .vsp script file and return a Session.

            Args:
                script_path: Path to the .vsp file

            Returns:
                Session object

            Raises:
                RuntimeError: If the session failed to launch
        )pbdoc")

        // run_string: run script from string
        .def("run_string", [](visionpipe::VisionPipe& self,
                              const std::string& script, const std::string& name)
            -> std::unique_ptr<visionpipe::Session> {
            auto session = self.runString(script, name);
            if (!session) {
                throw std::runtime_error("Failed to start VisionPipe session from string");
            }
            return session;
        }, py::arg("script"), py::arg("name") = "inline",
        R"pbdoc(
            Run a VisionPipe script from a string.

            The script is written to a temporary file and executed.
            The temp file is cleaned up when the Session is destroyed.

            Args:
                script: The .vsp script content as a string
                name: Optional name for logging (default: "inline")

            Returns:
                Session object

            Raises:
                RuntimeError: If the session failed to launch
        )pbdoc")

        // execute: synchronous command execution
        .def("execute", [](visionpipe::VisionPipe& self,
                          const std::vector<std::string>& args) -> int {
            py::gil_scoped_release release;
            return self.execute(args);
        }, py::arg("args"),
        "Execute a visionpipe command synchronously. Returns exit code.")

        // is_available
        .def("is_available", &visionpipe::VisionPipe::isAvailable,
            "Check if the visionpipe executable is available in PATH.")

        // version
        .def("version", &visionpipe::VisionPipe::version,
            "Get the version string of the visionpipe executable.")

        .def("__repr__", [](const visionpipe::VisionPipe& self) {
            return "<VisionPipe executable='" + self.config().executablePath + "'>";
        });
}
