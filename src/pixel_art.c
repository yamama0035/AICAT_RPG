#include "pixel_art.h"
#include <math.h>

static Color TintPixel(Color base, Color tint) {
    return (Color){
        (unsigned char)((base.r * tint.r) / 255),
        (unsigned char)((base.g * tint.g) / 255),
        (unsigned char)((base.b * tint.b) / 255),
        (unsigned char)((base.a * tint.a) / 255)
    };
}

static void DrawPixelGrid16(const char *rows[16], float x, float y, float px_size,
                            Color (*palette)(char), Color tint) {
    for (int r = 0; r < 16; r++) {
        const char *row = rows[r];
        for (int c = 0; c < 16 && row[c]; c++) {
            if (row[c] == '.') continue;
            Color col = palette(row[c]);
            if (col.a == 0) continue;
            DrawRectangleRec((Rectangle){x + c * px_size, y + r * px_size, px_size + 0.3f, px_size + 0.3f},
                             TintPixel(col, tint));
        }
    }
}

static Color CatPaletteTora(char ch) {
    switch (ch) {
        case 'K': return (Color){32, 24, 28, 255};
        case 'O': return (Color){245, 158, 52, 255};
        case 'S': return (Color){185, 88, 24, 255};
        case 'W': return (Color){255, 244, 224, 255};
        case 'R': return (Color){220, 48, 56, 255};
        case 'E': return (Color){24, 24, 32, 255};
        case 'P': return (Color){245, 130, 150, 255};
        case 'Y': return (Color){255, 215, 64, 255};
        default:  return BLANK;
    }
}

static Color CatPaletteMike(char ch) {
    switch (ch) {
        case 'K': return (Color){32, 26, 36, 255};
        case 'O': return (Color){255, 248, 240, 255};
        case 'S': return (Color){240, 130, 56, 255};
        case 'W': return (Color){56, 52, 68, 255};
        case 'R': return (Color){52, 168, 235, 255};
        case 'E': return (Color){24, 28, 40, 255};
        case 'P': return (Color){245, 130, 150, 255};
        case 'Y': return (Color){255, 225, 90, 255};
        default:  return BLANK;
    }
}

static Color CatPaletteKuro(char ch) {
    switch (ch) {
        case 'K': return (Color){20, 18, 28, 255};
        case 'O': return (Color){44, 42, 62, 255};
        case 'S': return (Color){128, 66, 200, 255};
        case 'W': return (Color){72, 68, 96, 255};
        case 'R': return (Color){158, 86, 230, 255};
        case 'E': return (Color){255, 215, 48, 255};
        case 'P': return (Color){230, 110, 150, 255};
        case 'Y': return (Color){255, 215, 64, 255};
        default:  return BLANK;
    }
}

void DrawPixelCat(int party_idx, float cx, float bottom_y, float px_size, Color tint) {
    static const char *cat_warrior[16] = {
        "..KK........KK..",
        ".KOOK......KOOK.",
        ".KORKKKKKKKKROK.",
        "KOOSOOOOOOOOOSOK",
        "KOOOSOOOOOOSOOOK",
        "KOOWEKOOOOKWEOOK",
        "KOOWWWPOOPWWWOOK",
        ".KRRRRRRRRRRRRK.",
        "..KOOOWWWWOOOK..",
        ".KOOOOWWWWOOOOK.",
        ".KOOOKWWWWKOOOK.",
        "..KK.KWWWWK.KK..",
        ".....KOOOK.KK...",
        "....KOOOK.KOOK..",
        "....KWWK..KWWK..",
        "....KKKK..KKKK.."
    };
    static const char *cat_calico[16] = {
        "..KK........KK..",
        ".KSSK......KWWK.",
        ".KSWKKKKKKKKWWK.",
        "KSSSOOOOOOOOWWWK",
        "KSSOOOOOOOOOOWWK",
        "KOOWEKOOOOKWEOOK",
        "KOOWWWPOOPWWWOOK",
        ".KRRRRRRRRRRRRK.",
        "..KOOOSWWSSRRK..",
        ".KOOOOSWWSSOOOK.",
        ".KOOOKOOOOOKORK.",
        "..KK.KSOOOSK.KK.",
        ".....KOOOK..KK..",
        "....KOOOK..KOOK.",
        "....KWWK...KSSK.",
        "....KKKK...KKKK."
    };
    static const char *cat_mage[16] = {
        "......KKKK......",
        ".....KSSSSK.....",
        "....KSSSSSSK....",
        "...KSSYYYYSSK...",
        ".KKKKKKKKKKKKKK.",
        "..KOOKKKKKKKOOK.",
        ".KOOOOOOOOOOOOOK",
        ".KOOEOKOOOKOEOK.",
        ".KOOOOOPOOPOOOOK",
        "..KRRRRRRRRRRK..",
        ".KRRRROWWWORRRK.",
        ".KRROOKWWWKOORK.",
        "..KK..KOOOK..KK.",
        ".....KOOOK.KK...",
        "....KWWK..KOOK..",
        "....KKKK..KKKK.."
    };

    float w = 16.0f * px_size;
    float x = cx - w * 0.5f;
    float y = bottom_y - w;

    if (party_idx == 0) {
        DrawPixelGrid16(cat_warrior, x, y, px_size, CatPaletteTora, tint);
    } else if (party_idx == 1) {
        DrawPixelGrid16(cat_calico, x, y, px_size, CatPaletteMike, tint);
    } else {
        DrawPixelGrid16(cat_mage, x, y, px_size, CatPaletteKuro, tint);
    }
}

