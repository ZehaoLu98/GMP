# Metrics Extraction Scripts

This repository contains Python scripts to extract and analyze NVIDIA profiler metrics from log files containing "Range Name:" sections. You can extract either **accumulated metrics per range** or **individual kernel metrics**.

## Scripts

### 1. `extract_metrics.py` - Single Metric Extraction (Range Summary)

Extracts a specific metric across all ranges and outputs a summary CSV with accumulated values per range.

**Usage:**
```bash
python3 extract_metrics.py <log_file> <metric_name> [output_file]
```

**Example:**
```bash
python3 extract_metrics.py logs/a100_eigen_forward_log.txt gpu__time_duration.sum
python3 extract_metrics.py logs/a100_eigen_forward_log.txt sm__cycles_active.max custom_output.csv
```

**Output:** CSV file with columns:
- `range_name`: Name of the range
- `num_kernels`: Number of kernels in this range
- `total_<metric>`: Sum of the metric across all kernels in the range
- `avg_<metric>`: Average value of the metric across kernels in the range
- `kernel_names`: First few kernel names (truncated for readability)

### 2. `extract_kernel_metrics.py` - Single/Multiple Metric Extraction (Individual Kernels)

Extracts one or more metrics for each individual kernel (not accumulated by range). **NEW: Now supports multiple metrics in additional columns!**

**Usage:**
```bash
python3 extract_kernel_metrics.py <log_file> <metric1> [metric2] [metric3] ... [--output output_file]
```

**Examples:**
```bash
# Single metric
python3 extract_kernel_metrics.py logs/a100_eigen_forward_log.txt gpu__time_duration.sum

# Multiple metrics in additional columns
python3 extract_kernel_metrics.py logs/a100_eigen_forward_log.txt gpu__time_duration.sum sm__cycles_active.max

# Multiple metrics with custom output filename
python3 extract_kernel_metrics.py logs/a100_eigen_forward_log.txt gpu__time_duration.sum sm__cycles_active.max gpc__cycles_elapsed.max --output my_analysis.csv
```

**Output:** CSV file with columns:
- `kernel_id`: Sequential kernel ID (maintains launch order)
- `range_name`: Name of the range this kernel belongs to
- `kernel_name`: Kernel name (truncated for readability)
- `<metric1>`: Individual value of the first metric for this kernel
- `<metric2>`: Individual value of the second metric for this kernel (if specified)
- `<metric3>`: Individual value of the third metric for this kernel (if specified)
- ... (additional columns for each metric specified)

### 3. `extract_all_metrics.py` - Comprehensive Extraction (Range Summary)

Extracts all metrics for all ranges, creating detailed and summary CSV files with accumulated values.

**Usage:**
```bash
python3 extract_all_metrics.py <log_file> [output_file]
```

**Example:**
```bash
python3 extract_all_metrics.py logs/a100_eigen_forward_log.txt
python3 extract_all_metrics.py logs/a100_eigen_forward_log.txt detailed_analysis.csv
```

**Output:** Two CSV files:
1. **Detailed CSV** (`<output_file>`): One row per kernel with all its metrics
2. **Summary CSV** (`<output_file>_summary.csv`): Aggregated metrics per range (sum, max, min, avg)

### 4. `extract_all_kernel_metrics.py` - Comprehensive Extraction (Individual Kernels)

Extracts all metrics for each individual kernel, creating a comprehensive CSV with one row per kernel.

**Usage:**
```bash
python3 extract_all_kernel_metrics.py <log_file> [output_file]
```

**Example:**
```bash
python3 extract_all_kernel_metrics.py logs/a100_eigen_forward_log.txt all_individual_kernels.csv
```

**Output:** CSV file with one row per kernel showing all its metric values.

## Script Comparison

| Feature | Range Summary | Individual Kernels |
|---------|---------------|-------------------|
| **Single Metric** | `extract_metrics.py` | `extract_kernel_metrics.py` |
| **All Metrics** | `extract_all_metrics.py` | `extract_all_kernel_metrics.py` |
| **Output** | Accumulated values per range | Individual kernel values |
| **Use Case** | Range-level analysis | Kernel-level analysis |
| **File Size** | Smaller (summarized) | Larger (detailed) |

