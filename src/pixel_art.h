#ifndef PIXEL_ART_H
#define PIXEL_ART_H

#include "raylib.h"
#include <stdbool.h>

// 16bit風プロシージャル猫スプライト描画（PNG未生成時用）
void DrawPixelCat(int party_idx, float cx, float bottom_y, float px_size, Color tint);

// 16bit風プロシージャル敵メカスプライト描画（PNG未生成時用）
void DrawPixelEnemy(int chapter_idx, float cx, float bottom_y, float px_size,
                    bool charging, float time, Color tint);

// 各章の雰囲気に合わせたプロシージャル背景描画（PNG未生成時用）
void DrawProceduralStageBg(int chapter_idx, Rectangle area, float time);

#endif // PIXEL_ART_H