static Color EnemyPalette(char ch) {
    switch (ch) {
        case 'K': return (Color){24, 26, 36, 255};
        case 'D': return (Color){52, 60, 78, 255};
        case 'M': return (Color){108, 122, 148, 255};
        case 'L': return (Color){185, 200, 222, 255};
        case 'R': return (Color){245, 52, 68, 255};
        case 'O': return (Color){255, 140, 36, 255};
        case 'Y': return (Color){255, 224, 68, 255};
        case 'C': return (Color){48, 220, 245, 255};
        case 'B': return (Color){64, 128, 220, 255};
        case 'P': return (Color){168, 72, 210, 255};
        default:  return BLANK;
    }
}

void DrawPixelEnemy(int chapter_idx, float cx, float bottom_y, float px_size,
                    bool charging, float time, Color tint) {
    static const char *enemy_sweeper[16] = {
        ".......KK.......",
        ".......KRK......",
        ".......KM.......",
        "...KKKKKKKKKK...",
        "..KLLLLLLLLLLK..",
        ".KLLMMMMMMMMLLK.",
        "KMMDDDDDDDDDDMMK",
        "KMDDDKRRRRKDDDMK",
        "KMDDDKRYORKDDDMK",
        "KDDDDKKKKKKKKDDK",
        "KDDDDDDDDDDDDDDK",
        ".KKKKKKKKKKKKKK.",
        "KYYK.KMMMMK.KYYK",
        ".KK..KKKKKK..KK.",
        "................",
        "................"
    };
    static const char *enemy_mop[16] = {
        "KKKKK......KKKKK",
        "..K..........K..",
        "..KKKK....KKKK..",
        "...KCCCCCCCCK...",
        "..KMMMMMMMMMMK..",
        ".KMDDDDDDDDDDMK.",
        ".KDDKRRRRRRKDDK.",
        ".KDDKRYOOYRKDDK.",
        "..KKMMMMMMMMKK..",
        "..KBBBBBBBBBBK..",
        ".KLLLLLLLLLLLLK.",
        ".KLBLBLBLBLBLBK.",
        ".KLBLBLBLBLBLBK.",
        "..K.K.K..K.K.K..",
        "................",
        "................"
    };
    static const char *enemy_cleaner[16] = {
        "....KK....KK....",
        "....KLLKKLLK....",
        "...KKKKKKKKKK...",
        "..KYYYYYYYYYYK..",
        ".KYYDDDDDDDDYYK.",
        "KYYDDKRRRRKDDYYK",
        "KCCDDKRYORKDDYYK",
        "KCCDDDDDDDDDDYYK",
        "KCCKMMMMMMMMKYYK",
        ".KKKDDDDDDDDKKK.",
        "..KMMDDDDDDMMK..",
        ".KMMMMK..KMMMMK.",
        ".KDDDDK..KDDDDK.",
        ".KKKKKK..KKKKKK.",
        "................",
        "................"
    };
    static const char *enemy_dustron[16] = {
        ".KK..........KK.",
        ".KYYK......KYYK.",
        "..KYYKKKKKKYYK..",
        "...KPPPPPPPPK...",
        "..KPPKCCCCKPPK..",
        ".KDDDKRYYRKDDDK.",
        "KRRRRKDDDDKRRRRK",
        "KRRKMMMMMMMMKRRK",
        "KRKMDDYYYYDDMKRK",
        "KKKMDDDDDDDDMKKK",
        "..KKMDDDDDDMKK..",
        "..KRRKMMMMKRRK..",
        "..KRRKDDDDKRRK..",
        "..KKK.KDDK.KKK..",
        "......KKKK......",
        "................"
    };
    static const char *boss_mega[16] = {
        "..KK..KKKK..KK..",
        ".KYYKKYYYYKKYYK.",
        "..KYYYYYYYYYYK..",
        ".KKRRRRRRRRRRKK.",
        "KRRDDDDDDDDDDRRK",
        "KRDKCCCCCCCCKDRK",
        "KRDKCYYYYYYCKDRK",
        "KDDKKKKKKKKKKDDK",
        "KMMDDDDYYDDDDMMK",
        "KMDDDRRRRRRDDDMM",
        "KMDDDRROORRDDDMK",
        "KDDDDKRRRRKDDDDK",
        ".KKKKDDDDDDKKKK.",
        "KYYK.KMMMMK.KYYK",
        "KYYK.KDDDDK.KYYK",
        ".KK..KKKKKK..KK."
    };

    int style = (chapter_idx >= 0) ? (chapter_idx % 5) : 0;
    const char **sprite = enemy_sweeper;
    if (style == 1) sprite = enemy_mop;
    else if (style == 2) sprite = enemy_cleaner;
    else if (style == 3) sprite = enemy_dustron;
    else if (style == 4) sprite = boss_mega;

    float w = 16.0f * px_size;
    float x = cx - w * 0.5f;
    float y = bottom_y - w;

    if (charging) {
        float pulse = (sinf(time * 10.0f) + 1.0f) * 0.5f;
        DrawCircleGradient((int)cx, (int)(bottom_y - w * 0.5f), w * (0.62f + 0.08f * pulse),
                           Fade(RED, 0.35f + 0.25f * pulse), BLANK);
    }
    DrawPixelGrid16(sprite, x, y, px_size, EnemyPalette, tint);
}

