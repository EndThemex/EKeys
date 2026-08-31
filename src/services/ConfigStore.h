/*
 * ConfigStore.h
 *
 * SPIFFS + SimpleIni 薄封装（FEATURE_DOC §3.2）。
 * 只负责"挂载文件系统"与"INI 文件读写"，不感知配置语义。
 */

#ifndef EKEYS_SERVICES_CONFIG_STORE_H
#define EKEYS_SERVICES_CONFIG_STORE_H

#include <SimpleIni.h>

namespace ekeys {

class ConfigStore {
public:
    /*
     * 挂载 SPIFFS。挂载失败时允许 format 后重试一次。
     * main.cpp 在 setup 早期调用；失败则由调用方进入报警死循环。
     */
    static bool mount();

    static bool exists(const char *path);

    /*
     * 把 path 的内容载入 ini。文件不存在或解析失败返回 false。
     * ini 需由调用方构造（统一 SetUnicode）。
     */
    static bool loadGlobal(const char *path, CSimpleIniA &ini);

    /*
     * 把 ini 全量写回 path（临时结果先在内存中生成）。
     */
    static bool saveGlobal(const char *path, CSimpleIniA &ini);

private:
    ConfigStore() = delete;
};

}  // namespace ekeys

#endif  // EKEYS_SERVICES_CONFIG_STORE_H
