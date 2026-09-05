/*
 * ConfigStore.cpp
 *
 * SPIFFS 挂载与 SimpleIni 文件读写。
 */

#include "ConfigStore.h"

#include <SPIFFS.h>
#include <stdio.h>
#include <string.h>

#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {
        /*
         * F8 修复：写临时文件，成功后再覆盖原文件，尽量减少掉电导致半写。
         * SPIFFS 无 rename 接口，所以采用：写 .tmp → 关闭 → remove 原 → move (.tmp → 原名)。
         * 失败时保留原文件不动。
         */
        bool writeAtomic(const char *path, CSimpleIniA &ini)
        {
            char tmp_path[96];
            snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

            /* 1) 写临时文件 */
            SI_Error rc = ini.SaveFile(tmp_path);
            if (rc < 0)
            {
                LOG_ERROR("CFGSTORE", "save tmp %s failed rc=%d",
                          tmp_path, static_cast<int>(rc));
                return false;
            }

            /* 2) 校验临时文件大小非 0 */
            File tmp = SPIFFS.open(tmp_path, "r");
            if (!tmp)
            {
                LOG_ERROR("CFGSTORE", "open tmp %s for verify failed", tmp_path);
                return false;
            }
            const size_t tmp_size = tmp.size();
            tmp.close();
            if (tmp_size == 0)
            {
                LOG_ERROR("CFGSTORE", "tmp %s is empty", tmp_path);
                SPIFFS.remove(tmp_path);
                return false;
            }

            /* 3) 备份原文件（如果存在）→ 写入新内容 → 删除备份 / 临时文件 */
            char bak_path[96];
            snprintf(bak_path, sizeof(bak_path), "%s.bak", path);

            /* 备份（避免 write+remove 顺序下掉电导致原文件丢） */
            if (SPIFFS.exists(path))
            {
                /* 先删旧 bak，避免 rename-style 冲突 */
                if (SPIFFS.exists(bak_path))
                {
                    SPIFFS.remove(bak_path);
                }
                /* SPIFFS 无 rename：用 read+write 复制 */
                File src = SPIFFS.open(path, "r");
                File dst = SPIFFS.open(bak_path, "w");
                if (src && dst)
                {
                    const size_t sz = src.size();
                    if (sz > 0 && sz < 8192)
                    {
                        uint8_t buf[256];
                        size_t remain = sz;
                        while (remain > 0)
                        {
                            const size_t n = src.read(buf, sizeof(buf));
                            if (n == 0)
                            {
                                break;
                            }
                            dst.write(buf, n);
                            remain -= n;
                        }
                    }
                }
                if (src)
                {
                    src.close();
                }
                if (dst)
                {
                    dst.close();
                }
            }

            /* 4) 把 tmp 内容拷到 path（SPIFFS 无 rename，open("w") truncate） */
            {
                File src = SPIFFS.open(tmp_path, "r");
                File dst = SPIFFS.open(path, "w");
                if (!src || !dst)
                {
                    LOG_ERROR("CFGSTORE", "open for swap failed (src=%d dst=%d)",
                              src ? 1 : 0, dst ? 1 : 0);
                    if (src)
                    {
                        src.close();
                    }
                    if (dst)
                    {
                        dst.close();
                    }
                    return false;
                }
                uint8_t buf[256];
                size_t remain = tmp_size;
                while (remain > 0)
                {
                    const size_t n = src.read(buf, sizeof(buf));
                    if (n == 0)
                    {
                        break;
                    }
                    dst.write(buf, n);
                    remain -= n;
                }
                src.close();
                dst.close();
            }

            /* 5) 清理临时 / 备份 */
            SPIFFS.remove(tmp_path);
            if (SPIFFS.exists(bak_path))
            {
                SPIFFS.remove(bak_path);
            }
            return true;
        }
    } // namespace

    bool ConfigStore::mount()
    {
        if (SPIFFS.begin(true))
        {
            return true;
        }
        LOG_ERROR("CFGSTORE", "SPIFFS mount failed, formatting");
        SPIFFS.format();
        if (SPIFFS.begin())
        {
            return true;
        }
        return false;
    }

    bool ConfigStore::exists(const char *path)
    {
        return SPIFFS.exists(path);
    }

    bool ConfigStore::loadGlobal(const char *path, CSimpleIniA &ini)
    {
        if (!SPIFFS.exists(path))
        {
            return false;
        }
        SI_Error rc = ini.LoadFile(path);
        if (rc < 0)
        {
            LOG_ERROR("CFGSTORE", "load %s failed rc=%d", path, static_cast<int>(rc));
            return false;
        }
        return true;
    }

    bool ConfigStore::saveGlobal(const char *path, CSimpleIniA &ini)
    {
        /* F8 修复：原子写（tmp → swap → 清理） */
        if (!writeAtomic(path, ini))
        {
            LOG_ERROR("CFGSTORE", "atomic save %s failed", path);
            return false;
        }
        return true;
    }

} // namespace ekeys
