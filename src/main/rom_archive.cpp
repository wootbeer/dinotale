// Reading the game out of a .zip or .7z archive, for the launcher's "select
// game" file picker. Registered with librecomp (recomp::register_rom_archive_reader),
// which calls it when the picked file isn't a ROM itself and then checks the
// result exactly like a picked ROM.
//
// .zip uses miniz (MIT, part of N64ModernRuntime); .7z uses the LZMA SDK's 7z
// decoder (public domain, thirdparty/lzma-sdk). The first file inside whose
// header is an N64 ROM's, in any byte order, is used.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "miniz.h"

extern "C" {
#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
}

namespace {
    // Larger files aren't N64 ROMs (the biggest cartridges are 64 MB).
    constexpr size_t max_rom_size = size_t(64) * 1024 * 1024;

    bool looks_like_rom(const uint8_t* data, size_t size) {
        if (size < 0x40) {
            return false;
        }
        static const uint8_t starts[3][4] = {
            { 0x80, 0x37, 0x12, 0x40 }, // .z64
            { 0x37, 0x80, 0x40, 0x12 }, // .v64
            { 0x40, 0x12, 0x37, 0x80 }, // .n64
        };
        for (const auto& start : starts) {
            if (std::memcmp(data, start, sizeof(start)) == 0) {
                return true;
            }
        }
        return false;
    }

    std::vector<uint8_t> read_zip(const std::vector<uint8_t>& file_data) {
        std::vector<uint8_t> rom;
        mz_zip_archive zip{};
        if (!mz_zip_reader_init_mem(&zip, file_data.data(), file_data.size(), 0)) {
            return rom;
        }

        const mz_uint count = mz_zip_reader_get_num_files(&zip);
        for (mz_uint i = 0; (i < count) && rom.empty(); i++) {
            mz_zip_archive_file_stat stat;
            if (!mz_zip_reader_file_stat(&zip, i, &stat) || stat.m_is_directory || (stat.m_uncomp_size > max_rom_size)) {
                continue;
            }

            size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
            if (data == nullptr) {
                continue;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            if (looks_like_rom(bytes, size)) {
                rom.assign(bytes, bytes + size);
            }
            mz_free(data);
        }

        mz_zip_reader_end(&zip);
        return rom;
    }

    // The archive in memory, as the LZMA SDK's seekable input stream.
    struct MemoryInStream {
        ISeekInStream vt;
        const uint8_t* data;
        size_t size;
        size_t pos;
    };

    SRes memory_read(ISeekInStreamPtr p, void* buf, size_t* size) {
        MemoryInStream* stream = (MemoryInStream*)p;
        const size_t count = std::min(*size, stream->size - stream->pos);
        std::memcpy(buf, stream->data + stream->pos, count);
        stream->pos += count;
        *size = count;
        return SZ_OK;
    }

    SRes memory_seek(ISeekInStreamPtr p, Int64* pos, ESzSeek origin) {
        MemoryInStream* stream = (MemoryInStream*)p;
        Int64 base = 0;
        switch (origin) {
            case SZ_SEEK_SET: base = 0; break;
            case SZ_SEEK_CUR: base = Int64(stream->pos); break;
            case SZ_SEEK_END: base = Int64(stream->size); break;
            default: return SZ_ERROR_PARAM;
        }
        const Int64 target = base + *pos;
        if ((target < 0) || (target > Int64(stream->size))) {
            return SZ_ERROR_PARAM;
        }
        stream->pos = size_t(target);
        *pos = target;
        return SZ_OK;
    }

    std::vector<uint8_t> read_7z(const std::vector<uint8_t>& file_data) {
        std::vector<uint8_t> rom;
        ISzAlloc alloc = { SzAlloc, SzFree };
        ISzAlloc alloc_temp = { SzAllocTemp, SzFreeTemp };

        MemoryInStream archive{};
        archive.vt.Read = memory_read;
        archive.vt.Seek = memory_seek;
        archive.data = file_data.data();
        archive.size = file_data.size();

        constexpr size_t look_buffer_size = size_t(1) << 18;
        CLookToRead2 look{};
        LookToRead2_CreateVTable(&look, False);
        look.buf = (Byte*)ISzAlloc_Alloc(&alloc, look_buffer_size);
        if (look.buf == nullptr) {
            return rom;
        }
        look.bufSize = look_buffer_size;
        look.realStream = &archive.vt;
        LookToRead2_INIT(&look)

        CrcGenerateTable();

        CSzArEx db;
        SzArEx_Init(&db);
        if (SzArEx_Open(&db, &look.vt, &alloc, &alloc_temp) == SZ_OK) {
            // Kept across files: each solid block is decoded once.
            UInt32 block_index = 0xFFFFFFFF;
            Byte* out_buffer = nullptr;
            size_t out_buffer_size = 0;
            for (UInt32 i = 0; (i < db.NumFiles) && rom.empty(); i++) {
                if (SzArEx_IsDir(&db, i) || (SzArEx_GetFileSize(&db, i) > max_rom_size)) {
                    continue;
                }

                size_t offset = 0;
                size_t size = 0;
                if (SzArEx_Extract(&db, &look.vt, i, &block_index, &out_buffer, &out_buffer_size, &offset, &size, &alloc, &alloc_temp) != SZ_OK) {
                    break;
                }
                if (looks_like_rom(out_buffer + offset, size)) {
                    rom.assign(out_buffer + offset, out_buffer + offset + size);
                }
            }
            ISzAlloc_Free(&alloc, out_buffer);
        }
        SzArEx_Free(&db, &alloc);
        ISzAlloc_Free(&alloc, look.buf);
        return rom;
    }
}

std::vector<uint8_t> dinotale_read_rom_archive(const std::vector<uint8_t>& file_data) {
    static const uint8_t zip_start[4] = { 'P', 'K', 0x03, 0x04 };
    static const uint8_t sevenzip_start[6] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
    if ((file_data.size() >= sizeof(zip_start)) && (std::memcmp(file_data.data(), zip_start, sizeof(zip_start)) == 0)) {
        return read_zip(file_data);
    }
    if ((file_data.size() >= sizeof(sevenzip_start)) && (std::memcmp(file_data.data(), sevenzip_start, sizeof(sevenzip_start)) == 0)) {
        return read_7z(file_data);
    }
    return {};
}
