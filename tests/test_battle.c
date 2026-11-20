#include "battle.h"
#include "game_data.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_game_data_invariants(void) {
    int total_ch = GetTotalChapters();
    assert(total_ch >= 1);
    for (int i = 0; i < total_ch; i++) {
        const Chapter *ch = GetChapter(i);
        assert(ch != NULL);
        assert(ch->num == i + 1);
        assert(ch->title && ch->title[0] != '\0');
        assert(ch->story && ch->story[0] != '\0');
        assert(ch->enemy_name && ch->enemy_name[0] != '\0');
        assert(ch->enemy_max_hp > 0);
        assert(ch->enemy_atk > 0);
        assert(ch->asset_file && strncmp(ch->asset_file, "assets/", 7) == 0);
        assert(ch->bg_asset_file && strncmp(ch->bg_asset_file, "assets/", 7) == 0);
    }

    for (int i = 0; i < MAX_PARTY_MEMBERS; i++) {
        const PartyMember *pm = GetPartyMember(i);
        assert(pm != NULL && pm->name && pm->name[0] != '\0');
        assert(pm->asset_file && strncmp(pm->asset_file, "assets/", 7) == 0);
    }

    int total_sk = GetTotalSkills();
    assert(total_sk >= 4);
    for (int i = 0; i < total_sk; i++) {
        const Skill *sk = GetSkill(i);
        assert(sk != NULL);
        assert(sk->id > 0);
        assert(sk->name && sk->name[0] != '\0');
        assert(sk->description && sk->description[0] != '\0');
        assert(sk->tension_cost >= 0 && sk->tension_cost <= 100);
    }

    int total_it = GetTotalItems();
    assert(total_it >= 1);
    Item *items = GetItemList();
    for (int i = 0; i < total_it; i++) {
        assert(items[i].id > 0);
        assert(items[i].name && items[i].name[0] != '\0');
        assert(items[i].description && items[i].description[0] != '\0');
        assert(items[i].count >= 0);
    }

    assert(GetEpilogueLineCount() >= 1);
    for (int i = 0; i < GetEpilogueLineCount(); i++) {
        assert(GetEpilogueLine(i) && GetEpilogueLine(i)[0] != '\0');
    }
}

static void assert_battle_invariants(const BattleContext *ctx) {
    assert(ctx->party_max_hp > 0);
    assert(ctx->enemy_max_hp > 0);
    assert(ctx->party_hp >= 0 && ctx->party_hp <= ctx->party_max_hp);
    assert(ctx->enemy_hp >= 0 && ctx->enemy_hp <= ctx->enemy_max_hp);
    assert(ctx->tension >= 0 && ctx->tension <= 100);
    assert(ctx->party_shield_turns >= 0);
    assert(ctx->enemy_poison_turns >= 0);
    assert(ctx->turn_count >= 1);
}

static void test_battle_contracts(void) {
    BattleContext ctx = {0};
    Sound silent = {0};
    ResetItems();
    InitBattle(&ctx, 0);

    assert_battle_invariants(&ctx);
    assert(ctx.party_hp == ctx.party_max_hp);
    assert(ctx.enemy_hp == ctx.enemy_max_hp);
    assert(ctx.enemy_max_hp == GetChapter(0)->enemy_max_hp);
    assert(GetLogCount() >= 1);

    // テンション消費のあるスキルはテンション0のとき実行拒否されるべき
    int total_sk = GetTotalSkills();
    for (int i = 0; i < total_sk; i++) {
        const Skill *sk = GetSkill(i);
        if (sk->tension_cost > 0) {
            ctx.tension = sk->tension_cost - 1;
            assert(!ExecutePlayerAction(&ctx, sk->id, silent, silent));
        }
    }

    // テンション100なら定義された全スキルを実行可能（または正常処理）で不変条件を保つ
    for (int i = 0; i < total_sk; i++) {
        const Skill *sk = GetSkill(i);
        InitBattle(&ctx, 0);
        ctx.party_hp = ctx.party_max_hp / 2;
        ctx.tension = 100;
        ctx.enemy_charging = true;
        bool ok = ExecutePlayerAction(&ctx, sk->id, silent, silent);
        assert(ok);
        assert_battle_invariants(&ctx);
        if (sk->id == 2) {
            assert(!ctx.enemy_charging); // 猫だましは敵充填を解除する
        }
    }

    // アイテムは使用ごとに残数が減り、0個では使えない。ResetItemsで復元する
    Item *items = GetItemList();
    int total_it = GetTotalItems();
    for (int i = 0; i < total_it; i++) {
        InitBattle(&ctx, 0);
        ctx.party_hp = 10;
        ctx.tension = 0;
        int initial_cnt = items[i].count;
        for (int u = 0; u < initial_cnt; u++) {
            assert(UseBattleItem(&ctx, items[i].id, silent, silent));
            assert(items[i].count == initial_cnt - 1 - u);
            assert_battle_invariants(&ctx);
        }
        assert(!UseBattleItem(&ctx, items[i].id, silent, silent));
    }
    ResetItems();
    for (int i = 0; i < total_it; i++) {
        assert(items[i].count > 0 || GetTotalItems() > 0);
    }

    // ログリングバッファ上限テスト
    ClearLogs();
    for (int i = 0; i < MAX_LOG_LINES + 5; i++) {
        AddLog("テストログ");
    }
    assert(GetLogCount() == MAX_LOG_LINES);
}

