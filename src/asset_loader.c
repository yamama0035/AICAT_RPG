#include "asset_loader.h"
#include <stdlib.h>
#include <math.h>

static int ColorDistSq(unsigned char r1, unsigned char g1, unsigned char b1,
                       unsigned char r2, unsigned char g2, unsigned char b2) {
    int dr = (int)r1 - (int)r2;
    int dg = (int)g1 - (int)g2;
    int db = (int)b1 - (int)b2;
    return dr * dr + dg * dg + db * db;
}

// 四隅の単色背景（白ベタ・黒ベタ等）を外周からフラッドフィルで透過化する
static void RemoveEdgeBackground(Image *img) {
    if (!img || !img->data || img->width <= 2 || img->height <= 2) return;
    if (img->width > 2048 || img->height > 2048) return;

    ImageFormat(img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int w = img->width;
    int h = img->height;
    unsigned char *px = (unsigned char *)img->data;

    int corners[4][2] = {{0, 0}, {w - 1, 0}, {0, h - 1}, {w - 1, h - 1}};
    for (int i = 0; i < 4; i++) {
        int idx = (corners[i][1] * w + corners[i][0]) * 4;
        if (px[idx + 3] < 200) return;
    }

    unsigned char bg_r = px[0], bg_g = px[1], bg_b = px[2];
    int matching_corners = 0;
    for (int i = 1; i < 4; i++) {
        int idx = (corners[i][1] * w + corners[i][0]) * 4;
        if (ColorDistSq(bg_r, bg_g, bg_b, px[idx], px[idx + 1], px[idx + 2]) <= 65 * 65) {
            matching_corners++;
        }
    }
    if (matching_corners < 2) return;

    size_t total = (size_t)w * (size_t)h;
    unsigned char *visited = (unsigned char *)calloc(total, 1);
    int *qx = (int *)malloc(total * sizeof(int));
    int *qy = (int *)malloc(total * sizeof(int));
    if (!visited || !qx || !qy) {
        free(visited);
        free(qx);
        free(qy);
        return;
    }

    int head = 0, tail = 0;
    const int tol_sq = 62 * 62;

    for (int x = 0; x < w; x++) {
        int top = x;
        int bot = (h - 1) * w + x;
        if (!visited[top] && ColorDistSq(bg_r, bg_g, bg_b, px[top * 4], px[top * 4 + 1], px[top * 4 + 2]) <= tol_sq) {
            visited[top] = 1; qx[tail] = x; qy[tail] = 0; tail++;
        }
        if (!visited[bot] && ColorDistSq(bg_r, bg_g, bg_b, px[bot * 4], px[bot * 4 + 1], px[bot * 4 + 2]) <= tol_sq) {
            visited[bot] = 1; qx[tail] = x; qy[tail] = h - 1; tail++;
        }
    }
    for (int y = 1; y < h - 1; y++) {
        int left = y * w;
        int right = y * w + (w - 1);
        if (!visited[left] && ColorDistSq(bg_r, bg_g, bg_b, px[left * 4], px[left * 4 + 1], px[left * 4 + 2]) <= tol_sq) {
            visited[left] = 1; qx[tail] = 0; qy[tail] = y; tail++;
        }
        if (!visited[right] && ColorDistSq(bg_r, bg_g, bg_b, px[right * 4], px[right * 4 + 1], px[right * 4 + 2]) <= tol_sq) {
            visited[right] = 1; qx[tail] = w - 1; qy[tail] = y; tail++;
        }
    }

    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    while (head < tail) {
        int cx = qx[head];
        int cy = qy[head];
        head++;
        px[(cy * w + cx) * 4 + 3] = 0;

        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int nidx = ny * w + nx;
            if (visited[nidx]) continue;
            int p = nidx * 4;
            if (ColorDistSq(bg_r, bg_g, bg_b, px[p], px[p + 1], px[p + 2]) <= tol_sq) {
                visited[nidx] = 1;
                qx[tail] = nx;
                qy[tail] = ny;
                tail++;
            }
        }
    }

    free(visited);
    free(qx);
    free(qy);
}

void UpdateWatchedSprite(WatchedTexture *wt, const char *filepath, bool remove_bg) {
    if (!wt || !filepath || !FileExists(filepath)) return;
    long mod_time = GetFileModTime(filepath);
    if (!wt->loaded || mod_time > wt->last_modified) {
        if (wt->loaded) {
            UnloadTexture(wt->texture);
            wt->loaded = false;
        }
        Image img = LoadImage(filepath);
        if (img.data != NULL && img.width > 0 && img.height > 0) {
            if (remove_bg) {
                RemoveEdgeBackground(&img);
            }
            wt->texture = LoadTextureFromImage(img);
            UnloadImage(img);
            if (wt->texture.id > 0) {
                SetTextureFilter(wt->texture, TEXTURE_FILTER_POINT);
                wt->loaded = true;
            }
        }
        wt->last_modified = mod_time;
    }
}

void UpdateWatchedTexture(WatchedTexture *wt, const char *filepath) {
    UpdateWatchedSprite(wt, filepath, false);
}

void UnloadWatchedTexture(WatchedTexture *wt) {
    if (wt && wt->loaded) {
        UnloadTexture(wt->texture);
        wt->loaded = false;
        wt->last_modified = 0;
    }
}
