#include "ai_monitor.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int AppendUtf8(unsigned int cp, char *out, int idx, int max_len) {
    if (cp < 0x80) {
        if (idx + 1 < max_len) out[idx++] = (char)cp;
    } else if (cp < 0x800) {
        if (idx + 2 < max_len) {
            out[idx++] = (char)(0xC0 | (cp >> 6));
            out[idx++] = (char)(0x80 | (cp & 0x3F));
        }
    } else if (cp < 0x10000) {
        if (idx + 3 < max_len) {
            out[idx++] = (char)(0xE0 | (cp >> 12));
            out[idx++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[idx++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    return idx;
}

static void ExtractJsonString(const char *json, const char *key, char *out, int max_len) {
    if (!json || !key || !out || max_len <= 0) return;
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *pos = strstr(json, pattern);
    if (!pos) return;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    if (*pos != '\"') return;

    pos++;
    int idx = 0;
    while (*pos && *pos != '\"' && idx < max_len - 1) {
        if (*pos == '\\' && *(pos + 1)) {
            pos++;
            char esc = *pos++;
            if (esc == 'n') {
                out[idx++] = ' ';
            } else if (esc == 'r') {
                /* skip */
            } else if (esc == 't') {
                out[idx++] = ' ';
            } else if (esc == 'u' && pos[0] && pos[1] && pos[2] && pos[3]) {
                int h0 = HexVal(pos[0]), h1 = HexVal(pos[1]), h2 = HexVal(pos[2]), h3 = HexVal(pos[3]);
                if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
                    unsigned int cp = (unsigned int)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
                    idx = AppendUtf8(cp, out, idx, max_len);
                    pos += 4;
                }
            } else {
                out[idx++] = esc;
            }
        } else {
            out[idx++] = *pos++;
        }
    }
    out[idx] = '\0';
}

static int ExtractJsonInt(const char *json, const char *key, int default_val) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *pos = strstr(json, pattern);
    if (!pos) return default_val;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    return atoi(pos);
}

static float ExtractJsonFloat(const char *json, const char *key, float default_val) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *pos = strstr(json, pattern);
    if (!pos) return default_val;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    return (float)atof(pos);
}

static int Utf8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

int DrawTextWrapped(Font font, const char *text, Rectangle bounds,
                    float font_size, float line_height, int max_lines, Color tint) {
    if (!text || !text[0]) return 0;

    int len = (int)strlen(text);
    int cur = 0;
    int lines_drawn = 0;

    while (cur < len && lines_drawn < max_lines) {
        while (cur < len && (text[cur] == ' ' || text[cur] == '\r')) cur++;
        if (cur >= len) break;
        if (text[cur] == '\n') {
            cur++;
            lines_drawn++;
            continue;
        }

        int best_end = cur;
        int last_space = -1;
        int pos = cur;
        bool forced_newline = false;

        while (pos < len) {
            if (text[pos] == '\n') {
                best_end = pos;
                forced_newline = true;
                break;
            }
            int clen = Utf8CharLen((unsigned char)text[pos]);
            if (pos + clen > len) clen = len - pos;

            char probe[512];
            int slice_len = (pos + clen) - cur;
            if (slice_len >= (int)sizeof(probe)) break;
            memcpy(probe, text + cur, (size_t)slice_len);
            probe[slice_len] = '\0';

            Vector2 measured = MeasureTextEx(font, probe, font_size, 1.0f);
            if (measured.x > bounds.width && pos > cur) {
                break;
            }
            if (text[pos] == ' ' || text[pos] == '/' || text[pos] == ',') {
                last_space = pos + clen;
            }
            pos += clen;
            best_end = pos;
        }

        if (!forced_newline && pos < len && last_space > cur) {
            if (last_space - cur > (best_end - cur) / 2) {
                best_end = last_space;
            }
        }
        if (best_end <= cur) {
            best_end = cur + Utf8CharLen((unsigned char)text[cur]);
            if (best_end > len) best_end = len;
        }

        char line_buf[512];
        int copy_len = best_end - cur;
        if (copy_len >= (int)sizeof(line_buf)) copy_len = (int)sizeof(line_buf) - 1;
        memcpy(line_buf, text + cur, (size_t)copy_len);
        line_buf[copy_len] = '\0';

        if (lines_drawn == max_lines - 1 && best_end < len && copy_len > 3) {
            int trim = copy_len;
            while (trim > 0 && ((unsigned char)line_buf[trim - 1] & 0xC0) == 0x80) trim--;
            if (trim > 0) trim--;
            snprintf(line_buf + trim, sizeof(line_buf) - (size_t)trim, "...");
        }

        DrawTextEx(font, line_buf, (Vector2){bounds.x, bounds.y + lines_drawn * line_height}, font_size, 1.0f, tint);
        lines_drawn++;
        cur = best_end;
        if (forced_newline && cur < len && text[cur] == '\n') cur++;
    }
    return lines_drawn;
}

