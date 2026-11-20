#include "battle.h"
#include "game_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char battle_logs[MAX_LOG_LINES][LOG_BUFFER_SIZE];
static int log_count = 0;

void ClearLogs(void) {
    log_count = 0;
    for (int i = 0; i < MAX_LOG_LINES; i++) {
        battle_logs[i][0] = '\0';
    }
}

void AddLog(const char *msg) {
    if (!msg) return;
    if (log_count < MAX_LOG_LINES) {
        snprintf(battle_logs[log_count], LOG_BUFFER_SIZE, "%s", msg);
        log_count++;
    } else {
        for (int i = 0; i < MAX_LOG_LINES - 1; i++) {
            snprintf(battle_logs[i], LOG_BUFFER_SIZE, "%s", battle_logs[i + 1]);
        }
        snprintf(battle_logs[MAX_LOG_LINES - 1], LOG_BUFFER_SIZE, "%s", msg);
    }
}

const char* GetLogLine(int index) {
    if (index < 0 || index >= log_count) return "";
    return battle_logs[index];
}

int GetLogCount(void) {
    return log_count;
}

void InitBattle(BattleContext *ctx, int chapter_idx) {
    const Chapter *ch = GetChapter(chapter_idx);
    ClearLogs();
    ctx->current_chapter_idx = chapter_idx;
    ctx->party_max_hp = 450;
    ctx->party_hp = ctx->party_max_hp;
    ctx->enemy_max_hp = ch->enemy_max_hp;
    ctx->enemy_hp = ch->enemy_max_hp;
    ctx->tension = 0;
    ctx->enemy_charging = false;
    ctx->party_shield_turns = 0;
    ctx->enemy_poison_turns = 0;
    ctx->turn_count = 1;
    ctx->last_player_dmg = 0;
    ctx->last_player_heal = 0;
    ctx->last_enemy_dmg = 0;
    ctx->last_was_crit = false;
    ctx->last_was_break = false;
    ctx->last_action_id = 0;

    char buf[160];
    snprintf(buf, sizeof(buf), "【%s】が出現！ 街の毛玉を守るため戦え！", ch->enemy_name);
    AddLog(buf);
}

