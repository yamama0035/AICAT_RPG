#include "font_loader.h"
#include "game_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int ReadBE32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8)  | (unsigned int)p[3];
}

static unsigned short ReadBE16(const unsigned char *p) {
    return (unsigned short)(((unsigned int)p[0] << 8) | (unsigned int)p[1]);
}

static void WriteBE32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)((v >> 24) & 0xFF);
    p[1] = (unsigned char)((v >> 16) & 0xFF);
    p[2] = (unsigned char)((v >> 8)  & 0xFF);
    p[3] = (unsigned char)(v & 0xFF);
}

static unsigned char* ReadBinaryFile(const char *path, int *out_size) {
    if (!path || !out_size) return NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long len = ftell(fp);
    if (len <= 0 || len > 64 * 1024 * 1024) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    unsigned char *buf = (unsigned char *)malloc((size_t)len);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    if (read_bytes != (size_t)len) {
        free(buf);
        return NULL;
    }
    *out_size = (int)len;
    return buf;
}

// .ttc (TrueType Collection) から先頭フォントを独立した .ttf バッファとして抽出する
static unsigned char* ExtractStandaloneTtf(const unsigned char *raw, int raw_size, int *out_size) {
    if (!raw || raw_size < 16 || !out_size) return NULL;

    if (!(raw[0] == 't' && raw[1] == 't' && raw[2] == 'c' && raw[3] == 'f')) {
        unsigned char *copy = (unsigned char *)malloc((size_t)raw_size);
        if (!copy) return NULL;
        memcpy(copy, raw, (size_t)raw_size);
        *out_size = raw_size;
        return copy;
    }

    unsigned int num_fonts = ReadBE32(raw + 8);
    if (num_fonts == 0) return NULL;
    unsigned int font_offset = ReadBE32(raw + 12);
    if (font_offset > (unsigned int)raw_size - 12) return NULL;

    unsigned short num_tables = ReadBE16(raw + font_offset + 4);
    if (num_tables == 0) return NULL;
    unsigned int header_size = 12u + (unsigned int)num_tables * 16u;
    if (font_offset + header_size > (unsigned int)raw_size) return NULL;

    unsigned int total_size = header_size;
    for (unsigned short i = 0; i < num_tables; i++) {
        const unsigned char *rec = raw + font_offset + 12u + (unsigned int)i * 16u;
        unsigned int tbl_off = ReadBE32(rec + 8);
        unsigned int tbl_len = ReadBE32(rec + 12);
        if (tbl_off > (unsigned int)raw_size || tbl_len > (unsigned int)raw_size - tbl_off) {
            return NULL;
        }
        unsigned int padded = (tbl_len + 3u) & ~3u;
        total_size += padded;
    }

    unsigned char *ttf = (unsigned char *)calloc(1, total_size);
    if (!ttf) return NULL;

    memcpy(ttf, raw + font_offset, 12);
    unsigned int write_pos = header_size;
    for (unsigned short i = 0; i < num_tables; i++) {
        const unsigned char *src_rec = raw + font_offset + 12u + (unsigned int)i * 16u;
        unsigned char *dst_rec = ttf + 12u + (unsigned int)i * 16u;
        unsigned int tbl_off = ReadBE32(src_rec + 8);
        unsigned int tbl_len = ReadBE32(src_rec + 12);

        memcpy(dst_rec, src_rec, 16);
        WriteBE32(dst_rec + 8, write_pos);
        if (tbl_len > 0) {
            memcpy(ttf + write_pos, raw + tbl_off, tbl_len);
        }
        write_pos += (tbl_len + 3u) & ~3u;
    }

    *out_size = (int)total_size;
    return ttf;
}

static bool AddUniqueCodepoint(GameFontContext *ctx, int cp) {
    if (!ctx || cp < 32 || ctx->count >= 4096) return false;
    if (cp < 65536) {
        int byte_idx = cp >> 3;
        unsigned char bit = (unsigned char)(1u << (cp & 7));
        if (ctx->seen_bmp[byte_idx] & bit) return false;
        ctx->seen_bmp[byte_idx] |= bit;
    } else {
        for (int i = 0; i < ctx->count; i++) {
            if (ctx->codepoints[i] == cp) return false;
        }
    }
    ctx->codepoints[ctx->count++] = cp;
    return true;
}

