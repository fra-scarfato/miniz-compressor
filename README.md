# Parallel File Compression with Miniz and OpenMP

## Overview

This project implements a high-performance parallel file compression system using the Miniz library and OpenMP. The implementation provides efficient compression for both small and large files through adaptive processing strategies, maintaining the integrity of the DEFLATE compression algorithm while maximizing parallelization opportunities.

## Architecture

### Sequential vs Parallel Processing

The implementation uses a dual-strategy approach:

1. **Small Files (< threshold)**: Processed independently using OpenMP parallel sections
2. **Large Files (≥ threshold)**: Split into configurable blocks and compressed in parallel

### Block-Based Compression for Large Files

Large files are divided into blocks to enable parallel processing while preserving the DEFLATE compression context:

```
Original File: [Block 1][Block 2][Block 3][Block 4]
                  ↓       ↓       ↓       ↓
Parallel:     Thread1  Thread2  Thread3  Thread4
                  ↓       ↓       ↓       ↓
Compressed:   [CBlock1][CBlock2][CBlock3][CBlock4]
```

Each compressed block includes:
- Block size information
- Compressed data
- Context preservation metadata

### Compression Algorithm

The implementation uses the DEFLATE algorithm through Miniz with the following optimizations:

- **Adaptive Compression Levels**: Adjusts compression level based on file characteristics
- **Buffer Management**: Optimizes I/O operations for both small and large files
- **Error Handling**: Comprehensive error checking and recovery mechanisms

## Usage

### Basic Usage

```bash
./parallel_compressor [options] <files/directories>
```

### Command Line Options

- `-r <0|1>`: Recursive directory processing (default: 0)
- `-C <0|1>`: Preserve original files (0) or delete them (1) (default: 0)
- `-b <size>`: Block size for large file processing in KB (default: 1024)
- `-t <threads>`: Number of threads to use (default: auto-detect)
- `-s <size>`: Small file threshold in KB (default: 512)

### Examples

```bash
# Compress all files in directory with subdirectories
./parallel_compressor -r 1 -C 0 /path/to/directory

# Compress specific files with custom block size
./parallel_compressor -b 2048 file1.dat file2.dat

# Compress with specific thread count
./parallel_compressor -t 8 -r 1 /data/directory
```

## Report
A report with the implementation details and results can be found [here](miniz-report.pdf).
