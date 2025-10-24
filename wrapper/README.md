# GMP Profiler Python Wrapper

This directory contains the Python wrapper for the GMP (GPU Memory and Performance) profiler, which allows you to profile CUDA kernels and memory operations from Python.

## Overview

The GMP profiler Python wrapper provides:

- **Kernel Profiling**: Profile CUDA kernel execution times and performance metrics
- **Memory Profiling**: Monitor GPU memory allocations and deallocations  
- **Range-based Profiling**: Profile specific ranges of operations with nested support
- **PyTorch Integration**: Easy integration with PyTorch and other CUDA-based libraries
- **Context Manager Support**: Clean profiling code using Python context managers
- **Function Decorators**: Profile individual functions with decorators

## Files Structure

```
wrapper/
├── gmp_profiler_wrapper.cpp    # C++ pybind11 wrapper implementation
├── gmp_python.py               # Python interface module
├── CMakeLists.txt              # CMake build configuration
├── setup.py                    # Python setuptools configuration
├── build.sh                    # Build script
└── README.md                   # This file
```

## Requirements

- **CUDA**: CUDA 11.0 or higher with CUPTI
- **Python**: Python 3.7 or higher
- **CMake**: CMake 3.16 or higher
- **pybind11**: For Python bindings
- **PyTorch**: Optional, for PyTorch integration examples

## Installation

### Recommended: pip install (Easiest)

The simplest way to install the GMP profiler wrapper:

```bash
cd wrapper/
pip install -e .
```

This will automatically:
- Install required dependencies (pybind11, etc.)
- Compile the C++ extension with proper CUDA/CUPTI linking
- Install the module to your Python environment
- Enable editable development mode

### Verify Installation

```bash
python3 -c "import gmp_py_wrapper; import gmp_python; print('✅ GMP Profiler installed successfully!')"
```

## Alternative Build Methods

### Quick Build Script

If you prefer manual control over the build process:

```bash
cd wrapper/
./build.sh
```

### Manual Build with CMake

For development or debugging:

```bash
cd wrapper/
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Manual Build with setuptools

```bash
cd wrapper/
python3 setup.py build_ext --inplace
```

## Usage Examples

### Basic Usage

```python
import gmp_python

# Create and initialize profiler
profiler = gmp_python.create_profiler()
profiler.init()

# Profile a range of operations
with profiler.profile_range("my_operations"):
    # Your CUDA operations here
    import torch
    x = torch.randn(1000, 1000, device='cuda')
    y = torch.randn(1000, 1000, device='cuda') 
    z = torch.matmul(x, y)

# Print results
profiler.print_profiler_ranges()
```

### Memory Profiling

```python
import gmp_python

profiler = gmp_python.create_profiler()
profiler.init()

# Profile memory operations
with profiler.profile_memory("memory_ops"):
    # Operations that allocate/deallocate memory
    large_tensor = torch.randn(5000, 5000, device='cuda')
    del large_tensor
    torch.cuda.empty_cache()

# Print memory activity
profiler.print_memory_activity()
```

### Function Decorator

```python
import gmp_python

profiler = gmp_python.create_profiler()
profiler.init()

@profiler.profile_function("matrix_multiply")
def my_function():
    x = torch.randn(1000, 1000, device='cuda')
    y = torch.randn(1000, 1000, device='cuda')
    return torch.matmul(x, y)

result = my_function()
profiler.print_profiler_ranges()
```

### Global Profiler (Convenience)

```python
import gmp_python

# Initialize global profiler
gmp_python.init_global_profiler()

# Use global convenience functions
with gmp_python.profile_range("operations"):
    # Your operations here
    pass

# Print results
gmp_python.print_results()
```

## API Reference

### GmpProfiler Class

- `init()`: Initialize the profiler
- `enable()` / `disable()`: Enable/disable profiling
- `profile_range(name, type)`: Context manager for profiling ranges
- `profile_memory(name)`: Context manager for memory profiling  
- `profile_function(name)`: Decorator for function profiling
- `print_profiler_ranges(reduction)`: Print kernel profiling results
- `print_memory_activity()`: Print memory profiling results
- `get_memory_activity()`: Get memory data as Python structures

### Profile Types

- `"CONCURRENT_KERNEL"` (default): Profile CUDA kernel execution
- `"MEMORY"`: Profile memory operations

### Reduction Options

- `"SUM"` (default): Sum metrics across kernels
- `"MAX"`: Maximum values across kernels  
- `"MEAN"`: Average values across kernels

## Integration with PyTorch

The wrapper integrates seamlessly with PyTorch. See `../hello_pytorch_with_gmp.py` for a complete example that profiles:

- Matrix operations (GEMM kernels)
- Element-wise operations (activations)
- Memory allocations/deallocations
- Neural network forward passes
- Convolution and pooling operations
- Reduction operations

## Output

The profiler generates:

1. **Console Output**: Human-readable profiling results
2. **CSV Files**: Detailed metrics in `output/result.csv`
3. **Python Data**: Memory activity data accessible via API

## Troubleshooting

### Common Issues

1. **Import Error**: Make sure the module is built and in your Python path
2. **CUDA Not Found**: Ensure CUDA is installed and `nvcc` is in your PATH
3. **CUPTI Not Found**: CUPTI should be at `/usr/local/cuda/extras/CUPTI`
4. **Permission Errors**: Try building in a directory with write permissions

### Environment Variables

Set these if needed:

```bash
export CUDA_HOME=/usr/local/cuda
export PATH=$CUDA_HOME/bin:$PATH
export LD_LIBRARY_PATH=$CUDA_HOME/lib64:$LD_LIBRARY_PATH
```

### Debug Build

For debugging, build with debug flags:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

## Performance Notes

- Initialize the profiler before any CUDA operations
- Profiling adds overhead - disable for production use
- Memory profiling captures allocations/deallocations
- Range profiling works best with well-defined operation boundaries

## Example Output

```
🔥 Kernel Profiling Results (SUM):
Number of ranges: 3
Range 1: matrix_operations
  gpu__time_duration.sum: 1250.45
  smsp__inst_executed.sum: 2048576
  ...

💾 Memory Activity Results:
Range 1: memory_ops
  Allocations: 5 operations, 104857600 bytes (100.00 MB)
  Deallocations: 5 operations, 104857600 bytes (100.00 MB)
```

## License

This wrapper follows the same license as the main GMP profiler project.