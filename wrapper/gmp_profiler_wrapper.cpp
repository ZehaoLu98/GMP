#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>
#include "gmp/profile.h"
#include "gmp/data_struct.h"

namespace py = pybind11;

// Helper class to wrap GmpProfiler as a singleton that can be used from Python
class PyGmpProfiler {
private:
    GmpProfiler* profiler;
    
public:
    PyGmpProfiler() {
        profiler = GmpProfiler::getInstance();
    }
    
    void init() {
        profiler->init();
    }
    
    void enable() {
        profiler->enable();
    }
    
    void disable() {
        profiler->disable();
    }
    
    void start_range_profiling() {
        profiler->startRangeProfiling();
    }
    
    void stop_range_profiling() {
        profiler->stopRangeProfiling();
    }
    
    int push_range(const std::string& name, int profile_type) {
        GmpProfileType type = static_cast<GmpProfileType>(profile_type);
        GmpResult result = profiler->pushRange(name, type);
        return static_cast<int>(result);
    }
    
    int pop_range(const std::string& name, int profile_type) {
        GmpProfileType type = static_cast<GmpProfileType>(profile_type);
        GmpResult result = profiler->popRange(name, type);
        return static_cast<int>(result);
    }
    
    void print_profiler_ranges(int output_reduction_option = 0) {
        GmpOutputKernelReduction option = static_cast<GmpOutputKernelReduction>(output_reduction_option);
        profiler->printProfilerRanges(option);
    }
    
    void print_memory_activity() {
        profiler->printMemoryActivity();
    }
    
    py::list get_memory_activity() {
        auto memory_data = profiler->getMemoryActivity();
        py::list result;
        
        for (const auto& range_data : memory_data) {
            py::dict range_dict;
            range_dict["name"] = range_data.name;
            
            py::list mem_operations;
            for (const auto& mem_data : range_data.memDataInRange) {
                py::dict mem_dict;
                mem_dict["name"] = mem_data.name ? std::string(mem_data.name) : "";
                mem_dict["memory_operation_type"] = static_cast<int>(mem_data.memoryOperationType);
                mem_dict["memory_kind"] = static_cast<int>(mem_data.memoryKind);
                mem_dict["correlation_id"] = mem_data.correlationId;
                mem_dict["address"] = mem_data.address;
                mem_dict["bytes"] = mem_data.bytes;
                mem_dict["timestamp"] = mem_data.timestamp;
                mem_dict["process_id"] = mem_data.processId;
                mem_dict["device_id"] = mem_data.deviceId;
                mem_dict["context_id"] = mem_data.contextId;
                mem_dict["stream_id"] = mem_data.streamId;
                mem_dict["is_async"] = static_cast<bool>(mem_data.isAsync);
                mem_dict["source"] = mem_data.source ? std::string(mem_data.source) : "";
                
                mem_operations.append(mem_dict);
            }
            range_dict["memory_operations"] = mem_operations;
            result.append(range_dict);
        }
        
        return result;
    }
    
    bool is_all_pass_submitted() {
        return profiler->isAllPassSubmitted();
    }
    
    void decode_counter_data() {
        profiler->decodeCounterData();
    }
    
    void add_metrics(const std::string& metric) {
        profiler->addMetrics(metric.c_str());
    }
};

PYBIND11_MODULE(gmp_py_wrapper, m) {
    m.doc() = "GMP Profiler Python Wrapper";
    
    // Enums
    py::enum_<GmpResult>(m, "GmpResult")
        .value("SUCCESS", GmpResult::SUCCESS)
        .value("WARNING", GmpResult::WARNING)
        .value("ERROR", GmpResult::ERROR);
    
    py::enum_<GmpProfileType>(m, "GmpProfileType")
        .value("CONCURRENT_KERNEL", GmpProfileType::CONCURRENT_KERNEL)
        .value("MEMORY", GmpProfileType::MEMORY);
    
    py::enum_<GmpOutputKernelReduction>(m, "GmpOutputKernelReduction")
        .value("SUM", GmpOutputKernelReduction::SUM)
        .value("MAX", GmpOutputKernelReduction::MAX)
        .value("MEAN", GmpOutputKernelReduction::MEAN);
    
    // Memory operation type constants
    m.attr("MEMORY_OP_ALLOCATION") = py::int_(static_cast<int>(CUPTI_ACTIVITY_MEMORY_OPERATION_TYPE_ALLOCATION));
    m.attr("MEMORY_OP_RELEASE") = py::int_(static_cast<int>(CUPTI_ACTIVITY_MEMORY_OPERATION_TYPE_RELEASE));
    
    // Memory kind constants  
    m.attr("MEMORY_KIND_DEVICE") = py::int_(static_cast<int>(CUPTI_ACTIVITY_MEMORY_KIND_DEVICE));
    m.attr("MEMORY_KIND_MANAGED") = py::int_(static_cast<int>(CUPTI_ACTIVITY_MEMORY_KIND_MANAGED));
    m.attr("MEMORY_KIND_PINNED") = py::int_(static_cast<int>(CUPTI_ACTIVITY_MEMORY_KIND_PINNED));
    
    // Main profiler class
    py::class_<PyGmpProfiler>(m, "GmpProfiler")
        .def(py::init<>())
        .def("init", &PyGmpProfiler::init, "Initialize the profiler")
        .def("enable", &PyGmpProfiler::enable, "Enable profiling")
        .def("disable", &PyGmpProfiler::disable, "Disable profiling")
        .def("start_range_profiling", &PyGmpProfiler::start_range_profiling, "Start range profiling")
        .def("stop_range_profiling", &PyGmpProfiler::stop_range_profiling, "Stop range profiling")
        .def("push_range", &PyGmpProfiler::push_range, 
             "Push a profiling range", py::arg("name"), py::arg("profile_type") = 0)
        .def("pop_range", &PyGmpProfiler::pop_range, 
             "Pop a profiling range", py::arg("name"), py::arg("profile_type") = 0)
        .def("print_profiler_ranges", &PyGmpProfiler::print_profiler_ranges, 
             "Print profiler ranges", py::arg("output_reduction_option") = 0)
        .def("print_memory_activity", &PyGmpProfiler::print_memory_activity, 
             "Print memory activity")
        .def("get_memory_activity", &PyGmpProfiler::get_memory_activity, 
             "Get memory activity data as Python list")
        .def("is_all_pass_submitted", &PyGmpProfiler::is_all_pass_submitted, 
             "Check if all passes are submitted")
        .def("decode_counter_data", &PyGmpProfiler::decode_counter_data, 
             "Decode counter data")
        .def("add_metrics", &PyGmpProfiler::add_metrics, 
             "Add metrics for profiling", py::arg("metric"));
}