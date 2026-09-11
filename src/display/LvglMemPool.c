/**
 * @file LvglMemPool.c
 * @brief LVGL 内置内存池的分配源（lv_conf.h 的 LV_MEM_POOL_ALLOC）。
 *
 * 注意：必须保持 C 链接（.c 文件），因为唯一调用方 lvgl/src/misc/lv_mem.c
 * 是 C 翻译单元；改名/迁移时保持 LvglMemPool.h 的 extern "C" 声明一致。
 */
#include "LvglMemPool.h"

#include <stdlib.h>

#include "esp_heap_caps.h"

void *lvgl_pool_alloc(size_t size)
{
    /* 池整体放 PSRAM，与 LvglPort 双缓冲策略一致（PSRAM 不可用时退回 malloc 保底）。
     * 仅在 lv_mem_init() 时调用一次，失败返回 NULL 会让 lv_tlsf_create_with_pool 崩溃，
     * 本板 N16R8 固定带 8MB PSRAM，不做更多兜底。 */
    void *pool = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (pool == NULL)
    {
        pool = malloc(size);
    }
    return pool;
}
