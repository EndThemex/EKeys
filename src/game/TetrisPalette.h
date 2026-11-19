/*
 * TetrisPalette.h
 *
 * 俄罗斯方块颜色单一数据源（docs/14 §4.4 / §5）：
 * 纯宏定义，C 与 C++ 均可 include，供 TetrisConfig.h（C++ 逻辑层）与
 * ui_TetrisScreen.c（C 层 UI）共同引用，杜绝两边颜色不一致。
 */
#ifndef EKEYS_GAME_TETRIS_PALETTE_H
#define EKEYS_GAME_TETRIS_PALETTE_H

/* 方块调色板 RGB 三元组：索引 0=空 1=I 2=O 3=T 4=S 5=Z 6=J 7=L 8=幽灵 */
#define TETRIS_PIECE_RGB_0 0, 0, 0
#define TETRIS_PIECE_RGB_1 0, 220, 255
#define TETRIS_PIECE_RGB_2 255, 220, 0
#define TETRIS_PIECE_RGB_3 180, 90, 255
#define TETRIS_PIECE_RGB_4 40, 230, 80
#define TETRIS_PIECE_RGB_5 255, 70, 70
#define TETRIS_PIECE_RGB_6 60, 120, 255
#define TETRIS_PIECE_RGB_7 255, 150, 0
#define TETRIS_PIECE_RGB_8 70, 70, 70

/* 11 个物理键 LED / 屏幕键位图色块 RGB（下标 = key_id - 1，颜色按物理键固定）
 * 当前功能映射（2026-11-19 调整）：
 *   K1 重开 / K2 右移 / K3 无 / K4 暂停 / K5 旋转 / K6 硬降 /
 *   K7 无 / K8 退出 / K9 左移 / K10 无 / K11 无
 */
#define TETRIS_ACTION_RGB_0 255, 48, 48
#define TETRIS_ACTION_RGB_1 255, 140, 0
#define TETRIS_ACTION_RGB_2 255, 220, 0
#define TETRIS_ACTION_RGB_3 40, 230, 80
#define TETRIS_ACTION_RGB_4 0, 220, 255
#define TETRIS_ACTION_RGB_5 60, 120, 255
#define TETRIS_ACTION_RGB_6 180, 90, 255
#define TETRIS_ACTION_RGB_7 255, 60, 180
#define TETRIS_ACTION_RGB_8 120, 255, 180
#define TETRIS_ACTION_RGB_9 255, 200, 80
#define TETRIS_ACTION_RGB_10 0, 220, 255

/*
 * 竖屏实体键位网格（屏幕在右、按键在左）：
 *   从上到下 4 行、从左到右 3 列；值为 key_id(1~11)，-1 = EC11 旋钮。
 *   ┌──────┬──────┬──────┐
 *   │ K8退出│ K4暂停│ K1重开│
 *   │ K9左移│ K5旋转│ K2右移│
 *   │ K10无 │ K6硬降│ K3无  │
 *   │ K11无 │ K7无  │ 旋钮 │
 *   └──────┴──────┴──────┘
 * 屏幕左侧的键位提示图必须按此顺序摆放，才能与实体键一一对应。
 * K3/K7/K10/K11 暂无功能（屏幕显示 "-"，LED 保留物理键色）。
 */
#define TETRIS_KEY_GRID_COLS 3
#define TETRIS_KEY_GRID_ROWS 4
#define TETRIS_KEY_GRID_IDS \
    {8, 4, 1, 9, 5, 2, 10, 6, 3, 11, 7, -1}

#endif /* EKEYS_GAME_TETRIS_PALETTE_H */
