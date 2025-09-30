#!/usr/bin/env python3
"""
Script to extract individual kernel metrics from NVIDIA profiler log files.
Takes a log file and one or more target metrics as input, extracts the metric values 
for each individual kernel and outputs to a CSV file.

Usage: python extract_kernel_metrics.py <log_file> <metric1> [metric2] [metric3] ... [--output output_file]
"""

import sys
import re
import csv
from typing import List, Dict, Set

def parse_log_file_individual(log_file_path: str, target_metrics: List[str]) -> List[Dict]:
    """
    Parse the log file and extract the target metrics for each individual kernel.
    
    Args:
        log_file_path: Path to the log file
        target_metrics: List of metric names to extract (e.g., ['gpu__time_duration.sum', 'sm__cycles_active.max'])
    
    Returns:
        List of dictionaries containing individual kernel data
    """
    kernel_data = []
    current_range = None
    current_kernel = None
    current_kernel_metrics = {}
    in_kernel_section = False
    kernel_counter = 0
    
    # Convert target_metrics to a set for faster lookup
    target_metrics_set = set(target_metrics)
    
    with open(log_file_path, 'r') as file:
        for line_num, line in enumerate(file, 1):
            line = line.strip()
            
            # Check for range name
            if line.startswith("Range Name:"):
                current_range = line.replace("Range Name:", "").strip()
                continue
            
            # Check for kernel line (starts with "Kernel:")
            if line.startswith("Kernel:"):
                current_kernel = line.replace("Kernel:", "").strip()
                kernel_counter += 1
                current_kernel_metrics = {}
                continue
            
            # Check for separator lines
            if line.startswith("======================================================================================"):
                in_kernel_section = False
                current_kernel = None
                continue
            
            if line.startswith("-----------------------------------------------------------------------------------"):
                if current_kernel and not in_kernel_section:
                    # This marks the start of metrics for the current kernel
                    in_kernel_section = True
                elif current_kernel and in_kernel_section:
                    # End of current kernel, save data if any target metrics were found
                    if current_range and current_kernel and current_kernel_metrics:
                        # Check if we have any of the target metrics
                        found_metrics = {k: v for k, v in current_kernel_metrics.items() if k in target_metrics_set}
                        if found_metrics:
                            # Truncate kernel name for better readability
                            kernel_display = current_kernel
                            if len(kernel_display) > 150:
                                kernel_display = kernel_display[:150] + "..."
                            
                            kernel_data.append({
                                'kernel_id': kernel_counter,
                                'range_name': current_range,
                                'kernel_name': kernel_display,
                                'metrics': found_metrics,
                                'line_number': line_num
                            })
                    
                    in_kernel_section = False
                    current_kernel_metrics = {}
                continue
            
            # Parse metric lines when in kernel section
            if in_kernel_section and current_range and current_kernel:
                # Look for lines with metric values (format: metric_name    value)
                if re.match(r'^[a-zA-Z_][a-zA-Z0-9_.]*\s+[\d.]+$', line):
                    parts = line.split()
                    if len(parts) >= 2:
                        metric_name = parts[0]
                        metric_value = parts[-1]
                        
                        # Store all metrics, we'll filter later
                        current_kernel_metrics[metric_name] = float(metric_value)
    
    # Handle the last kernel if it exists
    if current_range and current_kernel and current_kernel_metrics:
        found_metrics = {k: v for k, v in current_kernel_metrics.items() if k in target_metrics_set}
        if found_metrics:
            kernel_display = current_kernel
            if len(kernel_display) > 150:
                kernel_display = kernel_display[:150] + "..."
            
            kernel_data.append({
                'kernel_id': kernel_counter,
                'range_name': current_range,
                'kernel_name': kernel_display,
                'metrics': found_metrics,
                'line_number': line_num
            })
    
    return kernel_data

