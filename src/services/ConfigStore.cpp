/*
 * ConfigStore.cpp
 *
 * SPIFFS 挂载与 SimpleIni 文件读写。
 */

#include "ConfigStore.h"

#include <SPIFFS.h>

#include "logging/LogManager.h"

namespace ekeys
{

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
        SI_Error rc = ini.SaveFile(path);
        if (rc < 0)
        {
            LOG_ERROR("CFGSTORE", "save %s failed rc=%d", path, static_cast<int>(rc));
            return false;
        }
        return true;
    }

} // namespace ekeys