## Examples

### Range-Level Analysis (Accumulated Values):
```bash
# Get total GPU time per range
python3 extract_metrics.py logs/a100_eigen_forward_log.txt gpu__time_duration.sum
# Output: LN1_1 has total 734912.000 across 3 kernels (avg: 244970.667)
```

### Kernel-Level Analysis (Individual Values):
```bash
# Get GPU time for each individual kernel
python3 extract_kernel_metrics.py logs/a100_eigen_forward_log.txt gpu__time_duration.sum
# Output: Kernel 1 has 186656.000, Kernel 2 has 536768.000, etc.
```

## Available Metrics

Based on the analyzed log file, the following metrics are available:

- `gpu__time_duration.sum` / `gpu__time_duration.max`
- `gpc__cycles_elapsed.avg.per_second` / `gpc__cycles_elapsed.max`
- `sm__cycles_active.max`
- `dram__sectors_read.sum` / `dram__sectors_write.sum`
- `dram__throughput.avg.pct_of_peak_sustained_elapsed`
- Various L1TEX, LTS, and SM pipe metrics
- Memory operation metrics (ld/st operations)
- And many more (41 unique metrics total)

## Output Examples

### Range Summary Output (`gpu__time_duration.sum`):
| range_name | num_kernels | total_gpu__time_duration.sum | avg_gpu__time_duration.sum | kernel_names |
|------------|-------------|-------------------------------|----------------------------|--------------|
| LN1_1 | 3 | 734912.000 | 244970.667 | _ZN5Eigen8internal15EigenMetaKernelI... |
| Attention_1 | 440 | 2937120.000 | 6675.273 | _ZN5Eigen32EigenFloatContractionKernel... |

### Individual Kernel Output (Multiple Metrics):
| kernel_id | range_name | kernel_name | gpu__time_duration.sum | sm__cycles_active.max | gpc__cycles_elapsed.max |
|-----------|------------|-------------|------------------------|----------------------|-------------------------|
| 1 | LN1_1 | _ZN5Eigen8internal15EigenMetaKernelI... | 186656.000 | 202406.000 | 205431.000 |
| 2 | LN1_1 | _ZN5Eigen8internal15EigenMetaKernelI... | 536768.000 | 585733.000 | 590096.000 |
| 3 | LN1_1 | _ZN5Eigen8internal15EigenMetaKernelI... | 11488.000 | 10474.000 | 12507.000 |

## File Format

The scripts expect log files with the following structure:

```
Range Name: <range_name>
======================================================================================
Kernel: <kernel_name>
-----------------------------------------------------------------------------------
<metric_name>                                                     <metric_value>
<metric_name>                                                     <metric_value>
...
-----------------------------------------------------------------------------------
```

## Examples

### Extract individual kernel values for multiple metrics:
```bash
python3 extract_kernel_metrics.py logs/a100_eigen_forward_log.txt gpu__time_duration.sum sm__cycles_active.max
```

### Extract individual kernel values for three metrics with custom output:
```bash
python3 extract_kernel_metrics.py logs/a100_eigen_forward_log.txt gpu__time_duration.sum sm__cycles_active.max gpc__cycles_elapsed.max --output my_results.csv
```

### Extract all metrics for individual kernels:
```bash
python3 extract_all_kernel_metrics.py logs/a100_eigen_forward_log.txt comprehensive_kernels.csv
```

### Extract range summaries (accumulated values):
```bash
python3 extract_all_metrics.py logs/a100_eigen_forward_log.txt comprehensive_analysis.csv
```

### View available metrics first:
```bash
python3 extract_all_metrics.py logs/a100_eigen_forward_log.txt | grep "Metrics found:"
```

## Requirements

- Python 3.6+
- No external dependencies (uses only standard library)

## Performance

- Processes large log files efficiently
- The test file (47K+ lines) with 8K+ kernel instances processes in seconds
- Memory usage scales linearly with file size