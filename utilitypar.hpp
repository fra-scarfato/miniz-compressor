#include <utility.hpp>
#include <vector>

#include "config.hpp"

/**
 * @struct DataBlock
 * @brief Structure to hold metadata for a block of data during compression/decompression
 * 
 * This structure stores information about each data block processed during
 * parallel compression or decompression operations.
 */
struct DataBlock {
  size_t originalSize;    ///< Original uncompressed size of this block in bytes
  size_t compressedSize;  ///< Size after compression in bytes
  size_t blockIndex;      ///< Sequential index of this block in the complete dataset
};

/**
 * @brief Compresses a single block of data using the compression library
 * 
 * @param[in] inPtr Pointer to the input data block to compress
 * @param[in] inSize Size of the input data in bytes
 * @param[out] ptrOut Pointer to the output buffer for compressed data
 * @param[in,out] cmp_len On input, maximum size of output buffer; on output, actual compressed size
 * @return true if compression succeeded, false otherwise
 */
static bool compressBlock(unsigned char* inPtr, size_t inSize, unsigned char* ptrOut,
                          size_t& cmp_len) {
  if (compress(ptrOut, &cmp_len, inPtr, inSize) != Z_OK) {
    if (QUITE_MODE >= 1)
      std::fprintf(stderr, "Failed to compress block in memory\n");
    return false;
  }
  return true;
}

/**
 * @brief Decompresses a single block of data
 * 
 * @param[in] compressedData Pointer to the compressed data block
 * @param[in] compressedSize Size of the compressed data in bytes
 * @param[out] decompressedData Pointer to the output buffer for decompressed data
 * @param[in,out] decompressedSize On input, maximum size of output buffer; on output, actual decompressed size
 * @return true if decompression succeeded, false otherwise
 */
static bool decompressBlock(unsigned char* compressedData, size_t compressedSize,
                            unsigned char* decompressedData, size_t& decompressedSize) {
  if (uncompress(decompressedData, &decompressedSize, compressedData, compressedSize) != Z_OK) {
    if (QUITE_MODE >= 1)
      std::fprintf(stderr, "uncompress failed for block!\n");
    return false;
  }
  return true;
}

/**
 * @brief Compresses data using either single-block or parallel multi-block approach
 * 
 * For small files (size <= BIG_FILE_SIZE), performs standard single-block compression.
 * For large files, splits the data into multiple blocks and processes them in parallel
 * using OpenMP taskloops.
 * 
 * @param[in] ptr Pointer to the input data to compress
 * @param[in] size Size of the input data in bytes
 * @param[in] fname Base filename to use for the output compressed file
 * @return true if compression succeeded, false otherwise
 */
