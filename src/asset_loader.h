#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

#include "raylib.h"
#include <stdbool.h>

// ホットリロード対応テクスチャ構造体
typedef struct {
    Texture2D texture;
    long last_modified;
    bool loaded;
} WatchedTexture;

// テクスチャの更新チェック＆ホットリロード（remove_bg=true で外周背景を透過化）
void UpdateWatchedSprite(WatchedTexture *wt, const char *filepath, bool remove_bg);

// 既存互換ラッパー（背景透過なし）
void UpdateWatchedTexture(WatchedTexture *wt, const char *filepath);

// テクスチャの解放
void UnloadWatchedTexture(WatchedTexture *wt);

#endif // ASSET_LOADER_H