void UpdateAiMonitor(AiMonitorData *data) {
    if (!data) return;
    const char *filepath = "ai_status.json";
    if (!FileExists(filepath)) return;
    long mod_time = GetFileModTime(filepath);
    if (mod_time <= data->last_modified) return;
    data->last_modified = mod_time;

    char *json = LoadFileText(filepath);
    if (!json) return;

    ExtractJsonString(json, "phase", data->phase, sizeof(data->phase));
    ExtractJsonString(json, "status", data->status, sizeof(data->status));
    ExtractJsonString(json, "current_thought", data->current_thought, sizeof(data->current_thought));
    ExtractJsonString(json, "filename", data->filename, sizeof(data->filename));
    ExtractJsonString(json, "sampler_name", data->sampler, sizeof(data->sampler));
    data->steps = ExtractJsonInt(json, "steps", 20);
    data->cfg_scale = ExtractJsonFloat(json, "cfg_scale", 7.0f);
    data->seed = ExtractJsonInt(json, "seed", -1);

    data->history_count = 0;
    data->history_time[0] = '\0';
    data->history_title[0] = '\0';

    const char *h_pos = strstr(json, "\"history\":");
    if (h_pos) {
        const char *cursor = h_pos;
        for (int i = 0; i < AI_MONITOR_MAX_HISTORY; i++) {
            const char *obj = strchr(cursor, '{');
            if (!obj) break;
            const char *obj_end = strchr(obj, '}');
            if (!obj_end) break;

            size_t obj_len = (size_t)(obj_end - obj + 1);
            char item_buf[1024];
            if (obj_len >= sizeof(item_buf)) obj_len = sizeof(item_buf) - 1;
            memcpy(item_buf, obj, obj_len);
            item_buf[obj_len] = '\0';

            data->history_times[i][0] = '\0';
            data->history_titles[i][0] = '\0';
            data->history_details[i][0] = '\0';

            ExtractJsonString(item_buf, "time", data->history_times[i], sizeof(data->history_times[i]));
            ExtractJsonString(item_buf, "title", data->history_titles[i], sizeof(data->history_titles[i]));
            ExtractJsonString(item_buf, "detail", data->history_details[i], sizeof(data->history_details[i]));

            if (i == 0) {
                snprintf(data->history_time, sizeof(data->history_time), "%s", data->history_times[0]);
                snprintf(data->history_title, sizeof(data->history_title), "%s", data->history_titles[0]);
            }
            data->history_count++;
            cursor = obj_end + 1;
        }
    }

    UnloadFileText(json);
}