def write_individual_csv(data: List[Dict], output_file: str, target_metrics: List[str]):
    """
    Write the individual kernel data to a CSV file.
    
    Args:
        data: List of dictionaries containing the extracted data
        output_file: Path to the output CSV file
        target_metrics: List of target metric names
    """
    if not data:
        print(f"No data found for metrics {target_metrics}")
        return
    
    # Write CSV file
    with open(output_file, 'w', newline='', encoding='utf-8') as csvfile:
        fieldnames = ['kernel_id', 'range_name', 'kernel_name'] + target_metrics
        
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        
        # Write data for each kernel
        for kernel in data:
            row = {
                'kernel_id': kernel['kernel_id'],
                'range_name': kernel['range_name'],
                'kernel_name': kernel['kernel_name']
            }
            
            # Add metric values, using empty string for missing metrics
            for metric in target_metrics:
                value = kernel['metrics'].get(metric, '')
                row[metric] = f"{value:.3f}" if value != '' else ''
            
            writer.writerow(row)
    
    print(f"Individual kernel data successfully written to {output_file}")
    print(f"Total kernels with any of the target metrics: {len(data)}")
    
    # Print summary by range for each metric
    for metric in target_metrics:
        range_summary = {}
        total_kernels_with_metric = 0
        
        for kernel in data:
            if metric in kernel['metrics']:
                range_name = kernel['range_name']
                if range_name not in range_summary:
                    range_summary[range_name] = []
                range_summary[range_name].append(kernel['metrics'][metric])
                total_kernels_with_metric += 1
        
        if range_summary:
            print(f"\nSummary for '{metric}' by range:")
            print(f"Total kernels with this metric: {total_kernels_with_metric}")
            print(f"{'Range Name':<25} {'Count':<8} {'Min':<12} {'Max':<12} {'Avg':<12}")
            print("-" * 75)
            for range_name, values in range_summary.items():
                count = len(values)
                min_val = min(values)
                max_val = max(values)
                avg_val = sum(values) / count
                print(f"{range_name:<25} {count:<8} {min_val:<12.3f} {max_val:<12.3f} {avg_val:<12.3f}")

def main():
    """Main function to handle command line arguments and orchestrate the extraction."""
    if len(sys.argv) < 3:
        print("Usage: python extract_kernel_metrics.py <log_file> <metric1> [metric2] [metric3] ... [--output output_file]")
        print("\nThis script extracts individual kernel metrics (not accumulated by range).")
        print("You can specify multiple metrics to extract them all in one CSV with additional columns.")
        print("\nExample metrics from the log file:")
        print("  - gpu__time_duration.sum")
        print("  - gpu__time_duration.max")
        print("  - gpc__cycles_elapsed.avg.per_second")
        print("  - gpc__cycles_elapsed.max")
        print("  - sm__cycles_active.max")
        print("\nExamples:")
        print("  python extract_kernel_metrics.py log.txt gpu__time_duration.sum")
        print("  python extract_kernel_metrics.py log.txt gpu__time_duration.sum sm__cycles_active.max")
        print("  python extract_kernel_metrics.py log.txt gpu__time_duration.sum sm__cycles_active.max --output results.csv")
        sys.exit(1)
    
    # Parse command line arguments
    args = sys.argv[1:]
    log_file = args[0]
    
    # Check for --output flag
    output_file = None
    if '--output' in args:
        output_index = args.index('--output')
        if output_index + 1 < len(args):
            output_file = args[output_index + 1]
            # Remove --output and filename from args
            args = args[:output_index] + args[output_index + 2:]
        else:
            print("Error: --output flag requires a filename")
            sys.exit(1)
    
    # Remaining args are metrics
    target_metrics = args[1:]  # Skip log_file
    
    if not target_metrics:
        print("Error: At least one metric must be specified")
        sys.exit(1)
    
    # Generate default output filename if not specified
    if not output_file:
        metrics_str = "_".join([m.replace('.', '_') for m in target_metrics])
        output_file = f"individual_{metrics_str}.csv"
    
    print(f"Extracting individual kernel values for metrics {target_metrics} from '{log_file}'...")
    
    try:
        # Extract data
        data = parse_log_file_individual(log_file, target_metrics)
        
        if not data:
            print(f"No instances of metrics {target_metrics} found in the log file.")
            print("Please check the metric names and ensure they exist in the log file.")
            return
        
        # Write to CSV
        write_individual_csv(data, output_file, target_metrics)
        
    except FileNotFoundError:
        print(f"Error: File '{log_file}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f"Error processing file: {str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    main()