static bool compressDataParallel(unsigned char* ptr, size_t size, const std::string& fname) {
  // Small file case - use single-block compression approach
  if (size <= BIG_FILE_SIZE) {
    unsigned char* inPtr = ptr;
    size_t inSize = size;
    size_t cmp_len = compressBound(inSize);  // Get upper bound for compressed size
    unsigned char* ptrOut = new unsigned char[cmp_len];

    // Attempt to compress the data block
    if (!compressBlock(inPtr, inSize, ptrOut, cmp_len)) {
      delete[] ptrOut;
      return false;
    }

    // Open output file for writing compressed data
    std::string outfile = fname + SUFFIX;
    std::ofstream outFile(outfile, std::ios::binary);
    if (!outFile.is_open()) {
      std::fprintf(stderr, "Failed to open output file: %s\n", outfile.c_str());
      delete[] ptrOut;
      return false;
    }

    // Write file header with number of blocks (just 1 in this case)
    size_t numBlocks = 1;
    outFile.write(reinterpret_cast<const char*>(&numBlocks), sizeof(numBlocks));

    // Write block metadata
    DataBlock blockInfo;
    blockInfo.originalSize = inSize;
    blockInfo.compressedSize = cmp_len;
    blockInfo.blockIndex = 0;
    outFile.write(reinterpret_cast<const char*>(&blockInfo), sizeof(blockInfo));

    // Write the compressed data
    outFile.write(reinterpret_cast<const char*>(ptrOut), cmp_len);

    delete[] ptrOut;
    outFile.close();
  } else {
    // Large file case - use parallel processing with multiple blocks
    
    // Calculate optimal number of blocks based on available threads
    int num_threads = omp_get_max_threads();
    size_t numBlocks = (size + BIG_FILE_SIZE - 1) / BIG_FILE_SIZE;  // Ceiling division
    // Limit blocks to a reasonable multiple of thread count
    numBlocks = std::min(numBlocks, static_cast<size_t>(num_threads * 2));

    size_t blockSize = (size + numBlocks - 1) / numBlocks;  // Ceiling division for even block sizes

    // Create containers for block information and compressed data
    std::vector<DataBlock> blockInfos(numBlocks);
    std::vector<std::vector<unsigned char>> compressedBlocks(numBlocks);

    bool compressionSuccess = true;

    // Process blocks in parallel using OpenMP taskloop
    #pragma omp parallel
    {
      // nowait because threads can instantly take a task when it is scheduled 
      // instead of wait until all the task are created 
      #pragma omp single nowait
      {
        #pragma omp taskloop shared(blockInfos, compressedBlocks, compressionSuccess) grainsize(1)
        for (size_t i = 0; i < numBlocks; i++) {
          // Calculate offset and size for current block
          size_t offset = i * blockSize;
          // Calculate the block size
          // If this is the last block, the block size is computed with size - offset
          size_t currentBlockSize = (i < numBlocks - 1) ? blockSize : (size - offset);

          // Initialize block metadata
          blockInfos[i].originalSize = currentBlockSize;
          blockInfos[i].blockIndex = i;

          // Allocate space for compressed data (using upper bound)
          size_t cmp_bound = compressBound(currentBlockSize);
          compressedBlocks[i].resize(cmp_bound);

          // Compress this block
          size_t compressedSize = cmp_bound;
          bool success = compressBlock(ptr + offset, currentBlockSize, compressedBlocks[i].data(),
                                       compressedSize);

          // Handle compression failure
          if (!success) {
            #pragma omp critical
            { 
              compressionSuccess = false; 
            }
            continue;
          }

          // Update the compressed size in block info
          blockInfos[i].compressedSize = compressedSize;

          // Resize the compressed block vector to its actual size (optimization)
          compressedBlocks[i].resize(compressedSize);
        }
      }
    }

    // If any block failed to compress, abort the operation
    if (!compressionSuccess) {
      return false;
    }

    // Write all compressed blocks to output file
    std::string outfile = fname + SUFFIX;
    std::ofstream outFile(outfile, std::ios::binary);
    if (!outFile.is_open()) {
      std::fprintf(stderr, "Failed to open output file: %s\n", outfile.c_str());
      return false;
    }

    // Write file header with number of blocks
    outFile.write(reinterpret_cast<const char*>(&numBlocks), sizeof(numBlocks));

    // Write all block metadata first
    for (size_t i = 0; i < numBlocks; i++) {
      outFile.write(reinterpret_cast<const char*>(&blockInfos[i]), sizeof(DataBlock));
    }

    // Then write all compressed data blocks
    for (size_t i = 0; i < numBlocks; i++) {
      outFile.write(reinterpret_cast<const char*>(compressedBlocks[i].data()),
                    blockInfos[i].compressedSize);
    }

    outFile.close();
  }

  // Optionally remove the original file if configured to do so
  if (REMOVE_ORIGIN) {
    unlink(fname.c_str());
  }

  return true;
}

/**
 * @brief Decompresses data from a memory-mapped compressed file
 * 
 * Handles both single-block and multi-block compressed files.
 * For multi-block files, decompression is performed in parallel using OpenMP tasks.
 * 
 * @param[in] ptr Pointer to the memory-mapped compressed data
 * @param[in] fname Filename of the compressed file (used to derive output filename)
 * @return true if decompression succeeded, false otherwise
 */
static bool decompressDataParallel(unsigned char* ptr, const std::string& fname) {
  // Read number of blocks from file header
  size_t numBlocks = *reinterpret_cast<size_t*>(ptr);
  ptr += sizeof(size_t);

  // Single-block case
  if (numBlocks == 1) {
    // Read block metadata
    DataBlock blockInfo = *reinterpret_cast<DataBlock*>(ptr);
    ptr += sizeof(DataBlock);

    // Get the original size for allocation
    size_t decompressedSize = blockInfo.originalSize;

    // Prepare output file
    unsigned char* decompressed_data = nullptr;
    std::string outfile = fname.substr(0, fname.size() - strlen(SUFFIX));

    // Allocate space in output file using memory mapping
    if (!allocateFile(outfile.c_str(), decompressedSize, decompressed_data))
      return false;

    // Decompress the data
    if (!decompressBlock(ptr, blockInfo.compressedSize, decompressed_data, decompressedSize)) {
      unmapFile(decompressed_data, decompressedSize);
      return false;
    }

    // Flush to disk and close the file
    unmapFile(decompressed_data, decompressedSize);
  } else {
    // Multi-block case
    std::vector<DataBlock> blockInfos(numBlocks);

    // Read all block metadata
    for (size_t i = 0; i < numBlocks; i++) {
      blockInfos[i] = *reinterpret_cast<DataBlock*>(ptr);
      ptr += sizeof(DataBlock);
    }

    // Calculate total decompressed size by summing all block sizes
    size_t totalSize = 0;
    for (const auto& block : blockInfos) {
      totalSize += block.originalSize;
    }

    // Allocate the output file
    unsigned char* decompressed_data = nullptr;
    std::string outfile = fname.substr(0, fname.size() - strlen(SUFFIX));

    if (!allocateFile(outfile.c_str(), totalSize, decompressed_data))
      return false;

    // Store pointers to the start of each compressed block
    std::vector<unsigned char*> compressedBlocks(numBlocks);
    for (size_t i = 0; i < numBlocks; i++) {
      compressedBlocks[i] = ptr;
      if (i < numBlocks - 1) {  // Don't advance after the last block
        ptr += blockInfos[i].compressedSize;
      }
    }

    bool decompressionSuccess = true;

    // Decompress blocks in parallel using OpenMP tasks
    #pragma omp parallel
    {
      #pragma omp single nowait
      {
        #pragma omp taskloop shared(blockInfos, compressedBlocks, decompressionSuccess, decompressed_data) grainsize(1)
        for (size_t i = 0; i < numBlocks; i++) {
          // Calculate the output position for this block
          size_t outputOffset = 0;
          for (size_t j = 0; j < i; j++) {
            outputOffset += blockInfos[j].originalSize;
          }

          // Decompress this block directly into the appropriate position in the output buffer
          size_t currentDecompressedSize = blockInfos[i].originalSize;
          bool success = decompressBlock(compressedBlocks[i], blockInfos[i].compressedSize,
                                         decompressed_data + outputOffset, currentDecompressedSize);

          // Handle decompression failure
          if (!success) {
            #pragma omp critical
            { 
                decompressionSuccess = false;
            }
          }
        }
      }
    }

    // Flush to disk and close the file
    unmapFile(decompressed_data, totalSize);

    if (!decompressionSuccess) {
      return false;
    }
  }

  // Optionally remove the compressed file if configured to do so
  if (REMOVE_ORIGIN) {
    unlink(fname.c_str());
  }

  return true;
}