static bool AddCodepointsFromUtf8(GameFontContext *ctx, const char *text) {
    if (!ctx || !text) return false;
    bool added = false;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        int cp = 0;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && p[1]) {
            cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
            cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            p += 3;
        } else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
            cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            p += 4;
        } else {
            p++;
            continue;
        }
        if (AddUniqueCodepoint(ctx, cp)) {
            added = true;
        }
    }
    return added;
}

static void SeedBaseCodepoints(GameFontContext *ctx) {
    for (int i = 0x0020; i <= 0x007E; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0x2010; i <= 0x203B; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0x2190; i <= 0x2199; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0x25A0; i <= 0x25FF; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0x2605; i <= 0x2606; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0x3000; i <= 0x303F; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0x3040; i <= 0x309F; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0x30A0; i <= 0x30FF; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0xFF01; i <= 0xFF5E; i++) AddUniqueCodepoint(ctx, i);
    for (int i = 0xFFE0; i <= 0xFFE5; i++) AddUniqueCodepoint(ctx, i);

    static const char *kanji_set =
        "一二三四五六七八九十百千万億兆上下左右中大小長短高低強弱新古多少早遅明暗"
        "第章節話編幕巻回番目次的確度率値数割合比量質重軽固軟硬剛柔"
        "猫犬鳥魚虫獣人男女王皇皇帝帝将軍兵士勇者魔術師戦士騎士僧侶盗賊賢者精霊守護神"
        "街村森山川海空星宇宙世界国城塔神社寺遺跡洞窟工場船母艦動力室広場基地"
        "ひだまり陽気昼寝笑顔涙絆仲間味方敵対相手存在姿形表情輪郭鮮明自然"
        "黄金銀鉄鋼銅木火水風土雷氷光闇聖邪毒麻痺眠睡凍炎嵐震斬打突射撃砲"
        "爪牙尻尾耳毛玉羽翼角盾剣鎧兜杖弓矢弾爆破炸裂衝撃波連撃乱舞必殺奥義"
        "攻撃防御回復治癒支援強化弱体補助妨害解除充填蓄積発動消費獲得上昇低下"
        "体力命気力精神集中覚醒変身召喚転生復活勝利敗北撃破討伐撤退逃走再戦"
        "挑戦冒険出撃開始終了継続次前前後左右上下内外中央周辺全体単体"
        "掃除機吸引塵埃掃討暴走重装幹部侵略者防衛戦守保護奪還和平平和大団円"
        "完終幕真実謎翻訳音声修理修復整備清掃帰還感謝歓喜贈与特大毎日抜"
        "高級缶詰酒茶薬草爆弾道具装備品所持残個数使用不可不足要必要"
        "思考意図開発活動監視状態待機稼働準備完了成功失敗エラー検知再試行"
        "生成画像描絵背景質感輪郭解像色調明瞭構図修正改善案反映設定変更"
        "分析検討方針計画判断理由詳細記録履歴時刻時間更新読込保存配置接続"
        "命令指示指令人間自動自律制御拡張改修追加実装検証通過安定復元"
        "見表示確認選択操作押クリック入力送信受信処理計算結果変化発生効果"
        "受与奪削切裂貫通直撃致命傷軽減半減無効反射吸収回避命中会心即死"
        "毒付与継続経過進行待機中準備整全開警告注意危険緊急異常正常良好";

    AddCodepointsFromUtf8(ctx, kanji_set);

    int total_ch = GetTotalChapters();
    for (int i = 0; i < total_ch; i++) {
        const Chapter *ch = GetChapter(i);
        AddCodepointsFromUtf8(ctx, ch->title);
        AddCodepointsFromUtf8(ctx, ch->story);
        AddCodepointsFromUtf8(ctx, ch->enemy_name);
    }
    for (int i = 0; i < MAX_PARTY_MEMBERS; i++) {
        const PartyMember *pm = GetPartyMember(i);
        AddCodepointsFromUtf8(ctx, pm->name);
        AddCodepointsFromUtf8(ctx, pm->title);
    }
    int total_sk = GetTotalSkills();
    for (int i = 0; i < total_sk; i++) {
        const Skill *sk = GetSkill(i);
        AddCodepointsFromUtf8(ctx, sk->name);
        AddCodepointsFromUtf8(ctx, sk->description);
    }
    Item *items = GetItemList();
    int total_it = GetTotalItems();
    for (int i = 0; i < total_it; i++) {
        AddCodepointsFromUtf8(ctx, items[i].name);
        AddCodepointsFromUtf8(ctx, items[i].description);
    }
    int ep_lines = GetEpilogueLineCount();
    for (int i = 0; i < ep_lines; i++) {
        AddCodepointsFromUtf8(ctx, GetEpilogueLine(i));
    }
}

