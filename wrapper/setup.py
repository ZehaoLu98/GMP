from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext
from pybind11 import get_cmake_dir
import pybind11
import os
import glob

# Get the directory of this setup.py file
wrapper_dir = os.path.dirname(os.path.abspath(__file__))
gmp_root = os.path.dirname(wrapper_dir)

# Find CUDA paths
cuda_home = os.environ.get('CUDA_HOME', '/usr/local/cuda')
cupti_path = os.path.join(cuda_home, 'extras/CUPTI')

# Find GMP source files
gmp_sources = glob.glob(os.path.join(gmp_root, 'src', '*.cpp'))

# Extension definition
ext_modules = [
    Pybind11Extension(
        "gmp_py_wrapper",
        sources=[
            "gmp_profiler_wrapper.cpp"
        ] + gmp_sources,
        include_dirs=[
            os.path.join(gmp_root, 'include'),
            os.path.join(cupti_path, 'include'),
            os.path.join(cuda_home, 'include'),
        ],
        libraries=['cupti', 'cuda', 'cudart'],
        library_dirs=[
            os.path.join(cupti_path, 'lib64'),
            os.path.join(cupti_path, 'lib'),
            os.path.join(cuda_home, 'lib64'),
            os.path.join(cuda_home, 'lib'),
        ],
        define_macros=[('USE_CUPTI', None)],
        cxx_std=17,
    ),
]

setup(
    name="gmp_py_wrapper",
    version="0.1.0",
    author="GMP Team",
    description="Python wrapper for GMP profiler",
    long_description="GPU Memory and Performance profiler Python wrapper",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    py_modules=["gmp_python"],
    zip_safe=False,
    python_requires=">=3.7",
    install_requires=[
        "pybind11>=2.6.0",
    ],
)