#pragma once

#include <nn/types.h>

namespace nn::fs {

struct FileHandle {
    u32 data;
};

enum OpenMode {
    OpenMode_Read = 1 << 0,
    OpenMode_Write = 1 << 1,
    OpenMode_Append = 1 << 2,

    OpenMode_ReadWrite = OpenMode_Read | OpenMode_Write
};

Result OpenFile(FileHandle* outValue, const char* path, int mode) noexcept;
void CloseFile(FileHandle handle) noexcept;

Result GetFileSize(s64* outValue, FileHandle handle) noexcept;
Result ReadFile(FileHandle handle, s64 offset, void* buffer, size_t size) noexcept;

// ---- 实机日志落盘所需（与 MHGU 内 nnSdk 导出一致，见 exefs 导入表）----
struct WriteOption {
    u32 flags;

    constexpr WriteOption()
        : flags(0)
    {
    }
};

Result MountSdCard(const char* mountName) noexcept;
Result Unmount(const char* mountName) noexcept;

Result CreateFile(const char* path, s64 size) noexcept;
Result SetFileSize(FileHandle handle, s64 size) noexcept;
Result WriteFile(FileHandle handle, s64 offset, const void* buffer, u32 size, const WriteOption& option) noexcept;

}
