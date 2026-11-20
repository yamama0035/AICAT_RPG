#ifndef BATTLE_H
#define BATTLE_H

#include "common.h"
#include "raylib.h"
#include <stdbool.h>

// 戦闘コンテキスト構造体
typedef struct {
    int party_hp;
    int party_max_hp;
    int enemy_hp;
    int enemy_max_hp;
    int tension;
    bool enemy_charging;
    int party_shield_turns;
    int enemy_poison_turns;
    int turn_count;
    int current_chapter_idx;
    // UI演出用フィードバック情報
    int last_player_dmg;
    int last_player_heal;
    int last_enemy_dmg;
    bool last_was_crit;
    bool last_was_break;
    int last_action_id;
} BattleContext;

// 戦闘の初期化
void InitBattle(BattleContext *ctx, int chapter_idx);

// プレイヤーのアクション実行（1..6のスキルID）
// 戻り値: アクションが正常に実行されたか
bool ExecutePlayerAction(BattleContext *ctx, int action_id, Sound se_hit, Sound se_heal);

// アイテム使用
bool UseBattleItem(BattleContext *ctx, int item_id, Sound se_heal, Sound se_hit);

// 敵ターンの実行
void ExecuteEnemyTurn(BattleContext *ctx, Sound se_hit);

// ターン終了処理（状態異常などの判定）
void ProcessTurnEnd(BattleContext *ctx);

#endif // BATTLE_H
