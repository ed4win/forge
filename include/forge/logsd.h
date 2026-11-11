#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 把一行日志追加写入 SD 卡根目录的 forge-loader.log（挂载点 forge_log:）。
 * 挂载/打开失败时静默降级，只走 svcOutputDebugString，不影响游戏。 */
void forge_logsd_write(const char* text, size_t len);

#ifdef __cplusplus
}
#endif
