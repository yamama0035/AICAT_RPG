#ifndef AI_MONITOR_H
#define AI_MONITOR_H

#include "raylib.h"
#include <stdbool.h>

#define AI_MONITOR_MAX_HISTORY 3

// AIモニターデータ構造体
typedef struct {
    char phase[32];
    char status[128];
    char current_thought[512];
    char filename[64];
    int steps;
    float cfg_scale;
    char sampler[32];
    int seed;
    char history_time[32];
    char history_title[128];
    int history_count;
    char history_times[AI_MONITOR_MAX_HISTORY][32];
    char history_titles[AI_MONITOR_MAX_HISTORY][128];
    char history_details[AI_MONITOR_MAX_HISTORY][128];
    long last_modified;
} AiMonitorData;

// AIモニター情報の更新（ai_status.jsonの読み込み）
void UpdateAiMonitor(AiMonitorData *data);

// 右ペインのAIモニター画面描画
void DrawAiMonitor(const AiMonitorData *data, Font font);

// UTF-8テキストを指定幅内に折り返して描画する共通ヘルパー
int DrawTextWrapped(Font font, const char *text, Rectangle bounds,
                    float font_size, float line_height, int max_lines, Color tint);

#endif // AI_MONITOR_H
