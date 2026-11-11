#include "forge/logsd.h"

#ifndef FORGE_SD_LOG
#define FORGE_SD_LOG 1
#endif

#if FORGE_SD_LOG

#include "forge/nn/fs.h"

#include <switch.h>

// Writes the forge log to the SD card root as forge-loader.log (mount name forge_log:).
// Any failure simply disables file logging; the game is never aborted from here.

namespace {

constexpr const char* kMountName = "forge_log";
constexpr const char* kLogPath = "forge_log:/forge-loader.log";

bool s_disabled = false;
bool s_opened = false;
int s_lock = 0;
int s_mountRetries = 0;
nn::fs::FileHandle s_file = {};
s64 s_offset = 0;

void openLogLocked()
{
    if (s_disabled) {
        return;
    }

    if (nn::fs::MountSdCard(kMountName).IsFailure()) {
        // fs service may not be ready during early init; retry a bounded number of times.
        if (++s_mountRetries >= 64) {
            s_disabled = true;
        }
        return;
    }

    // Fails when the file already exists; that is fine.
    (void)nn::fs::CreateFile(kLogPath, 0);

    if (nn::fs::OpenFile(&s_file, kLogPath, nn::fs::OpenMode_ReadWrite).IsFailure()) {
        s_disabled = true;
        return;
    }

    if (nn::fs::GetFileSize(&s_offset, s_file).IsFailure()) {
        s_offset = 0;
    }

    s_opened = true;
}

void writeLocked(const char* text, size_t len)
{
    if (!s_opened) {
        openLogLocked();
        if (!s_opened) {
            return;
        }
    }

    if (len > 0x1000) {
        len = 0x1000;
    }

    nn::fs::WriteOption option {};
    if (nn::fs::WriteFile(s_file, s_offset, text, static_cast<unsigned int>(len), option).IsFailure()) {
        s_opened = false;
        s_disabled = true;
        return;
    }

    s_offset += static_cast<s64>(len);
}

} // namespace

extern "C" void forge_logsd_write(const char* text, size_t len)
{
    if (text == nullptr || len == 0) {
        return;
    }

    while (__sync_lock_test_and_set(&s_lock, 1) != 0) {
        /* spin */
    }

    writeLocked(text, len);

    __sync_lock_release(&s_lock);
}

#else

// Build with -DFORGE_SD_LOG=0 to remove every nn::fs call from the module.
extern "C" void forge_logsd_write(const char* text, size_t len)
{
    (void)text;
    (void)len;
}

#endif
