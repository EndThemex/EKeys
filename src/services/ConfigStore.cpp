/*
 * ConfigStore.cpp
 *
 * SPIFFS 挂载与 SimpleIni 文件读写。
 */

#include "ConfigStore.h"

#include <SPIFFS.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {
        /*
         * F8 修复：写临时文件，成功后再原子替换原文件，尽量减少掉电导致半写。
         * 2026-09-11 优化：SPIFFS 经 Arduino fs::FS 的 rename() 可用
         * （VFS 层 SPIFFS_rename，仅改索引页），旧实现的 read+write
         * 全文件拷贝（3 次整文件写 + 2 次读）改为 1 次整文件写 + 2 次 rename：
         *   写 .tmp → 旧文件 rename 为 .bak → .tmp rename 为原文件 → 删 .bak。
         * 任一时刻原文件/.bak 至少有一份完整内容；rename 失败时回滚保留原文件。
         */
        bool writeAtomic(const char *path, CSimpleIniA &ini)
        {
            char tmp_path[96];
            char bak_path[96];
            snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
            snprintf(bak_path, sizeof(bak_path), "%s.bak", path);

            /* 1) 序列化到内存（SimpleIni 的 SaveFile 内部用裸 fopen，
             *    无法访问 SPIFFS 挂载点，必须经 Arduino FS 写入；
             *    4.19 的字符串保存接口为 Save(std::string&)） */
            std::string data;
            SI_Error rc = ini.Save(data);
            if (rc < 0 || data.empty())
            {
                LOG_ERROR("CFGSTORE", "serialize %s failed rc=%d",
                          path, static_cast<int>(rc));
                return false;
            }

            /* 2) 经 SPIFFS 写临时文件并校验写入长度 */
            {
                File tmp = SPIFFS.open(tmp_path, "w");
                if (!tmp)
                {
                    LOG_ERROR("CFGSTORE", "open tmp %s for write failed", tmp_path);
                    return false;
                }
                const size_t written = tmp.write(
                    reinterpret_cast<const uint8_t *>(data.data()), data.size());
                tmp.close();
                if (written != data.size())
                {
                    LOG_ERROR("CFGSTORE", "write tmp %s short (%u/%u)",
                              tmp_path, static_cast<unsigned>(written),
                              static_cast<unsigned>(data.size()));
                    SPIFFS.remove(tmp_path);
                    return false;
                }
            }

            /* 3) 旧文件改名 .bak（无内容拷贝）；旧 .bak 先删避免 rename 冲突 */
            if (SPIFFS.exists(bak_path))
            {
                SPIFFS.remove(bak_path);
            }
            if (SPIFFS.exists(path) && !SPIFFS.rename(path, bak_path))
            {
                LOG_ERROR("CFGSTORE", "rename %s -> %s failed", path, bak_path);
                SPIFFS.remove(tmp_path);
                return false;
            }

            /* 4) .tmp 改名为正式文件；失败时回滚 .bak 到原文件 */
            if (!SPIFFS.rename(tmp_path, path))
            {
                LOG_ERROR("CFGSTORE", "rename %s -> %s failed", tmp_path, path);
                SPIFFS.remove(tmp_path);
                if (SPIFFS.exists(bak_path))
                {
                    (void)SPIFFS.rename(bak_path, path);
                }
                return false;
            }

            /* 5) 成功，清理 .bak */
            SPIFFS.remove(bak_path);
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

        /*
         * SimpleIni 的 LoadFile 内部用裸 fopen，而 SPIFFS 挂载在 /spiffs，
         * 只有经 Arduino FS 包装（SPIFFS.open）才会补挂载点前缀，
         * 裸 fopen("/config.ini") 必然返回 SI_FILE(-3)。
         * 这里改为经 SPIFFS 读入内存后 LoadData 解析。
         */
        File f = SPIFFS.open(path, "r");
        if (!f)
        {
            LOG_ERROR("CFGSTORE", "open %s failed", path);
            return false;
        }
        const size_t size = f.size();
        if (size == 0)
        {
            LOG_ERROR("CFGSTORE", "%s is empty", path);
            f.close();
            return false;
        }

        std::vector<char> buf(size);
        const size_t n = f.read(reinterpret_cast<uint8_t *>(buf.data()), size);
        f.close();
        if (n != size)
        {
            LOG_ERROR("CFGSTORE", "read %s short (%u/%u)", path,
                      static_cast<unsigned>(n), static_cast<unsigned>(size));
            return false;
        }

        SI_Error rc = ini.LoadData(buf.data(), n);
        if (rc < 0)
        {
            LOG_ERROR("CFGSTORE", "parse %s failed rc=%d", path,
                      static_cast<int>(rc));
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
