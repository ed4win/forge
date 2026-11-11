#include "forge/logsd.h"
#include "forge/nn/fs.h"

#include <switch.h>

/* 实机日志落盘：挂 SD 卡为 forge_log:，把日志追加到 SD 根目录 forge-loader.log。
 * 任何一步失败都整体降级（只保留 svcOutputDebugString），不抛异常、不 abort。 */

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

    if (R_FAILED(nn::fs::MountSdCard(kMountName))) {
        // 早期调用时 fs 服务可能还没就绪，允许重试；超过阈值才放弃。
        if (++s_mountRetries >= 64) {
            s_disabled = true;
        }
        return;
    }

    // 文件已存在时 CreateFile 会返回错误，忽略即可。
    (void)nn::fs::CreateFile(kLogPath, 0);

    if (R_FAILED(nn::fs::OpenFile(&s_file, kLogPath, nn::fs::OpenMode_ReadWrite))) {
        s_disabled = true;
        return;
    }

    if (R_FAILED(nn::fs::GetFileSize(&s_offset, s_file))) {
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
    if (R_FAILED(nn::fs::WriteFile(s_file, s_offset, text, (u32)len, option))) {
        s_opened = false;
        s_disabled = true;
        return;
    }

    s_offset += (s64)len;
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