void DrawAiMonitor(const AiMonitorData *data, Font font) {
    int px = AI_PANEL_X;
    int pw = AI_PANEL_WIDTH;

    DrawRectangle(px, 0, pw, SCREEN_HEIGHT, (Color){13, 16, 22, 255});
    DrawLine(px, 0, px, SCREEN_HEIGHT, (Color){38, 48, 62, 255});

    DrawRectangle(px, 0, pw, 46, (Color){20, 26, 38, 255});
    DrawLine(px, 46, px + pw, 46, (Color){0, 240, 255, 140});

    float pulse = (sinf((float)GetTime() * 4.5f) + 1.0f) * 0.5f;
    Color dot_color = (Color){0, (unsigned char)(180 + 75 * pulse), (unsigned char)(220 + 35 * pulse), 255};
    DrawCircle(px + 22, 23, 6, dot_color);
    DrawTextEx(font, "AI LIVE DIRECTOR", (Vector2){(float)(px + 36), 14}, 17, 1.0f, RAYWHITE);

    DrawRectangle(px + 14, 54, pw - 28, 38, (Color){18, 24, 34, 255});
    DrawRectangleLines(px + 14, 54, pw - 28, 38, (Color){0, 240, 255, 100});
    char status_line[160];
    snprintf(status_line, sizeof(status_line), "状態: %s", data->status[0] ? data->status : "待機中");
    DrawTextWrapped(font, status_line, (Rectangle){(float)(px + 22), 62, (float)(pw - 44), 26},
                    14.0f, 16.0f, 1, (Color){0, 240, 255, 255});

    DrawRectangle(px + 14, 100, pw - 28, 212, (Color){18, 22, 30, 255});
    DrawRectangleLines(px + 14, 100, pw - 28, 212, (Color){55, 70, 90, 255});
    DrawTextEx(font, "【AIの現在の思考・意図】", (Vector2){(float)(px + 22), 108}, 15, 1.0f, (Color){255, 183, 77, 255});

    const char *thought = data->current_thought[0] ? data->current_thought : "システム監視中...";
    DrawTextWrapped(font, thought, (Rectangle){(float)(px + 22), 132, (float)(pw - 44), 174},
                    14.0f, 21.0f, 8, (Color){220, 230, 242, 255});

    DrawRectangle(px + 14, 320, pw - 28, 126, (Color){18, 22, 30, 255});
    DrawRectangleLines(px + 14, 320, pw - 28, 126, (Color){55, 70, 90, 255});
    DrawTextEx(font, "【SD生成パラメータ (LLM調整)】", (Vector2){(float)(px + 22), 328}, 15, 1.0f, (Color){100, 220, 255, 255});

    char param_buf[128];
    snprintf(param_buf, sizeof(param_buf), "Target: %s", data->filename[0] ? data->filename : "---");
    DrawTextWrapped(font, param_buf, (Rectangle){(float)(px + 22), 354, (float)(pw - 44), 20},
                    13.0f, 16.0f, 1, LIGHTGRAY);

    snprintf(param_buf, sizeof(param_buf), "Steps: %d (最適化)", data->steps > 0 ? data->steps : 20);
    DrawTextEx(font, param_buf, (Vector2){(float)(px + 22), 378}, 13, 1.0f, (Color){0, 245, 155, 255});

    snprintf(param_buf, sizeof(param_buf), "CFG: %.1f", data->cfg_scale > 0 ? data->cfg_scale : 7.0f);
    DrawTextEx(font, param_buf, (Vector2){(float)(px + 190), 378}, 13, 1.0f, (Color){255, 183, 77, 255});

    snprintf(param_buf, sizeof(param_buf), "Sampler: %s", data->sampler[0] ? data->sampler : "Euler a");
    DrawTextEx(font, param_buf, (Vector2){(float)(px + 22), 400}, 13, 1.0f, LIGHTGRAY);

    if (data->seed >= 0) {
        snprintf(param_buf, sizeof(param_buf), "Seed: %d", data->seed);
    } else {
        snprintf(param_buf, sizeof(param_buf), "Seed: Random(-1)");
    }
    DrawTextEx(font, param_buf, (Vector2){(float)(px + 22), 422}, 13, 1.0f, (Color){170, 185, 205, 255});

    DrawRectangle(px + 14, 454, pw - 28, 174, (Color){18, 22, 30, 255});
    DrawRectangleLines(px + 14, 454, pw - 28, 174, (Color){55, 70, 90, 255});
    DrawTextEx(font, "【直近の開発アクション】", (Vector2){(float)(px + 22), 462}, 15, 1.0f, (Color){186, 104, 200, 255});

    if (data->history_count > 0) {
        int hy = 486;
        for (int i = 0; i < data->history_count && i < 3; i++) {
            char line_buf[192];
            snprintf(line_buf, sizeof(line_buf), "[%s] %s",
                     data->history_times[i][0] ? data->history_times[i] : "--:--",
                     data->history_titles[i]);
            DrawTextWrapped(font, line_buf, (Rectangle){(float)(px + 22), (float)hy, (float)(pw - 44), 36},
                            13.0f, 16.0f, 2, (i == 0) ? WHITE : (Color){170, 185, 200, 255});
            hy += 38;
        }
    } else {
        DrawTextEx(font, "アクション待機中...", (Vector2){(float)(px + 22), 494}, 13, 1.0f, GRAY);
    }

    DrawTextEx(font, "Web: http://127.0.0.1:8080", (Vector2){(float)(px + 22), 606}, 12, 1.0f, (Color){95, 120, 150, 255});
}
