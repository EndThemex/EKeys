/**
 * @file LvglMemPool.h
 * @brief LVGL 内置内存池（TLSF pool）的分配源声明。
 *
 * lv_conf.h 中 LV_MEM_POOL_ALLOC 指向本函数：
 * lv_mem_init() 在 lv_init() 时调用一次，返回 size 字节的池内存。
 * 实现把池整体放 PSRAM，保住内部 RAM 给 ASR TLS 等关键路径。
 */
#ifndef LVGL_MEM_POOL_H
#define LVGL_MEM_POOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void *lvgl_pool_alloc(size_t size);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_MEM_POOL_H */
