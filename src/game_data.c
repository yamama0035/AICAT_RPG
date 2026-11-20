#include "game_data.h"

static const Chapter CHAPTERS[] = {
    {1, "第1章：ひだまりの街と侵略者の影",
     "キャットピアの広場にお掃除メカ兵が出現！\n街の毛玉を吸い込む機械を撃退せよ。",
     "お掃除メカ兵", 180, 16, "assets/enemy_sweeper.png", "assets/bg_ch1.png", (Color){232, 245, 233, 255}},
    {2, "第2章：迷いの森と三毛・黒猫の出会い",
     "迷いの森でミケ・クロと合流！\n3匹の連携で暴走モップドローンをお仕置きしろ！",
     "暴走モップドローン", 340, 26, "assets/enemy_mop.png", "assets/bg_ch2.png", (Color){200, 230, 201, 255}},
    {3, "第3章：マタタビ神社と黄金の爪研ぎ",
     "神器『黄金の爪研ぎ』を授かった。\n重装スチームクリーナーの装甲を切り裂け！",
     "重装スチームクリーナー", 550, 38, "assets/enemy_cleaner.png", "assets/bg_ch3.png", (Color){255, 249, 196, 255}},
    {4, "第4章：侵略母船バキューム・コア潜入",
     "敵母船へ突入。動力室を守る\n幹部「吸引将軍ダストロン」を打ち倒せ！",
     "吸引将軍ダストロン", 820, 52, "assets/enemy_dustron.png", "assets/bg_ch4.png", (Color){207, 216, 220, 255}},
    {5, "第5章：最終決戦！ 皇帝メガ・バキューム",
     "全宇宙吸引モードを起動した皇帝メガ・バキューム！\n合体奥義『超・黄金毛玉乱舞』で決着をつけろ！",
     "皇帝メガ・バキューム", 1400, 70, "assets/boss_megavacuum.png", "assets/bg_ch5.png", (Color){255, 205, 210, 255}}
};

static const PartyMember PARTY[MAX_PARTY_MEMBERS] = {
    {0, "とらねこ", "熱血アタッカー", "assets/party_tora.png", (Color){255, 183, 77, 255}},
    {1, "ミケ",     "俊足サポーター", "assets/party_mike.png", (Color){255, 138, 101, 255}},
    {2, "クロ",     "神秘の魔術師",   "assets/party_kuro.png", (Color){186, 104, 200, 255}}
};

static const Skill SKILLS[] = {
    {1, "黄金の爪研ぎ", "装甲を裂く強力な一撃", 45, 70, 0, 0, 15, STATUS_NONE},
    {2, "猫だまし", "電光石火の妨害攻撃（敵の充填を解除）", 25, 40, 0, 0, 20, STATUS_NONE},
    {3, "マタタビ魔法", "味方HP回復＆敵に精神ダメージ", 20, 35, 45, 0, 15, STATUS_NONE},
    {4, "超・毛玉乱舞", "3匹の絆で放つ最大奥義（テンション100消費）", 300, 380, 0, 100, 0, STATUS_NONE},
    {5, "毛玉シールド", "次ターン受けるダメージを50%軽減", 0, 0, 0, 30, 0, STATUS_SHIELD},
    {6, "猫パンチラッシュ", "素早い連続打撃と毒付与", 35, 55, 0, 20, 10, STATUS_POISON}
};

static Item ITEMS[] = {
    {1, "高級カリカリ缶詰", "味方全体のHPを150回復", 150, 2},
    {2, "マタタビ酒", "テンションを一気に+50%上昇", 0, 2},
    {3, "聖なる毛玉ボム", "敵に防御無視の120ダメージ", 0, 1},
    {4, "特製マタタビ茶", "HPを80回復し2ターンシールド展開", 80, 2}
};

static int saved_item_counts[64];
static bool item_counts_saved = false;

static void EnsureInitialItemCountsSaved(void) {
    if (item_counts_saved) return;
    int n = GetTotalItems();
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) {
        saved_item_counts[i] = ITEMS[i].count;
    }
    item_counts_saved = true;
}

static const char *EPILOGUE_LINES[] = {
    "戦闘後、音声翻訳機が修復された皇帝メガ・バキュームが語り出す。",
    "「我ら掃除機星人にとって、猫たちの『ブラッシング毛玉』こそが全宇宙最高のクリーンエネルギー源だったのだニャ！」",
    "街を滅ぼす意図はなく、回収に熱が入りすぎて街をパニックに陥れていただけであった。",
    "事情を理解したミケが「それなら毎日の抜け毛をあげるニャ！」と特大毛玉をプレゼント。",
    "皇帝メガ・バキュームは歓喜し、お詫びとしてキャットピア全土をピカピカに清掃して宇宙へ帰還した。",
    "ポカポカ陽気の日向ぼっこ広場で、3匹は寄り添いながら穏やかな昼寝を楽しんだ。（完）"
};

const Chapter* GetChapter(int index) {
    int total = GetTotalChapters();
    if (index < 0 || index >= total) return &CHAPTERS[0];
    return &CHAPTERS[index];
}

int GetTotalChapters(void) {
    return (int)(sizeof(CHAPTERS) / sizeof(CHAPTERS[0]));
}

const PartyMember* GetPartyMember(int index) {
    if (index < 0 || index >= MAX_PARTY_MEMBERS) return &PARTY[0];
    return &PARTY[index];
}

const Skill* GetSkill(int index) {
    int total = GetTotalSkills();
    if (index < 0 || index >= total) return &SKILLS[0];
    return &SKILLS[index];
}

int GetTotalSkills(void) {
    return (int)(sizeof(SKILLS) / sizeof(SKILLS[0]));
}

Item* GetItemList(void) {
    EnsureInitialItemCountsSaved();
    return ITEMS;
}

int GetTotalItems(void) {
    return (int)(sizeof(ITEMS) / sizeof(ITEMS[0]));
}

void ResetItems(void) {
    EnsureInitialItemCountsSaved();
    int n = GetTotalItems();
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) {
        ITEMS[i].count = saved_item_counts[i];
    }
}

int GetEpilogueLineCount(void) {
    return (int)(sizeof(EPILOGUE_LINES) / sizeof(EPILOGUE_LINES[0]));
}

const char* GetEpilogueLine(int index) {
    if (index < 0 || index >= GetEpilogueLineCount()) return "";
    return EPILOGUE_LINES[index];
}
