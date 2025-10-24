"""
GMP Python Interface

This module provides a Python interface to the GMP.

Example usage:
    ```python
    import gmp_py_wrapper
    
    profiler = gmp_py_wrapper.GmpProfiler()
    profiler.init()
    
    # Profile a range of operations
    with gmp_py_wrapper.profile_range(profiler, "my_kernels"):
        # Your CUDA/PyTorch operations here
        pass
    
    # Print results
    profiler.print_profiler_ranges()
    ```
"""

try:
    print("Importing gmp_py_wrapper")  # Debug line to check import
    import gmp_py_wrapper  # The compiled C++ module
    print("Imported gmp_py_wrapper successfully")  # Debug line to confirm success
except ImportError as e:
    print(f"Warning: Could not import gmp_py_wrapper module: {e}")
    print("Make sure the GMP Python wrapper is compiled and installed.")
    exit(1)

from typing import Optional, Dict, List, Any, Union
import warnings


class ProfilerError(Exception):
    """Exception raised for profiler-related errors."""
    pass


class GmpProfiler:
    """
    This class provides a Pythonic interface to the underlying C++ GMP profiler,
    with additional error handling and convenience methods.
    """
    
    def __init__(self):
        if gmp_py_wrapper is None:
            raise ImportError("GMP Python wrapper module is not available")
        
        self._profiler = gmp_py_wrapper.GmpProfiler()
        self._initialized = False
        self._enabled = False
    
    def init(self) -> None:
        """
        Initialize the profiler.
        
        This must be called before any kernel launches to ensure proper profiling.
        """
        try:
            self._profiler.init()
            self._initialized = True
            self._enabled = True
        except Exception as e:
            raise ProfilerError(f"Failed to initialize profiler: {e}")
    
    def enable(self) -> None:
        """Enable profiling."""
        if not self._initialized:
            warnings.warn("Profiler not initialized. Call init() first.")
        self._profiler.enable()
        self._enabled = True
    
    def disable(self) -> None:
        """Disable profiling."""
        self._profiler.disable()
        self._enabled = False
    
    def is_enabled(self) -> bool:
        """Check if profiler is enabled."""
        return self._enabled and self._initialized
    
    def start_range_profiling(self) -> None:
        """Start range profiling (delegates to underlying C++ profiler)."""
        if not self._initialized:
            warnings.warn("Profiler not initialized. Call init() first.")
        self._profiler.start_range_profiling()

    def stop_range_profiling(self) -> None:
        """Stop range profiling (delegates to underlying C++ profiler)."""
        if not self._initialized:
            return
        self._profiler.stop_range_profiling()

    def print_profiler_ranges(self, reduction: Union[str, int] = "SUM") -> None:
        """
        Print profiling results for all ranges.
        
        Args:
            reduction: Reduction method for results ("SUM", "AVG", etc., or corresponding int)
        """
        if isinstance(reduction, str):
            reduction_map = {
                "SUM": 0,
                "MAX": 1,
                "MEAN": 2,
            }
            reduction = reduction_map.get(reduction.upper(), 0)
        
        self._profiler.print_profiler_ranges(reduction)
    
    def push_range(self, name: str, profile_type: Union[str, int] = "CONCURRENT_KERNEL") -> None:
        """
        Push a profiling range.
        
        Args:
            name: Name of the range
            profile_type: Type of profiling ("CONCURRENT_KERNEL" or "MEMORY", or corresponding int)
        """
        if not self.is_enabled():
            return
        
        # Convert string profile type to int
        if isinstance(profile_type, str):
            type_map = {
                "CONCURRENT_KERNEL": 0,
                "MEMORY": 1
            }
            profile_type = type_map.get(profile_type.upper(), 0)
        
        result = self._profiler.push_range(name, profile_type)
        if result != 0:  # Not SUCCESS
            raise ProfilerError(f"Failed to push range '{name}': result={result}")
            
    def pop_range(self, name: str, profile_type: Union[str, int] = "CONCURRENT_KERNEL") -> None:
        """
        Pop a profiling range.
        
        Args:
            name: Name of the range (should match the last pushed range)
            profile_type: Type of profiling
        """
        if not self.is_enabled():
            return
        
        # Convert string profile type to int
        if isinstance(profile_type, str):
            type_map = {
                "CONCURRENT_KERNEL": 0,
                "MEMORY": 1
            }
            profile_type = type_map.get(profile_type.upper(), 0)
        
        result = self._profiler.pop_range(name, profile_type)
        if result != 0:  # Not SUCCESS
            raise ProfilerError(f"Failed to pop range '{name}': result={result}")
    
    def print_memory_activity(self) -> None:
        """Print detailed memory activity report."""
        self._profiler.print_memory_activity()
    
    def get_memory_activity(self) -> List[Dict[str, Any]]:
        """
        Get memory activity data as Python structures.
        
        Returns:
            List of dictionaries containing memory activity data for each range
        """
        if not self.is_enabled():
            return []
        return self._profiler.get_memory_activity()
    
    def add_metrics(self, metric: str) -> None:
        """
        Add metrics for profiling.
        
        Args:
            metric: Name of the metric to add
        """
        self._profiler.add_metrics(metric)
    
    def decode_counter_data(self) -> None:
        """Decode counter data."""
        if self.is_enabled():
            self._profiler.decode_counter_data()
    
    def is_all_pass_submitted(self) -> bool:
        """Check if all passes are submitted."""
        if not self.is_enabled():
            return True
        return self._profiler.is_all_pass_submitted()

__all__ = [
    'GmpProfiler'
]