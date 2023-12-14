// Copyright 2022-2023 by Rightware. All rights reserved.

#include "decompressor.h"
#include "lz4frame_static.h"
#include "zlib.h"

#if defined (__cplusplus)
extern "C" {
#endif

    int DecompressBufferLZ4(size_t compressedSize, void* compressedBuffer, size_t decompressedSize,
                            void* decompressedBuffer_o)
    {
        size_t ret;
        LZ4F_dctx* dctx;

        ret = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
        if (LZ4F_isError(ret)) {
            return LZ4F_getErrorCode(ret);
        }

        ret = LZ4F_decompress(dctx, decompressedBuffer_o, &decompressedSize, compressedBuffer,
                              &compressedSize, NULL);
        if (LZ4F_isError(ret)) {
            LZ4F_freeDecompressionContext(dctx);
            return LZ4F_getErrorCode(ret);
        }

        LZ4F_freeDecompressionContext(dctx);

        return 0;
    }


    int DecompressBufferZLIB(size_t compressedSize, void* compressedBuffer, size_t decompressedSize,
                             void* decompressedBuffer_o)
    {

        int ret;
        z_stream strm;

        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;

        strm.avail_in = compressedSize;
        strm.next_in = reinterpret_cast<Bytef*>(compressedBuffer);

        strm.avail_out = decompressedSize;
        strm.next_out = reinterpret_cast<Bytef*>(decompressedBuffer_o);

        ret = inflateInit(&strm);
        if (ret != Z_OK) {
            return ret;
        }

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            return ret;
        }

        inflateEnd(&strm);

        return 0;
    }

#if defined (__cplusplus)
}
#endif