bool ExecutePlayerAction(BattleContext *ctx, int action_id, Sound se_hit, Sound se_heal) {
    if (ctx->party_hp <= 0 || ctx->enemy_hp <= 0) return false;

    ctx->last_player_dmg = 0;
    ctx->last_player_heal = 0;
    ctx->last_was_crit = false;
    ctx->last_was_break = false;
    ctx->last_action_id = action_id;

    if (action_id == 1) { // とらねこ: 黄金の爪研ぎ
        if (se_hit.frameCount > 0) PlaySound(se_hit);
        int dmg = 45 + rand() % 25;
        bool crit = (rand() % 100) < 18;
        if (crit) {
            dmg = (int)(dmg * 1.5f);
            ctx->last_was_crit = true;
        }
        ctx->enemy_hp -= dmg;
        if (ctx->enemy_hp < 0) ctx->enemy_hp = 0;
        ctx->last_player_dmg = dmg;
        ctx->tension = (ctx->tension + 15 > 100) ? 100 : ctx->tension + 15;
        char buf[160];
        if (crit) {
            snprintf(buf, sizeof(buf), "会心の一撃！ とらねこの黄金爪研ぎで %d ダメージ！", dmg);
        } else {
            snprintf(buf, sizeof(buf), "とらねこの黄金爪研ぎ！ 装甲を切り裂き %d ダメージ！", dmg);
        }
        AddLog(buf);
        return true;
    } else if (action_id == 2) { // ミケ: 猫だまし
        if (se_hit.frameCount > 0) PlaySound(se_hit);
        int dmg = 25 + rand() % 16;
        if (ctx->enemy_charging) {
            dmg += 45;
            ctx->enemy_charging = false;
            ctx->last_was_break = true;
            char buf[160];
            snprintf(buf, sizeof(buf), "ミケの猫だまし！ 敵の吸引充填をブレイクし %d ダメージ！", dmg);
            AddLog(buf);
        } else {
            char buf[160];
            snprintf(buf, sizeof(buf), "ミケの電光石火アタック！ %d ダメージ！", dmg);
            AddLog(buf);
        }
        ctx->enemy_hp -= dmg;
        if (ctx->enemy_hp < 0) ctx->enemy_hp = 0;
        ctx->last_player_dmg = dmg;
        ctx->tension = (ctx->tension + 20 > 100) ? 100 : ctx->tension + 20;
        return true;
    } else if (action_id == 3) { // クロ: マタタビ魔法
        if (se_heal.frameCount > 0) PlaySound(se_heal);
        else if (se_hit.frameCount > 0) PlaySound(se_hit);
        int heal = 45 + rand() % 21;
        int dmg = 20 + rand() % 16;
        int before_hp = ctx->party_hp;
        ctx->party_hp = (ctx->party_hp + heal > ctx->party_max_hp) ? ctx->party_max_hp : ctx->party_hp + heal;
        ctx->last_player_heal = ctx->party_hp - before_hp;
        if (ctx->last_player_heal == 0) ctx->last_player_heal = heal;
        ctx->enemy_hp -= dmg;
        if (ctx->enemy_hp < 0) ctx->enemy_hp = 0;
        ctx->last_player_dmg = dmg;
        ctx->tension = (ctx->tension + 15 > 100) ? 100 : ctx->tension + 15;
        char buf[160];
        snprintf(buf, sizeof(buf), "クロのマタタビ魔法！ HPを %d 回復＆敵に %d ダメージ！", heal, dmg);
        AddLog(buf);
        return true;
    } else if (action_id == 4) { // 合体奥義: 超・毛玉乱舞
        if (ctx->tension < 100) {
            AddLog("テンションが不足しています！（100%必要）");
            return false;
        }
        if (se_hit.frameCount > 0) PlaySound(se_hit);
        int dmg = 300 + rand() % 81;
        ctx->enemy_hp -= dmg;
        if (ctx->enemy_hp < 0) ctx->enemy_hp = 0;
        ctx->last_player_dmg = dmg;
        ctx->last_was_crit = true;
        ctx->tension = 0;
        if (ctx->enemy_charging) {
            ctx->enemy_charging = false;
            ctx->last_was_break = true;
        }
        char buf[160];
        snprintf(buf, sizeof(buf), "合体奥義『超・黄金毛玉乱舞』炸裂！ %d の特大ダメージ！", dmg);
        AddLog(buf);
        return true;
    } else if (action_id == 5) { // 毛玉シールド
        if (ctx->tension < 30) {
            AddLog("テンションが不足しています！（30%必要）");
            return false;
        }
        if (se_heal.frameCount > 0) PlaySound(se_heal);
        ctx->tension -= 30;
        ctx->party_shield_turns = 2;
        AddLog("毛玉シールドを展開！ 2ターン受けるダメージを大幅軽減！");
        return true;
    } else if (action_id == 6) { // 猫パンチラッシュ（毒付与）
        if (ctx->tension < 20) {
            AddLog("テンションが不足しています！（20%必要）");
            return false;
        }
        if (se_hit.frameCount > 0) PlaySound(se_hit);
        ctx->tension -= 20;
        int dmg = 38 + rand() % 20;
        ctx->enemy_hp -= dmg;
        if (ctx->enemy_hp < 0) ctx->enemy_hp = 0;
        ctx->last_player_dmg = dmg;
        ctx->enemy_poison_turns = 3;
        char buf[160];
        snprintf(buf, sizeof(buf), "猫パンチラッシュ！ %d ダメージ＋マタタビ毒を3ターン付与！", dmg);
        AddLog(buf);
        return true;
    }

    return false;
}

