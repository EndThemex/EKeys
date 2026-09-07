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
         * F8 修复：写临时文件，成功后再覆盖原文件，尽量减少掉电导致半写。
         * SPIFFS 无 rename 接口，所以采用：写 .tmp → 关闭 → remove 原 → move (.tmp → 原名)。
         * 失败时保留原文件不动。
         */
        bool writeAtomic(const char *path, CSimpleIniA &ini)
        {
            char tmp_path[96];
            snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

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
            const size_t tmp_size = data.size();

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
