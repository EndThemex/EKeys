/*
 * Upgrade.h
 *
 * OTA 升级（FEATURE_DOC §17，阶段 07 任务 7.3）。
 *
 * 流程（docs/07 7.3 / 验收标准）：
 *   1. App 通过 CMD_FIRMWARE_INFO (0x0b) 下发 data.url + data.checksum
 *      （固件 MD5，32 位十六进制；必须提供，否则拒绝升级）；
 *   2. cmd handler 回成功响应后调用 performOta()（MainTask 上下文阻塞）；
 *   3. 下载流式写入 ota_1 分区，边写边计算 MD5，写完先校验再 finalize——
 *      校验失败 abort() 不覆盖当前固件（docs/07 备注）；
 *   4. 校验通过 → 自动重启进入新固件。
 */

#ifndef EKEYS_UPGRADE_UPGRADE_H
#define EKEYS_UPGRADE_UPGRADE_H

#include <stddef.h>

namespace ekeys {

class Upgrade {
public:
    static Upgrade &instance();

    Upgrade(const Upgrade &) = delete;
    Upgrade &operator=(const Upgrade &) = delete;

    /* 记录升级参数（不立即执行） */
    void requestStart(const char *url, const char *checksum);

    /* 阻塞执行：下载 + MD5 校验 + 重启。失败仅日志，不影响当前固件 */
    void performOta();

    bool pending() const { return pending_; }

private:
    Upgrade() = default;

    char url_[256] = {0};
    char checksum_[33] = {0}; // MD5 hex + '\0'
    bool pending_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_UPGRADE_UPGRADE_H
