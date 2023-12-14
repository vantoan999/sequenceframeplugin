// Copyright 2022-2023 by Rightware. All rights reserved.

#ifndef PLUGIN_SRC_DECOMPRESSOR_H_
#define PLUGIN_SRC_DECOMPRESSOR_H_

#include "stddef.h"

#if defined (__cplusplus)
extern "C" {
#endif
    // Decompress the LZ4 format data from buffer
    int DecompressBufferLZ4(size_t compressedSize, void* compressedBuffer, size_t decompressedSize,
                            void* decompressedBuffer_o);

    // Decompress the ZLIB format data from buffer
    int DecompressBufferZLIB(size_t compressedSize, void* compressedBuffer, size_t decompressedSize,
                             void* decompressedBuffer_o);

#if defined (__cplusplus)
}
#endif

#endif // PLUGIN_SRC_DECOMPRESSOR_H_
