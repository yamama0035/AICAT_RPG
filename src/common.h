#ifndef COMMON_H
#define COMMON_H

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

// 画面解像度
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 640
#define GAME_AREA_WIDTH 960
#define AI_PANEL_X 960
#define AI_PANEL_WIDTH 320

// ログ設定
#define MAX_LOG_LINES 6
#define LOG_BUFFER_SIZE 256

// ゲーム状態
typedef enum {
    STATE_BATTLE,
    STATE_CHAPTER_CLEAR,
    STATE_GAME_OVER,
    STATE_EPILOGUE
} GameState;

// 状態異常・バフ
typedef enum {
    STATUS_NONE = 0,
    STATUS_POISON,    // 毒: 毎ターンダメージ
    STATUS_STUN,      // スタン: 行動不能
    STATUS_SHIELD,    // シールド: 被ダメージ軽減
    STATUS_ATTACK_UP  // 攻撃力強化
} StatusEffect;

// アイテム
typedef struct {
    int id;
    const char *name;
    const char *description;
    int heal_amount;
    int count;
} Item;

// スキル
typedef struct {
    int id;
    const char *name;
    const char *description;
    int min_dmg;
    int max_dmg;
    int heal_val;
    int tension_cost;
    int tension_gain;
    StatusEffect effect;
} Skill;

// 章データ
typedef struct {
    int num;
    const char *title;
    const char *story;
    const char *enemy_name;
    int enemy_max_hp;
    int enemy_atk;
    const char *asset_file;
    const char *bg_asset_file;
    Color bg_color;
} Chapter;

// 共通戦闘ログ関数
void ClearLogs(void);
void AddLog(const char *msg);
const char* GetLogLine(int index);
int GetLogCount(void);

#endif // COMMON_H