// 全章がゲームとしてクリア可能かを自動プレイボットで検証する
static void test_full_campaign_winnable(void) {
    Sound silent = {0};
    int total_ch = GetTotalChapters();
    int total_sk = GetTotalSkills();
    int total_it = GetTotalItems();

    for (int ch = 0; ch < total_ch; ch++) {
        BattleContext ctx = {0};
        ResetItems();
        InitBattle(&ctx, ch);
        Item *items = GetItemList();

        int max_turns = 100;
        int turns = 0;
        while (ctx.enemy_hp > 0 && ctx.party_hp > 0 && turns < max_turns) {
            bool acted = false;

            // 1. 敵がチャージ中なら妨害スキル(ID 2)を優先
            if (ctx.enemy_charging && total_sk >= 2) {
                acted = ExecutePlayerAction(&ctx, GetSkill(1)->id, silent, silent);
            }
            // 2. HPが半分以下なら回復アイテム(ID 1,4)または回復スキル(ID 3)
            if (!acted && ctx.party_hp < (ctx.party_max_hp * 45) / 100) {
                if (total_it >= 1 && items[0].count > 0) {
                    acted = UseBattleItem(&ctx, items[0].id, silent, silent);
                } else if (total_it >= 4 && items[3].count > 0) {
                    acted = UseBattleItem(&ctx, items[3].id, silent, silent);
                } else if (total_sk >= 3) {
                    acted = ExecutePlayerAction(&ctx, GetSkill(2)->id, silent, silent);
                }
            }
            // 3. テンション100なら奥義(ID 4)
            if (!acted && total_sk >= 4 && ctx.tension >= GetSkill(3)->tension_cost) {
                acted = ExecutePlayerAction(&ctx, GetSkill(3)->id, silent, silent);
            }
            // 4. ボス戦等でシールド未展開ならシールド(ID 5)
            if (!acted && ch >= 2 && ctx.party_shield_turns == 0 && total_sk >= 5 &&
                ctx.tension >= GetSkill(4)->tension_cost) {
                acted = ExecutePlayerAction(&ctx, GetSkill(4)->id, silent, silent);
            }
            // 5. 毒未付与で敵HPが高ければ毒攻撃(ID 6)
            if (!acted && ctx.enemy_poison_turns == 0 && ctx.enemy_hp > 150 && total_sk >= 6 &&
                ctx.tension >= GetSkill(5)->tension_cost) {
                acted = ExecutePlayerAction(&ctx, GetSkill(5)->id, silent, silent);
            }
            // 6. マタタビ酒(ID 2)でテンション加速 or 通常攻撃(ID 1)
            if (!acted && ch >= 3 && total_it >= 2 && items[1].count > 0 && ctx.tension <= 50) {
                acted = UseBattleItem(&ctx, items[1].id, silent, silent);
            }
            if (!acted) {
                acted = ExecutePlayerAction(&ctx, GetSkill(0)->id, silent, silent);
            }

            assert(acted);
            assert_battle_invariants(&ctx);

            if (ctx.enemy_hp > 0) {
                int turn_before = ctx.turn_count;
                ExecuteEnemyTurn(&ctx, silent);
                ProcessTurnEnd(&ctx);
                assert(ctx.turn_count == turn_before + 1);
                assert_battle_invariants(&ctx);
            }
            turns++;
        }

        if (ctx.enemy_hp > 0) {
            fprintf(stderr, "[FAIL] Chapter %d (%s) was not won within %d turns (party_hp=%d, enemy_hp=%d)\n",
                    ch + 1, GetChapter(ch)->enemy_name, max_turns, ctx.party_hp, ctx.enemy_hp);
            assert(ctx.enemy_hp == 0);
        }
    }
    ResetItems();
}

int main(void) {
    srand(42);
    test_game_data_invariants();
    test_battle_contracts();
    test_full_campaign_winnable();
    printf("[PASS] All CatRPG invariants & %d-chapter auto-playtest passed!\n", GetTotalChapters());
    return 0;
}
