#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "common.h"

#define MAX_PARTY_MEMBERS 3

// パーティメンバー情報
typedef struct {
    int id;
    const char *name;
    const char *title;
    const char *asset_file;
    Color color;
} PartyMember;

// チャプター一覧の取得
const Chapter* GetChapter(int index);
int GetTotalChapters(void);

// パーティメンバー一覧の取得
const PartyMember* GetPartyMember(int index);

// スキル一覧の取得
const Skill* GetSkill(int index);
int GetTotalSkills(void);

// アイテム一覧の取得
Item* GetItemList(void);
int GetTotalItems(void);
void ResetItems(void);

// エピローグテキストの取得
int GetEpilogueLineCount(void);
const char* GetEpilogueLine(int index);

#endif // GAME_DATA_H