/**
 * @brief Entry point for compression and decompression operations
 * 
 * Maps the file into memory for efficient processing, then calls the appropriate
 * function for either compression or decompression.
 * 
 * @param[in] fname Path to the file to process
 * @param[in] size Size of the file in bytes
 * @param[in] comp Flag indicating whether to compress (true) or decompress (false)
 * @return true if operation succeeded, false otherwise
 */
static inline bool doParallelWork(const char fname[], size_t size, const bool comp) {
  unsigned char* ptr = nullptr;

  // Memory-map the file for efficient access
  if (!mapFile(fname, size, ptr))
    return false;

  // Perform compression or decompression
  bool r = (comp) ? compressDataParallel(ptr, size, fname) : decompressDataParallel(ptr, fname);

  // Unmap the file
  unmapFile(ptr, size);
  return r;
}

/**
 * @brief Recursively processes all files in a directory and its subdirectories
 * 
 * Traverses the directory structure, collecting files to process and handling
 * them in parallel batches for each directory level.
 * 
 * @param[in] dname Path to the directory to process
 * @param[in] comp Flag indicating whether to compress (true) or decompress (false)
 * @return true if all operations succeeded, false if any operation failed
 */
static inline bool walkDirParallel(const char dname[], const bool comp) {
  // Change to target directory
  if (chdir(dname) == -1) {
    if (QUITE_MODE >= 1) {
      perror("chdir");
      std::fprintf(stderr, "Error: chdir %s\n", dname);
    }
    return false;
  }

  // Open directory for reading
  DIR* dir;
  if ((dir = opendir(".")) == NULL) {
    if (QUITE_MODE >= 1) {
      perror("opendir");
      std::fprintf(stderr, "Error: opendir %s\n", dname);
    }
    return false;
  }

  struct dirent* file;
  // Store files to process in a vector for parallel processing
  std::vector<std::pair<std::string, size_t>> files;

  // First pass: collect all files and recursively process subdirectories
  while ((errno = 0, file = readdir(dir)) != NULL) {
    struct stat statbuf;
    if (stat(file->d_name, &statbuf) == -1) {
      if (QUITE_MODE >= 1) {
        perror("stat");
        std::fprintf(stderr, "Error: stat %s\n", file->d_name);
      }
      return false;
    }

    if (S_ISDIR(statbuf.st_mode)) {
      // Handle subdirectories (excluding "." and "..")
      if (!isdot(file->d_name)) {
        if (walkDirParallel(file->d_name, comp)) {
          // Return to parent directory after processing subdirectory
          if (chdir("..") == -1) {
            perror("chdir");
            std::fprintf(stderr, "Error: chdir ..\n");
            return false;
          }
        } else {
          return false;
        }
      }
    } else {
      // Handle regular files, skip those with wrong extensions
      if (discardIt(file->d_name, comp)) {
        if (QUITE_MODE >= 2) {
          if (comp) {
            std::fprintf(stderr, "%s has already a %s suffix -- ignored\n", file->d_name, SUFFIX);
          } else {
            std::fprintf(stderr, "%s does not have a %s suffix -- ignored\n", file->d_name, SUFFIX);
          }
        }
        continue;
      }
      // Add valid file to processing queue
      files.emplace_back(std::pair(file->d_name, statbuf.st_size));
    }
  }

  // Check for directory reading errors
  if (errno != 0) {
    if (QUITE_MODE >= 1)
      perror("readdir");
    return false;
  }
  closedir(dir);

  // Second pass: process collected files in parallel
  bool success = true;
  #pragma omp parallel for schedule(dynamic) shared(success)
  for (int i = 0; i < (int)files.size(); ++i) {
    if (!doParallelWork(files[i].first.c_str(), files[i].second, comp))
    #pragma omp critical
    {
      success = false;
    }
  }
  return success;
}