bool UseBattleItem(BattleContext *ctx, int item_id, Sound se_heal, Sound se_hit) {
    if (ctx->party_hp <= 0 || ctx->enemy_hp <= 0) return false;

    Item *items = GetItemList();
    int total = GetTotalItems();
    Item *target = NULL;
    for (int i = 0; i < total; i++) {
        if (items[i].id == item_id) {
            target = &items[i];
            break;
        }
    }
    if (!target || target->count <= 0) {
        AddLog("そのアイテムはもう残っていません！");
        return false;
    }

    ctx->last_player_dmg = 0;
    ctx->last_player_heal = 0;
    ctx->last_was_crit = false;
    ctx->last_was_break = false;
    ctx->last_action_id = 100 + item_id;

    target->count--;
    char buf[160];

    switch (item_id) {
        case 1: // 高級カリカリ缶詰
            if (se_heal.frameCount > 0) PlaySound(se_heal);
            ctx->party_hp = (ctx->party_hp + 150 > ctx->party_max_hp) ? ctx->party_max_hp : ctx->party_hp + 150;
            ctx->last_player_heal = 150;
            snprintf(buf, sizeof(buf), "高級カリカリ缶詰を使用！ HPが150回復！（残り%d個）", target->count);
            AddLog(buf);
            break;
        case 2: // マタタビ酒
            if (se_heal.frameCount > 0) PlaySound(se_heal);
            ctx->tension = (ctx->tension + 50 > 100) ? 100 : ctx->tension + 50;
            snprintf(buf, sizeof(buf), "マタタビ酒を飲んだ！ テンション+50%%上昇！（残り%d個）", target->count);
            AddLog(buf);
            break;
        case 3: // 聖なる毛玉ボム
            if (se_hit.frameCount > 0) PlaySound(se_hit);
            ctx->enemy_hp -= 120;
            if (ctx->enemy_hp < 0) ctx->enemy_hp = 0;
            ctx->last_player_dmg = 120;
            snprintf(buf, sizeof(buf), "聖なる毛玉ボム炸裂！ 敵に120の固定ダメージ！（残り%d個）", target->count);
            AddLog(buf);
            break;
        case 4: // 特製マタタビ茶
            if (se_heal.frameCount > 0) PlaySound(se_heal);
            ctx->party_hp = (ctx->party_hp + 80 > ctx->party_max_hp) ? ctx->party_max_hp : ctx->party_hp + 80;
            ctx->last_player_heal = 80;
            ctx->party_shield_turns = 2;
            snprintf(buf, sizeof(buf), "特製マタタビ茶！ HP80回復＆2ターンシールド展開！（残り%d個）", target->count);
            AddLog(buf);
            break;
        default:
            return false;
    }
    return true;
}

void ExecuteEnemyTurn(BattleContext *ctx, Sound se_hit) {
    ctx->last_enemy_dmg = 0;
    if (ctx->enemy_hp <= 0) return;

    const Chapter *ch = GetChapter(ctx->current_chapter_idx);

    if (ctx->enemy_charging) {
        int edmg = ch->enemy_atk * 2;
        if (ctx->party_shield_turns > 0) {
            edmg /= 2;
            AddLog("毛玉シールドが作動！ 必殺吸引ダメージを半減！");
        }
        ctx->party_hp -= edmg;
        ctx->enemy_charging = false;
        ctx->last_enemy_dmg = edmg;
        char buf[160];
        snprintf(buf, sizeof(buf), "警告：全開吸引バキューム直撃！ %d ダメージを受けた！", edmg);
        AddLog(buf);
        if (se_hit.frameCount > 0) PlaySound(se_hit);
    } else {
        int charge_chance = (ctx->current_chapter_idx >= 1) ? 32 : 20;
        if (ctx->turn_count >= 2 && (rand() % 100 < charge_chance)) {
            ctx->enemy_charging = true;
            AddLog("敵がエネルギーを充填中！ 次ターン巨大吸引（猫だましで解除可能）");
        } else {
            int edmg = ch->enemy_atk + (rand() % 11 - 5);
            if (ctx->party_shield_turns > 0) {
                edmg = (int)(edmg * 0.5f);
            }
            if (edmg < 1) edmg = 1;
            ctx->party_hp -= edmg;
            ctx->last_enemy_dmg = edmg;
            char buf[160];
            snprintf(buf, sizeof(buf), "%s の掃討攻撃！ %d ダメージを受けた。", ch->enemy_name, edmg);
            AddLog(buf);
            if (se_hit.frameCount > 0) PlaySound(se_hit);
        }
    }

    if (ctx->party_hp < 0) ctx->party_hp = 0;
}

void ProcessTurnEnd(BattleContext *ctx) {
    if (ctx->enemy_poison_turns > 0 && ctx->enemy_hp > 0) {
        int poison_dmg = 25 + ctx->current_chapter_idx * 5;
        ctx->enemy_hp -= poison_dmg;
        if (ctx->enemy_hp < 0) ctx->enemy_hp = 0;
        ctx->enemy_poison_turns--;
        char buf[160];
        snprintf(buf, sizeof(buf), "毒ダメージ！ %s に %d ダメージ！（残り%dターン）",
                 GetChapter(ctx->current_chapter_idx)->enemy_name, poison_dmg, ctx->enemy_poison_turns);
        AddLog(buf);
    }

    if (ctx->party_shield_turns > 0) {
        ctx->party_shield_turns--;
        if (ctx->party_shield_turns == 0) {
            AddLog("毛玉シールドの効果が切れた。");
        }
    }

    ctx->turn_count++;
}
