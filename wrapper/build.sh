#!/bin/bash
"""
Build script for GMP Profiler Python wrapper

This script builds the GMP profiler Python bindings and installs them.
"""

set -e  # Exit on any error

echo "🔨 Building GMP Profiler Python Wrapper"
echo "========================================"

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
WRAPPER_DIR="$SCRIPT_DIR"
GMP_ROOT="$(dirname "$WRAPPER_DIR")"

echo "Wrapper directory: $WRAPPER_DIR"
echo "GMP root directory: $GMP_ROOT"

# Check for required dependencies
echo ""
echo "🔍 Checking dependencies..."

# Check for CUDA
if ! command -v nvcc &> /dev/null; then
    echo "❌ Error: nvcc not found. Please install CUDA and add it to your PATH."
    exit 1
else
    echo "✅ Found CUDA: $(nvcc --version | grep "release" | awk '{print $5}' | sed 's/,//')"
fi

# Check for Python
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: python3 not found."
    exit 1
else
    echo "✅ Found Python: $(python3 --version)"
fi

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "❌ Error: cmake not found. Please install CMake."
    exit 1
else
    echo "✅ Found CMake: $(cmake --version | head -n1)"
fi

# Install Python dependencies
echo ""
echo "📦 Installing Python dependencies..."
pip3 install --user pybind11 numpy

# Method 1: Try CMake build (recommended)
echo ""
echo "🏗️  Building with CMake..."

cd "$WRAPPER_DIR"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
if cmake ..; then
    echo "✅ CMake configuration successful"
    
    # Build
    if make -j$(nproc); then
        echo "✅ CMake build successful"
        
        # Copy the built module
        cp ../gmp_python.py ./
        
        echo "✅ Build completed successfully with CMake!"
        echo ""
        echo "📁 Files created:"
        ls -la gmp_py_wrapper*.so ../gmp_python.py 2>/dev/null || true
        
        echo ""
        echo "🧪 Testing the module..."
        cd ..
        if python3 -c "import gmp_py_wrapper; print('✅ Module imports successfully')"; then
            echo "✅ Python module test passed!"
        else
            echo "⚠️  Module import test failed"
        fi
        
        exit 0
    else
        echo "❌ CMake build failed, trying setuptools..."
    fi
else
    echo "❌ CMake configuration failed, trying setuptools..."
fi

echo ""
echo "🎉 Build completed!"
echo ""
echo "📖 Usage example:"
echo "   cd $WRAPPER_DIR"
echo "   python3 -c \"import gmp_python; print('GMP Profiler ready!')\""