void DrawProceduralStageBg(int chapter_idx, Rectangle area, float time) {
    float x = area.x, y = area.y, w = area.width, h = area.height;
    int style = (chapter_idx >= 0) ? (chapter_idx % 5) : 0;
    if (style == 0) {
        DrawRectangleGradientV((int)x, (int)y, (int)w, (int)(h * 0.72f),
                               (Color){115, 196, 255, 255}, (Color){205, 238, 255, 255});
        DrawCircle((int)(x + w - 95), (int)(y + 68), 26, (Color){255, 238, 130, 255});
        for (int i = 0; i < 3; i++) {
            float cx = x + fmodf(110.0f + i * 270.0f + time * 12.0f, w - 80.0f);
            float cy = y + 60.0f + (i % 2) * 24.0f;
            DrawRectangle((int)cx, (int)cy, 68, 18, (Color){255, 255, 255, 220});
            DrawRectangle((int)cx + 12, (int)cy - 10, 42, 12, (Color){255, 255, 255, 220});
        }
        for (int b = 0; b < 9; b++) {
            int bx = (int)x + 20 + b * 104;
            int bh = 55 + ((b * 37) % 45);
            int by = (int)(y + h * 0.72f) - bh;
            DrawRectangle(bx, by, 88, bh, (Color){168, 198, 214, 255});
            DrawRectangle(bx + 12, by + 14, 16, 16, (Color){255, 240, 175, 255});
            DrawRectangle(bx + 54, by + 14, 16, 16, (Color){255, 240, 175, 255});
        }
        int ground_y = (int)(y + h * 0.72f);
        DrawRectangle((int)x, ground_y, (int)w, (int)(y + h) - ground_y, (Color){188, 172, 146, 255});
        DrawRectangle((int)x, ground_y, (int)w, 6, (Color){114, 178, 92, 255});
        for (int gx = 0; gx < (int)w; gx += 48) {
            DrawLine((int)x + gx, ground_y + 16, (int)x + gx - 16, (int)(y + h), (Color){160, 144, 118, 255});
        }
    } else if (style == 1) {
        DrawRectangleGradientV((int)x, (int)y, (int)w, (int)(h * 0.74f),
                               (Color){22, 58, 56, 255}, (Color){52, 118, 92, 255});
        for (int t = 0; t < 11; t++) {
            int tx = (int)x + 18 + t * 88;
            int ty = (int)(y + h * 0.74f);
            DrawRectangle(tx + 22, ty - 70, 14, 70, (Color){68, 48, 36, 255});
            DrawTriangle((Vector2){(float)(tx + 29), (float)(ty - 115)},
                         (Vector2){(float)tx, (float)(ty - 45)},
                         (Vector2){(float)(tx + 58), (float)(ty - 45)}, (Color){28, 94, 62, 255});
        }
        int gy = (int)(y + h * 0.74f);
        DrawRectangle((int)x, gy, (int)w, (int)(y + h) - gy, (Color){44, 88, 54, 255});
        for (int f = 0; f < 10; f++) {
            float fx = x + 60.0f + f * 86.0f + sinf(time * 2.0f + f) * 14.0f;
            float fy = y + 90.0f + cosf(time * 1.6f + f * 2) * 28.0f;
            DrawCircle((int)fx, (int)fy, 3, (Color){190, 255, 130, 200});
        }
    } else if (style == 2) {
        DrawRectangleGradientV((int)x, (int)y, (int)w, (int)(h * 0.74f),
                               (Color){255, 166, 110, 255}, (Color){255, 224, 168, 255});
        int tx = (int)(x + w * 0.48f);
        int gy = (int)(y + h * 0.74f);
        DrawRectangle(tx - 68, gy - 118, 136, 14, (Color){210, 48, 42, 255});
        DrawRectangle(tx - 54, gy - 94, 108, 10, (Color){210, 48, 42, 255});
        DrawRectangle(tx - 44, gy - 110, 14, 110, (Color){195, 38, 34, 255});
        DrawRectangle(tx + 30, gy - 110, 14, 110, (Color){195, 38, 34, 255});
        DrawRectangle((int)x, gy, (int)w, (int)(y + h) - gy, (Color){176, 166, 152, 255});
    } else if (style == 3) {
        DrawRectangleGradientV((int)x, (int)y, (int)w, (int)(h * 0.74f),
                               (Color){22, 28, 42, 255}, (Color){44, 58, 82, 255});
        for (int p = 0; p < 8; p++) {
            int px = (int)x + 24 + p * 116;
            DrawRectangleLines(px, (int)y + 58, 92, 118, (Color){0, 210, 240, 90});
            DrawCircle(px + 18, (int)y + 76, 4, (p % 2 == 0) ? SKYBLUE : ORANGE);
        }
        int gy = (int)(y + h * 0.74f);
        DrawRectangle((int)x, gy, (int)w, (int)(y + h) - gy, (Color){54, 62, 76, 255});
        DrawLine((int)x, gy, (int)(x + w), gy, (Color){0, 225, 255, 180});
    } else {
        DrawRectangleGradientV((int)x, (int)y, (int)w, (int)(h * 0.74f),
                               (Color){36, 12, 54, 255}, (Color){92, 20, 48, 255});
        float pulse = (sinf(time * 3.0f) + 1.0f) * 0.5f;
        DrawCircleLines((int)(x + w * 0.5f), (int)(y + 130), 75.0f + pulse * 14.0f, Fade(GOLD, 0.35f));
        DrawCircleLines((int)(x + w * 0.5f), (int)(y + 130), 48.0f, Fade(RED, 0.45f));
        int gy = (int)(y + h * 0.74f);
        DrawRectangle((int)x, gy, (int)w, (int)(y + h) - gy, (Color){48, 32, 58, 255});
        DrawLine((int)x, gy, (int)(x + w), gy, GOLD);
    }
}
