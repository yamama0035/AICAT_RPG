#include "common.h"
#include "asset_loader.h"
#include "ai_monitor.h"
#include "font_loader.h"
#include "pixel_art.h"
#include "game_data.h"
#include "battle.h"

#define MAX_FLOAT_TEXTS 8

typedef struct {
    bool active;
    float timer;
    float duration;
    Vector2 pos;
    char text[48];
    Color color;
} FloatingText;

static void SpawnFloatingText(FloatingText *pool, Vector2 pos, const char *text, Color color) {
    for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
        if (!pool[i].active) {
            pool[i].active = true;
            pool[i].timer = 0.0f;
            pool[i].duration = 0.95f;
            pool[i].pos = pos;
            snprintf(pool[i].text, sizeof(pool[i].text), "%s", text);
            pool[i].color = color;
            return;
        }
    }
}

static void DrawProgressBar(Rectangle bar, float ratio, Color fill_color, Color bg_color, Color border_color) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    DrawRectangleRec(bar, bg_color);
    if (ratio > 0.0f) {
        Rectangle fill = {bar.x + 2, bar.y + 2, (bar.width - 4) * ratio, bar.height - 4};
        if (fill.width > 0) {
            DrawRectangleRec(fill, fill_color);
            DrawRectangle((int)fill.x, (int)fill.y, (int)fill.width, (int)(fill.height * 0.38f),
                          Fade(WHITE, 0.22f));
        }
    }
    DrawRectangleLinesEx(bar, 1.5f, border_color);
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "キャットピア防衛戦 - Cat vs Machine");
    InitAudioDevice();
    SetTargetFPS(60);
    srand((unsigned int)time(NULL));

    AiMonitorData ai_monitor = {0};
    UpdateAiMonitor(&ai_monitor);
    long prev_ai_mod = ai_monitor.last_modified;

    GameFontContext font_ctx = {0};
    InitGameFont(&font_ctx, &ai_monitor);

    Sound se_hit = {0}, se_heal = {0}, se_ougi = {0};
    if (FileExists("assets/se_hit.wav")) se_hit = LoadSound("assets/se_hit.wav");
    if (FileExists("assets/se_heal.wav")) se_heal = LoadSound("assets/se_heal.wav");
    if (FileExists("assets/se_ougi.wav")) se_ougi = LoadSound("assets/se_ougi.wav");

    GameState state = STATE_BATTLE;
    BattleContext battle = {0};
    InitBattle(&battle, 0);

    WatchedTexture enemy_tex = {0};
    WatchedTexture bg_tex = {0};
    WatchedTexture party_tex[MAX_PARTY_MEMBERS] = {0};

    const Chapter *cur_ch = GetChapter(battle.current_chapter_idx);
    UpdateWatchedSprite(&enemy_tex, cur_ch->asset_file, true);
    UpdateWatchedSprite(&bg_tex, cur_ch->bg_asset_file, false);
    for (int i = 0; i < MAX_PARTY_MEMBERS; i++) {
        UpdateWatchedSprite(&party_tex[i], GetPartyMember(i)->asset_file, true);
    }

    FloatingText floats[MAX_FLOAT_TEXTS] = {0};
    bool pending_enemy_turn = false;
    float enemy_turn_delay = 0.0f;
    int active_attacker = -1;
    float player_lunge_timer = 0.0f;
    float enemy_hit_timer = 0.0f;
    float party_hit_timer = 0.0f;
    float heal_fx_timer = 0.0f;
    float ougi_fx_timer = 0.0f;
    float screen_shake_timer = 0.0f;
    int skill_page = 0;
    int item_page = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        float now = (float)GetTime();

        cur_ch = GetChapter(battle.current_chapter_idx);

        UpdateWatchedSprite(&enemy_tex, cur_ch->asset_file, true);
        UpdateWatchedSprite(&bg_tex, cur_ch->bg_asset_file, false);
        for (int i = 0; i < MAX_PARTY_MEMBERS; i++) {
            UpdateWatchedSprite(&party_tex[i], GetPartyMember(i)->asset_file, true);
        }
        UpdateAiMonitor(&ai_monitor);
        if (ai_monitor.last_modified != prev_ai_mod) {
            prev_ai_mod = ai_monitor.last_modified;
            EnsureFontCoversAiData(&font_ctx, &ai_monitor);
        }

        Font jp_font = font_ctx.font;
        Vector2 mouse = GetMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        if (player_lunge_timer > 0.0f) player_lunge_timer -= dt;
        if (enemy_hit_timer > 0.0f) enemy_hit_timer -= dt;
        if (party_hit_timer > 0.0f) party_hit_timer -= dt;
        if (heal_fx_timer > 0.0f) heal_fx_timer -= dt;
        if (ougi_fx_timer > 0.0f) ougi_fx_timer -= dt;
        if (screen_shake_timer > 0.0f) screen_shake_timer -= dt;

        for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
            if (floats[i].active) {
                floats[i].timer += dt;
                floats[i].pos.y -= 36.0f * dt;
                if (floats[i].timer >= floats[i].duration) {
                    floats[i].active = false;
                }
            }
        }

        Rectangle btn_skill[6];
        for (int i = 0; i < 6; i++) {
            int col = i % 3;
            int row = i / 3;
            btn_skill[i] = (Rectangle){(float)(24 + col * 194), (float)(478 + row * 48), 186, 42};
        }
        Rectangle btn_item[4];
        for (int i = 0; i < 4; i++) {
            int col = i % 2;
            int row = i / 2;
            btn_item[i] = (Rectangle){(float)(622 + col * 160), (float)(478 + row * 48), 154, 42};
        }

        int total_skills = GetTotalSkills();
        int max_skill_pages = (total_skills > 0) ? ((total_skills + 5) / 6) : 1;
        if (skill_page >= max_skill_pages) skill_page = 0;

        int total_items = GetTotalItems();
        int max_item_pages = (total_items > 0) ? ((total_items + 3) / 4) : 1;
        if (item_page >= max_item_pages) item_page = 0;

        Rectangle skill_page_btn = {524, 456, 80, 20};
        Rectangle item_page_btn = {854, 456, 82, 20};

        char hover_hint_buf[256];
        const char *hover_hint = "スキル[1-6]・アイテム[Q/W/E/R]をクリックまたはキー入力で選択してください。";

        if (state == STATE_BATTLE) {
            if (pending_enemy_turn) {
                enemy_turn_delay -= dt;
                if (enemy_turn_delay <= 0.0f) {
                    pending_enemy_turn = false;
                    int prev_enemy_hp = battle.enemy_hp;
                    ExecuteEnemyTurn(&battle, se_hit);
                    ProcessTurnEnd(&battle);

                    if (battle.last_enemy_dmg > 0) {
                        char dmg_str[32];
                        snprintf(dmg_str, sizeof(dmg_str), "-%d", battle.last_enemy_dmg);
                        SpawnFloatingText(floats, (Vector2){210, 118}, dmg_str, (Color){255, 85, 85, 255});
                        party_hit_timer = 0.30f;
                        screen_shake_timer = 0.16f;
                    }
                    if (battle.enemy_hp < prev_enemy_hp) {
                        char pdmg_str[32];
                        snprintf(pdmg_str, sizeof(pdmg_str), "毒 -%d", prev_enemy_hp - battle.enemy_hp);
                        SpawnFloatingText(floats, (Vector2){710, 120}, pdmg_str, (Color){196, 112, 255, 255});
                    }

                    if (battle.enemy_hp <= 0) {
                        battle.enemy_hp = 0;
                        state = (battle.current_chapter_idx >= GetTotalChapters() - 1) ? STATE_EPILOGUE : STATE_CHAPTER_CLEAR;
                    } else if (battle.party_hp <= 0) {
                        battle.party_hp = 0;
                        state = STATE_GAME_OVER;
                    }
                }
            } else {
                for (int i = 0; i < 6; i++) {
                    int sk_idx = skill_page * 6 + i;
                    if (sk_idx < total_skills && CheckCollisionPointRec(mouse, btn_skill[i])) {
                        const Skill *sk = GetSkill(sk_idx);
                        snprintf(hover_hint_buf, sizeof(hover_hint_buf), "%s: %s", sk->name, sk->description);
                        hover_hint = hover_hint_buf;
                    }
                }
                Item *items_list = GetItemList();
                for (int i = 0; i < 4; i++) {
                    int it_idx = item_page * 4 + i;
                    if (it_idx < total_items && CheckCollisionPointRec(mouse, btn_item[i])) {
                        snprintf(hover_hint_buf, sizeof(hover_hint_buf), "%s: %s",
                                 items_list[it_idx].name, items_list[it_idx].description);
                        hover_hint = hover_hint_buf;
                    }
                }

                int chosen_skill = 0;
                int chosen_item = 0;

                if (clicked) {
                    if (max_skill_pages > 1 && CheckCollisionPointRec(mouse, skill_page_btn)) {
                        skill_page = (skill_page + 1) % max_skill_pages;
                    } else if (max_item_pages > 1 && CheckCollisionPointRec(mouse, item_page_btn)) {
                        item_page = (item_page + 1) % max_item_pages;
                    } else {
                        for (int i = 0; i < 6; i++) {
                            int sk_idx = skill_page * 6 + i;
                            if (sk_idx < total_skills && CheckCollisionPointRec(mouse, btn_skill[i])) {
                                chosen_skill = GetSkill(sk_idx)->id;
                                break;
                            }
                        }
                        if (chosen_skill == 0) {
                            for (int i = 0; i < 4; i++) {
                                int it_idx = item_page * 4 + i;
                                if (it_idx < total_items && CheckCollisionPointRec(mouse, btn_item[i])) {
                                    chosen_item = items_list[it_idx].id;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    int key_slot = -1;
                    if (IsKeyPressed(KEY_ONE)) key_slot = 0;
                    else if (IsKeyPressed(KEY_TWO)) key_slot = 1;
                    else if (IsKeyPressed(KEY_THREE)) key_slot = 2;
                    else if (IsKeyPressed(KEY_FOUR)) key_slot = 3;
                    else if (IsKeyPressed(KEY_FIVE)) key_slot = 4;
                    else if (IsKeyPressed(KEY_SIX)) key_slot = 5;
                    if (key_slot >= 0) {
                        int sk_idx = skill_page * 6 + key_slot;
                        if (sk_idx < total_skills) chosen_skill = GetSkill(sk_idx)->id;
                    }

                    int item_slot = -1;
                    if (IsKeyPressed(KEY_Q)) item_slot = 0;
                    else if (IsKeyPressed(KEY_W)) item_slot = 1;
                    else if (IsKeyPressed(KEY_E)) item_slot = 2;
                    else if (IsKeyPressed(KEY_R)) item_slot = 3;
                    if (item_slot >= 0 && chosen_skill == 0) {
                        int it_idx = item_page * 4 + item_slot;
                        if (it_idx < total_items) chosen_item = items_list[it_idx].id;
                    }
                }

                bool action_taken = false;
                if (chosen_skill > 0) {
                    action_taken = ExecutePlayerAction(&battle, chosen_skill, se_hit, se_heal);
                    if (action_taken) {
                        if (chosen_skill == 4 && se_ougi.frameCount > 0) {
                            PlaySound(se_ougi);
                            ougi_fx_timer = 0.70f;
                        }
                        active_attacker = (chosen_skill == 4) ? 3 : ((chosen_skill - 1) % MAX_PARTY_MEMBERS);
                        player_lunge_timer = 0.28f;
                    }
                } else if (chosen_item > 0) {
                    action_taken = UseBattleItem(&battle, chosen_item, se_heal, se_hit);
                    if (action_taken) {
                        active_attacker = 1;
                        player_lunge_timer = 0.20f;
                    }
                }

                if (action_taken) {
                    if (battle.last_player_dmg > 0) {
                        char dstr[40];
                        if (battle.last_was_break) {
                            snprintf(dstr, sizeof(dstr), "BREAK! -%d", battle.last_player_dmg);
                        } else if (battle.last_was_crit) {
                            snprintf(dstr, sizeof(dstr), "CRIT! -%d", battle.last_player_dmg);
                        } else {
                            snprintf(dstr, sizeof(dstr), "-%d", battle.last_player_dmg);
                        }
                        Color dcol = battle.last_was_break ? (Color){0, 240, 255, 255}
                                   : (battle.last_was_crit ? GOLD : WHITE);
                        SpawnFloatingText(floats, (Vector2){695, 108}, dstr, dcol);
                        enemy_hit_timer = 0.30f;
                        screen_shake_timer = (battle.last_was_crit || battle.last_was_break) ? 0.24f : 0.10f;
                    }
                    if (battle.last_player_heal > 0) {
                        char hstr[32];
                        snprintf(hstr, sizeof(hstr), "+%d HP", battle.last_player_heal);
                        SpawnFloatingText(floats, (Vector2){200, 108}, hstr, (Color){100, 255, 160, 255});
                        heal_fx_timer = 0.45f;
                    }
                    if (chosen_skill == 5 || chosen_item == 4) {
                        SpawnFloatingText(floats, (Vector2){210, 128}, "SHIELD!", SKYBLUE);
                        heal_fx_timer = 0.35f;
                    }

                    if (battle.enemy_hp <= 0) {
                        battle.enemy_hp = 0;
                        state = (battle.current_chapter_idx >= GetTotalChapters() - 1) ? STATE_EPILOGUE : STATE_CHAPTER_CLEAR;
                    } else {
                        pending_enemy_turn = true;
                        enemy_turn_delay = 0.45f;
                    }
                }
            }
        } else if (state == STATE_CHAPTER_CLEAR) {
            Rectangle next_btn = {240, 502, 480, 56};
            if ((clicked && CheckCollisionPointRec(mouse, next_btn)) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                int next_ch = battle.current_chapter_idx + 1;
                UnloadWatchedTexture(&enemy_tex);
                UnloadWatchedTexture(&bg_tex);
                pending_enemy_turn = false;
                InitBattle(&battle, next_ch);
                cur_ch = GetChapter(next_ch);
                UpdateWatchedSprite(&enemy_tex, cur_ch->asset_file, true);
                UpdateWatchedSprite(&bg_tex, cur_ch->bg_asset_file, false);
                state = STATE_BATTLE;
            }
        } else if (state == STATE_GAME_OVER) {
            Rectangle retry_btn = {240, 502, 480, 56};
            if ((clicked && CheckCollisionPointRec(mouse, retry_btn)) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                pending_enemy_turn = false;
                InitBattle(&battle, battle.current_chapter_idx);
                ResetItems();
                state = STATE_BATTLE;
            }
        } else if (state == STATE_EPILOGUE) {
            Rectangle replay_btn = {320, 520, 320, 48};
            if ((clicked && CheckCollisionPointRec(mouse, replay_btn)) || IsKeyPressed(KEY_R)) {
                UnloadWatchedTexture(&enemy_tex);
                UnloadWatchedTexture(&bg_tex);
                pending_enemy_turn = false;
                ResetItems();
                InitBattle(&battle, 0);
                cur_ch = GetChapter(0);
                UpdateWatchedSprite(&enemy_tex, cur_ch->asset_file, true);
                UpdateWatchedSprite(&bg_tex, cur_ch->bg_asset_file, false);
                state = STATE_BATTLE;
            }
        }

        float shake_x = 0.0f, shake_y = 0.0f;
        if (screen_shake_timer > 0.0f) {
            shake_x = (float)((rand() % 9) - 4);
            shake_y = (float)((rand() % 7) - 3);
        }

        BeginDrawing();
        ClearBackground((Color){16, 20, 28, 255});

        Rectangle stage = {16 + shake_x, 12 + shake_y, 928, 258};
        if (bg_tex.loaded) {
            DrawTexturePro(bg_tex.texture,
                           (Rectangle){0, 0, (float)bg_tex.texture.width, (float)bg_tex.texture.height},
                           stage, (Vector2){0, 0}, 0.0f, WHITE);
            DrawRectangleGradientV((int)stage.x, (int)(stage.y + 150), (int)stage.width, 108,
                                   BLANK, Fade(BLACK, 0.42f));
        } else {
            DrawProceduralStageBg(battle.current_chapter_idx, stage, now);
        }

        if (ougi_fx_timer > 0.0f) {
            DrawRectangleRec(stage, Fade(GOLD, 0.25f * (ougi_fx_timer / 0.70f)));
        }
        DrawRectangleLinesEx(stage, 2.0f, (Color){72, 88, 114, 255});

        DrawRectangle((int)stage.x, (int)stage.y, (int)stage.width, 36, Fade((Color){10, 14, 24, 255}, 0.82f));
        DrawLine((int)stage.x, (int)stage.y + 36, (int)(stage.x + stage.width), (int)stage.y + 36,
                 (Color){0, 210, 255, 100});
        DrawTextEx(jp_font, cur_ch->title, (Vector2){stage.x + 16, stage.y + 8}, 20, 1.0f, RAYWHITE);

        char turn_buf[32];
        snprintf(turn_buf, sizeof(turn_buf), "TURN %d", battle.turn_count);
        DrawRectangle((int)(stage.x + stage.width - 106), (int)stage.y + 6, 92, 24, (Color){26, 40, 62, 255});
        DrawRectangleLines((int)(stage.x + stage.width - 106), (int)stage.y + 6, 92, 24, (Color){0, 240, 255, 160});
        DrawTextEx(jp_font, turn_buf, (Vector2){stage.x + stage.width - 92, stage.y + 9}, 16, 1.0f,
                   (Color){0, 240, 255, 255});

        const float party_x[MAX_PARTY_MEMBERS] = {112, 224, 336};
        const float party_y[MAX_PARTY_MEMBERS] = {220, 232, 218};
        for (int i = 0; i < MAX_PARTY_MEMBERS; i++) {
            const PartyMember *mem = GetPartyMember(i);
            float bob = sinf(now * 3.4f + i * 1.4f) * 3.0f;
            float lunge = 0.0f;
            if (player_lunge_timer > 0.0f && (active_attacker == i || active_attacker == 3)) {
                lunge = sinf((player_lunge_timer / 0.28f) * 3.14159f) * 26.0f;
            }
            float hit_jit = (party_hit_timer > 0.0f) ? sinf(now * 55.0f) * 5.0f : 0.0f;

            float cx = party_x[i] + shake_x + lunge + hit_jit;
            float by = party_y[i] + shake_y + bob;

            DrawEllipse((int)cx, (int)(party_y[i] + shake_y + 4), 34, 9, Fade(BLACK, 0.45f));

            if (battle.party_shield_turns > 0) {
                DrawCircleLines((int)cx, (int)(by - 40), 46.0f, Fade(SKYBLUE, 0.55f));
            }
            if (heal_fx_timer > 0.0f) {
                DrawCircleGradient((int)cx, (int)(by - 38), 48.0f, Fade(GREEN, 0.30f), BLANK);
            }

            Color sprite_tint = WHITE;
            if (party_hit_timer > 0.0f && ((int)(party_hit_timer * 30) % 2 == 0)) {
                sprite_tint = (Color){255, 110, 110, 255};
            }

            if (party_tex[i].loaded) {
                DrawTexturePro(party_tex[i].texture,
                               (Rectangle){0, 0, (float)party_tex[i].texture.width, (float)party_tex[i].texture.height},
                               (Rectangle){cx - 46, by - 92, 92, 92},
                               (Vector2){0, 0}, 0.0f, sprite_tint);
            } else {
                DrawPixelCat(i, cx, by, 5.0f, sprite_tint);
            }

            DrawRectangle((int)(cx - 40), (int)(party_y[i] + shake_y + 8), 80, 22, Fade((Color){12, 16, 26, 255}, 0.85f));
            DrawRectangleLines((int)(cx - 40), (int)(party_y[i] + shake_y + 8), 80, 22, Fade(mem->color, 0.8f));
            Vector2 name_sz = MeasureTextEx(jp_font, mem->name, 14.0f, 1.0f);
            DrawTextEx(jp_font, mem->name,
                       (Vector2){cx - name_sz.x * 0.5f, party_y[i] + shake_y + 11}, 14, 1.0f, mem->color);
        }

        float ex = 720.0f + shake_x;
        float ey = 216.0f + shake_y;
        float enemy_hover = sinf(now * 2.6f) * 4.0f;
        float enemy_jit = (enemy_hit_timer > 0.0f) ? sinf(now * 60.0f) * 8.0f : 0.0f;

        DrawEllipse((int)ex, (int)(ey + 4), 64, 14, Fade(BLACK, 0.50f));

        Color enemy_tint = WHITE;
        if (enemy_hit_timer > 0.0f && ((int)(enemy_hit_timer * 32) % 2 == 0)) {
            enemy_tint = (Color){255, 160, 160, 255};
        } else if (battle.enemy_poison_turns > 0) {
            enemy_tint = (Color){220, 175, 255, 255};
        }

        if (enemy_tex.loaded) {
            DrawTexturePro(enemy_tex.texture,
                           (Rectangle){0, 0, (float)enemy_tex.texture.width, (float)enemy_tex.texture.height},
                           (Rectangle){ex + enemy_jit - 78, ey + enemy_hover - 156, 156, 156},
                           (Vector2){0, 0}, 0.0f, enemy_tint);
        } else {
            DrawPixelEnemy(battle.current_chapter_idx, ex + enemy_jit, ey + enemy_hover, 8.5f,
                           battle.enemy_charging, now, enemy_tint);
        }

        if (battle.enemy_charging) {
            float warn_pulse = (sinf(now * 8.0f) + 1.0f) * 0.5f;
            Rectangle warn_box = {ex - 105, stage.y + 42, 210, 24};
            DrawRectangleRec(warn_box, Fade((Color){180, 20, 20, 255}, 0.88f));
            DrawRectangleLinesEx(warn_box, 1.5f, (Color){255, (unsigned char)(180 + 75 * warn_pulse), 60, 255});
            DrawTextEx(jp_font, "⚠ 必殺吸引充填中！", (Vector2){warn_box.x + 18, warn_box.y + 4}, 15, 1.0f, YELLOW);
        }

        DrawRectangle((int)(ex - 105), (int)(ey + 10), 210, 24, Fade((Color){12, 16, 26, 255}, 0.88f));
        DrawRectangleLines((int)(ex - 105), (int)(ey + 10), 210, 24, (Color){230, 80, 90, 200});
        Vector2 ename_sz = MeasureTextEx(jp_font, cur_ch->enemy_name, 15.0f, 1.0f);
        DrawTextEx(jp_font, cur_ch->enemy_name, (Vector2){ex - ename_sz.x * 0.5f, ey + 14}, 15, 1.0f, WHITE);

        for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
            if (!floats[i].active) continue;
            float alpha = 1.0f - (floats[i].timer / floats[i].duration);
            if (alpha < 0.0f) alpha = 0.0f;
            DrawTextEx(jp_font, floats[i].text, (Vector2){floats[i].pos.x + 2, floats[i].pos.y + 2},
                       22, 1.0f, Fade(BLACK, alpha));
            DrawTextEx(jp_font, floats[i].text, floats[i].pos, 22, 1.0f, Fade(floats[i].color, alpha));
        }

        Rectangle hud_party = {16, 278, 316, 56};
        Rectangle hud_tens  = {340, 278, 272, 56};
        Rectangle hud_enemy = {620, 278, 324, 56};

        DrawRectangleRec(hud_party, (Color){24, 30, 42, 255});
        DrawRectangleLinesEx(hud_party, 1.0f, (Color){52, 66, 88, 255});
        char hp_buf[64];
        snprintf(hp_buf, sizeof(hp_buf), "味方HP: %d / %d", battle.party_hp, battle.party_max_hp);
        float hp_ratio = (battle.party_max_hp > 0) ? (float)battle.party_hp / (float)battle.party_max_hp : 0.0f;
        Color hp_col = (hp_ratio > 0.5f) ? (Color){46, 213, 115, 255}
                     : (hp_ratio > 0.25f ? ORANGE : RED);
        DrawTextEx(jp_font, hp_buf, (Vector2){hud_party.x + 12, hud_party.y + 7}, 16, 1.0f, RAYWHITE);
        if (battle.party_shield_turns > 0) {
            char sh_buf[32];
            snprintf(sh_buf, sizeof(sh_buf), "盾:%dT", battle.party_shield_turns);
            DrawTextEx(jp_font, sh_buf, (Vector2){hud_party.x + 245, hud_party.y + 7}, 15, 1.0f, SKYBLUE);
        }
        DrawProgressBar((Rectangle){hud_party.x + 12, hud_party.y + 30, 292, 16}, hp_ratio,
                        hp_col, (Color){12, 16, 24, 255}, (Color){70, 88, 110, 255});

        DrawRectangleRec(hud_tens, (Color){24, 30, 42, 255});
        DrawRectangleLinesEx(hud_tens, 1.0f, (battle.tension >= 100) ? GOLD : (Color){52, 66, 88, 255});
        char ten_buf[64];
        snprintf(ten_buf, sizeof(ten_buf), "テンション: %d%%", battle.tension);
        DrawTextEx(jp_font, ten_buf, (Vector2){hud_tens.x + 12, hud_tens.y + 7}, 16, 1.0f,
                   (battle.tension >= 100) ? GOLD : (Color){255, 183, 77, 255});
        if (battle.tension >= 100) {
            DrawTextEx(jp_font, "★奥義OK!", (Vector2){hud_tens.x + 178, hud_tens.y + 7}, 15, 1.0f, YELLOW);
        }
        DrawProgressBar((Rectangle){hud_tens.x + 12, hud_tens.y + 30, 248, 16}, (float)battle.tension / 100.0f,
                        (battle.tension >= 100) ? GOLD : ORANGE,
                        (Color){12, 16, 24, 255}, (Color){70, 88, 110, 255});

        DrawRectangleRec(hud_enemy, (Color){24, 30, 42, 255});
        DrawRectangleLinesEx(hud_enemy, 1.0f, (Color){52, 66, 88, 255});
        char ehp_buf[64];
        snprintf(ehp_buf, sizeof(ehp_buf), "敵HP: %d / %d", battle.enemy_hp, battle.enemy_max_hp);
        float ehp_ratio = (battle.enemy_max_hp > 0) ? (float)battle.enemy_hp / (float)battle.enemy_max_hp : 0.0f;
        DrawTextEx(jp_font, ehp_buf, (Vector2){hud_enemy.x + 12, hud_enemy.y + 7}, 16, 1.0f, (Color){255, 120, 120, 255});
        if (battle.enemy_poison_turns > 0) {
            char ps_buf[32];
            snprintf(ps_buf, sizeof(ps_buf), "毒:%dT", battle.enemy_poison_turns);
            DrawTextEx(jp_font, ps_buf, (Vector2){hud_enemy.x + 252, hud_enemy.y + 7}, 15, 1.0f, (Color){200, 120, 255, 255});
        }
        DrawProgressBar((Rectangle){hud_enemy.x + 12, hud_enemy.y + 30, 300, 16}, ehp_ratio,
                        (Color){235, 68, 78, 255}, (Color){12, 16, 24, 255}, (Color){70, 88, 110, 255});

        Rectangle log_box = {16, 342, 928, 104};
        DrawRectangleRec(log_box, (Color){13, 17, 25, 255});
        DrawRectangleLinesEx(log_box, 1.5f, (Color){62, 78, 102, 255});
        int lcount = GetLogCount();
        int start_idx = (lcount > 4) ? (lcount - 4) : 0;
        for (int i = start_idx; i < lcount; i++) {
            const char *line = GetLogLine(i);
            int row = i - start_idx;
            Color lcol = (Color){180, 238, 180, 255};
            if (strstr(line, "警告") || strstr(line, "充填中")) lcol = (Color){255, 210, 90, 255};
            else if (strstr(line, "奥義") || strstr(line, "会心") || strstr(line, "ブレイク")) lcol = GOLD;
            else if (strstr(line, "回復") || strstr(line, "シールド")) lcol = (Color){120, 235, 255, 255};
            else if (strstr(line, "不足") || strstr(line, "残っていません")) lcol = (Color){255, 120, 120, 255};

            DrawTextEx(jp_font, line, (Vector2){log_box.x + 16, log_box.y + 10 + row * 22}, 16, 1.0f, lcol);
        }

        Rectangle cmd_panel = {16, 454, 928, 174};
        DrawRectangleRec(cmd_panel, (Color){20, 25, 35, 255});
        DrawRectangleLinesEx(cmd_panel, 1.5f, (Color){54, 68, 92, 255});

        if (state == STATE_BATTLE) {
            DrawTextEx(jp_font, "◆ スキル (1〜6)", (Vector2){26, 458}, 14, 1.0f, (Color){255, 200, 100, 255});
            DrawTextEx(jp_font, "◆ アイテム (Q/W/E/R)", (Vector2){624, 458}, 14, 1.0f, (Color){130, 210, 255, 255});

            if (max_skill_pages > 1) {
                char pbuf[24];
                snprintf(pbuf, sizeof(pbuf), "頁 %d/%d ▶", skill_page + 1, max_skill_pages);
                bool phov = CheckCollisionPointRec(mouse, skill_page_btn);
                DrawRectangleRec(skill_page_btn, phov ? (Color){64, 84, 118, 255} : (Color){40, 52, 74, 255});
                DrawRectangleLinesEx(skill_page_btn, 1.0f, (Color){120, 160, 210, 255});
                DrawTextEx(jp_font, pbuf, (Vector2){skill_page_btn.x + 8, skill_page_btn.y + 3}, 12, 1.0f, WHITE);
            }
            if (max_item_pages > 1) {
                char ibuf_p[24];
                snprintf(ibuf_p, sizeof(ibuf_p), "頁 %d/%d ▶", item_page + 1, max_item_pages);
                bool phov = CheckCollisionPointRec(mouse, item_page_btn);
                DrawRectangleRec(item_page_btn, phov ? (Color){64, 84, 118, 255} : (Color){40, 52, 74, 255});
                DrawRectangleLinesEx(item_page_btn, 1.0f, (Color){120, 160, 210, 255});
                DrawTextEx(jp_font, ibuf_p, (Vector2){item_page_btn.x + 8, item_page_btn.y + 3}, 12, 1.0f, WHITE);
            }

            static const Color skill_colors[6] = {
                {235, 160, 48, 255},
                {235, 112, 82, 255},
                {178, 108, 214, 255},
                {220, 48, 56, 255},
                {58, 162, 224, 255},
                {224, 102, 162, 255}
            };

            for (int i = 0; i < 6; i++) {
                int sk_idx = skill_page * 6 + i;
                if (sk_idx >= total_skills) {
                    DrawRectangleRec(btn_skill[i], (Color){26, 30, 40, 255});
                    DrawRectangleLinesEx(btn_skill[i], 1.0f, (Color){42, 48, 62, 255});
                    continue;
                }
                const Skill *sk = GetSkill(sk_idx);
                bool usable = !pending_enemy_turn && (battle.tension >= sk->tension_cost);
                bool hovered = CheckCollisionPointRec(mouse, btn_skill[i]);
                Color base_c = skill_colors[sk_idx % 6];
                Color bg = usable ? base_c : (Color){58, 62, 72, 255};
                if (usable && hovered) {
                    bg = (Color){
                        (unsigned char)(bg.r + (255 - bg.r) / 4),
                        (unsigned char)(bg.g + (255 - bg.g) / 4),
                        (unsigned char)(bg.b + (255 - bg.b) / 4),
                        255
                    };
                }
                DrawRectangleRec(btn_skill[i], bg);
                DrawRectangleLinesEx(btn_skill[i], hovered ? 2.0f : 1.0f, hovered ? WHITE : Fade(BLACK, 0.35f));

                bool dark_bg_text = (sk->id == 4);
                Color txt_c = usable ? (dark_bg_text ? WHITE : (Color){18, 20, 26, 255}) : (Color){150, 156, 168, 255};
                Color sub_c = usable ? (dark_bg_text ? (Color){255, 235, 190, 255} : (Color){36, 38, 48, 255})
                                     : (Color){120, 125, 136, 255};

                char sk_title[64];
                snprintf(sk_title, sizeof(sk_title), "[%d] %s", i + 1, sk->name);
                char sk_sub[64];
                if (sk->tension_cost > 0) {
                    const char *tag = (sk->effect == STATUS_SHIELD) ? " / 軽減"
                                    : (sk->effect == STATUS_POISON ? " / 毒"
                                    : (sk->id == 4 ? " / 奥義" : ""));
                    snprintf(sk_sub, sizeof(sk_sub), "必要TN:%d%%%s", sk->tension_cost, tag);
                } else {
                    const char *tag = (sk->heal_val > 0) ? " / 回復"
                                    : (sk->id == 2 ? " / 妨害" : " / 攻撃");
                    snprintf(sk_sub, sizeof(sk_sub), "獲得TN:+%d%%%s", sk->tension_gain, tag);
                }
                DrawTextWrapped(jp_font, sk_title, (Rectangle){btn_skill[i].x + 8, btn_skill[i].y + 5, 170, 18},
                                14.0f, 16.0f, 1, txt_c);
                DrawTextWrapped(jp_font, sk_sub, (Rectangle){btn_skill[i].x + 10, btn_skill[i].y + 23, 168, 16},
                                12.0f, 14.0f, 1, sub_c);
            }

            Item *items = GetItemList();
            static const char *item_keys[4] = {"Q", "W", "E", "R"};
            for (int i = 0; i < 4; i++) {
                int it_idx = item_page * 4 + i;
                if (it_idx >= total_items) {
                    DrawRectangleRec(btn_item[i], (Color){24, 28, 38, 255});
                    DrawRectangleLinesEx(btn_item[i], 1.0f, (Color){40, 48, 62, 255});
                    continue;
                }
                bool usable = !pending_enemy_turn && (items[it_idx].count > 0);
                bool hovered = CheckCollisionPointRec(mouse, btn_item[i]);
                Color bg = usable ? (hovered ? (Color){68, 98, 128, 255} : (Color){46, 68, 92, 255})
                                  : (Color){34, 38, 46, 255};
                DrawRectangleRec(btn_item[i], bg);
                DrawRectangleLinesEx(btn_item[i], hovered ? 2.0f : 1.0f,
                                     hovered ? SKYBLUE : (Color){78, 102, 130, 255});

                char ititle[64];
                snprintf(ititle, sizeof(ititle), "[%s] %s", item_keys[i], items[it_idx].name);
                char icnt[32];
                if (items[it_idx].count > 0) snprintf(icnt, sizeof(icnt), "残り %d 個", items[it_idx].count);
                else snprintf(icnt, sizeof(icnt), "在庫なし");

                Color itxt = usable ? (Color){225, 242, 255, 255} : GRAY;
                Color icnt_c = usable ? (Color){155, 215, 255, 255} : (Color){110, 115, 125, 255};
                DrawTextWrapped(jp_font, ititle, (Rectangle){btn_item[i].x + 8, btn_item[i].y + 5, 138, 18},
                                13.0f, 15.0f, 1, itxt);
                DrawTextEx(jp_font, icnt, (Vector2){btn_item[i].x + 10, btn_item[i].y + 23}, 12, 1.0f, icnt_c);
            }

            DrawRectangle(24, 578, 912, 40, (Color){13, 17, 24, 255});
            DrawRectangleLines(24, 578, 912, 40, (Color){48, 60, 82, 255});
            if (pending_enemy_turn) {
                DrawTextEx(jp_font, "敵の行動中...", (Vector2){36, 589}, 16, 1.0f, ORANGE);
            } else {
                DrawTextEx(jp_font, hover_hint, (Vector2){36, 589}, 15, 1.0f, (Color){205, 222, 242, 255});
            }
        } else if (state == STATE_CHAPTER_CLEAR) {
            DrawTextEx(jp_font, "VICTORY! ステージ撃破成功！", (Vector2){340, 468}, 20, 1.0f, (Color){120, 255, 150, 255});
            Rectangle next_btn = {240, 502, 480, 56};
            bool hov = CheckCollisionPointRec(mouse, next_btn);
            DrawRectangleRec(next_btn, hov ? (Color){62, 158, 68, 255} : (Color){46, 125, 50, 255});
            DrawRectangleLinesEx(next_btn, 2.0f, (Color){170, 255, 180, 255});
            DrawTextEx(jp_font, "次の章へ進む（クリック / Space）", (Vector2){345, 520}, 18, 1.0f, WHITE);
        } else if (state == STATE_GAME_OVER) {
            DrawTextEx(jp_font, "DEFEAT... 猫パーティは力尽きた……", (Vector2){315, 468}, 20, 1.0f, (Color){255, 130, 130, 255});
            Rectangle retry_btn = {240, 502, 480, 56};
            bool hov = CheckCollisionPointRec(mouse, retry_btn);
            DrawRectangleRec(retry_btn, hov ? (Color){215, 42, 42, 255} : (Color){175, 28, 28, 255});
            DrawRectangleLinesEx(retry_btn, 2.0f, (Color){255, 180, 180, 255});
            DrawTextEx(jp_font, "この章を再挑戦する（クリック / Space）", (Vector2){330, 520}, 18, 1.0f, WHITE);
        } else if (state == STATE_EPILOGUE) {
            DrawRectangle(40, 42, 880, 556, (Color){12, 18, 28, 246});
            DrawRectangleLinesEx((Rectangle){40, 42, 880, 556}, 2.0f, GOLD);

            DrawTextEx(jp_font, "【大団円：明かされた真実】", (Vector2){72, 68}, 24, 1.0f, GOLD);
            int ep_count = GetEpilogueLineCount();
            float ey_cursor = 118.0f;
            for (int i = 0; i < ep_count; i++) {
                int drawn = DrawTextWrapped(jp_font, GetEpilogueLine(i),
                                            (Rectangle){72, ey_cursor, 816, 80},
                                            17.0f, 24.0f, 3, (Color){238, 244, 252, 255});
                ey_cursor += drawn * 24.0f + 16.0f;
            }

            Rectangle replay_btn = {320, 520, 320, 48};
            bool hov = CheckCollisionPointRec(mouse, replay_btn);
            DrawRectangleRec(replay_btn, hov ? (Color){220, 165, 45, 255} : (Color){185, 135, 32, 255});
            DrawRectangleLinesEx(replay_btn, 2.0f, YELLOW);
            DrawTextEx(jp_font, "最初からプレイ（クリック / R）", (Vector2){365, 535}, 17, 1.0f, BLACK);
        }

        DrawAiMonitor(&ai_monitor, jp_font);
        EndDrawing();
    }

    UnloadWatchedTexture(&enemy_tex);
    UnloadWatchedTexture(&bg_tex);
    for (int i = 0; i < MAX_PARTY_MEMBERS; i++) {
        UnloadWatchedTexture(&party_tex[i]);
    }
    if (se_hit.frameCount > 0) UnloadSound(se_hit);
    if (se_heal.frameCount > 0) UnloadSound(se_heal);
    if (se_ougi.frameCount > 0) UnloadSound(se_ougi);
    UnloadGameFont(&font_ctx);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