static bool RebuildGameFont(GameFontContext *ctx) {
    if (!ctx || !ctx->ttf_data || ctx->ttf_size <= 0) return false;
    Font new_font = LoadFontFromMemory(".ttf", ctx->ttf_data, ctx->ttf_size,
                                       ctx->font_size, ctx->codepoints, ctx->count);
    if (new_font.texture.id == 0 || new_font.glyphCount <= 0) {
        return false;
    }
    SetTextureFilter(new_font.texture, TEXTURE_FILTER_BILINEAR);
    if (ctx->has_custom_font) {
        UnloadFont(ctx->font);
    }
    ctx->font = new_font;
    ctx->has_custom_font = true;
    return true;
}

void InitGameFont(GameFontContext *ctx, const AiMonitorData *ai) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->font = GetFontDefault();
    ctx->font_size = 20;

    SeedBaseCodepoints(ctx);
    if (ai) {
        AddCodepointsFromUtf8(ctx, ai->status);
        AddCodepointsFromUtf8(ctx, ai->current_thought);
        for (int i = 0; i < ai->history_count && i < AI_MONITOR_MAX_HISTORY; i++) {
            AddCodepointsFromUtf8(ctx, ai->history_titles[i]);
            AddCodepointsFromUtf8(ctx, ai->history_details[i]);
        }
    }

    static const char *candidates[] = {
        "assets/font.ttf",
        "C:\\Windows\\Fonts\\meiryo.ttc",
        "C:\\Windows\\Fonts\\YuGothM.ttc",
        "C:\\Windows\\Fonts\\YuGothR.ttc",
        "C:\\Windows\\Fonts\\BIZ-UDGothicR.ttc",
        "C:\\Windows\\Fonts\\msgothic.ttc",
        "C:\\Windows\\Fonts\\yumin.ttf",
        "C:\\Windows\\Fonts\\NotoSansJP-VF.ttf"
    };
    int num_candidates = (int)(sizeof(candidates) / sizeof(candidates[0]));

    for (int i = 0; i < num_candidates; i++) {
        if (!FileExists(candidates[i])) continue;
        int raw_size = 0;
        unsigned char *raw = ReadBinaryFile(candidates[i], &raw_size);
        if (!raw) continue;

        int ttf_size = 0;
        unsigned char *ttf = ExtractStandaloneTtf(raw, raw_size, &ttf_size);
        free(raw);
        if (!ttf) continue;

        ctx->ttf_data = ttf;
        ctx->ttf_size = ttf_size;
        if (RebuildGameFont(ctx)) {
            return;
        }
        free(ctx->ttf_data);
        ctx->ttf_data = NULL;
        ctx->ttf_size = 0;
    }
}

void EnsureFontCoversAiData(GameFontContext *ctx, const AiMonitorData *ai) {
    if (!ctx || !ctx->has_custom_font || !ai) return;
    bool need_rebuild = false;
    if (AddCodepointsFromUtf8(ctx, ai->status)) need_rebuild = true;
    if (AddCodepointsFromUtf8(ctx, ai->current_thought)) need_rebuild = true;
    for (int i = 0; i < ai->history_count && i < AI_MONITOR_MAX_HISTORY; i++) {
        if (AddCodepointsFromUtf8(ctx, ai->history_titles[i])) need_rebuild = true;
        if (AddCodepointsFromUtf8(ctx, ai->history_details[i])) need_rebuild = true;
    }
    if (need_rebuild) {
        RebuildGameFont(ctx);
    }
}

void UnloadGameFont(GameFontContext *ctx) {
    if (!ctx) return;
    if (ctx->has_custom_font) {
        UnloadFont(ctx->font);
        ctx->has_custom_font = false;
    }
    if (ctx->ttf_data) {
        free(ctx->ttf_data);
        ctx->ttf_data = NULL;
        ctx->ttf_size = 0;
    }
}
