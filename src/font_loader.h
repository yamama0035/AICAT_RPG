#ifndef FONT_LOADER_H
#define FONT_LOADER_H

#include "ai_monitor.h"
#include "raylib.h"
#include <stdbool.h>

typedef struct {
    Font font;
    unsigned char *ttf_data;
    int ttf_size;
    int font_size;
    int codepoints[4096];
    int count;
    unsigned char seen_bmp[8192]; // U+0000..U+FFFF 用 O(1) 重複チェック
    bool has_custom_font;
} GameFontContext;

// Windows .ttc / .ttf フォントを読み込み、必要な日本語コードポイントを展開する
void InitGameFont(GameFontContext *ctx, const AiMonitorData *ai);

// AIモニターの文字列に未収録の文字が含まれていればフォントを再構築する
void EnsureFontCoversAiData(GameFontContext *ctx, const AiMonitorData *ai);

// 解放処理
void UnloadGameFont(GameFontContext *ctx);

#endif // FONT_LOADER_H
