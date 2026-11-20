# キャットピア防衛戦 - CatRPG AI Autonomous Dev

ローカルLLM（LM Studio）と画像生成AI（Stable Diffusion WebUI）が協調し、C言語＋Raylib製の猫RPG『キャットピア防衛戦』をリアルタイムに自律拡張・アセット生成し続ける開発実験プロジェクトです。

詳細な処理フローは [docs/architecture.md](./docs/architecture.md) を参照してください。

## 主な特徴

- **ゲーム本体（C99 / Raylib / `src/`）**
  - 猫パーティ3匹（とらねこ・ミケ・クロ）で掃除機メカ軍団と戦う全5章（以降拡張対応）のターン制コマンドバトルRPG。
  - `src/game_data.c` 駆動のスキル／アイテム動的ボタン＆ページ切替に対応し、データ拡張が即座にUIへ反映。
  - `.ttc` フォントからの単体 `.ttf` 抽出と動的コードポイント拡張により、Windows日本語環境での文字化けを防止。
  - PNGアセット未生成時でも遊べる16bit風ピクセルアート描画を内蔵し、AIが生成した画像を起動中のままホットリロード（外周背景の自動透過対応）。
  - 右側320pxの `AI LIVE DIRECTOR` パネルに、AIの思考ログ・SDパラメータ・直近履歴をリアルタイム表示。
- **自律開発デーモン（Python / `scripts/auto_game_dev.py`）**
  - `dev_memory.json` で改修履歴とバックログを記憶し、毎サイクル「やるべき拡張（`src/battle.c` / `src/game_data.c` / 両方 / スキップ）」を自律判断。
  - `src/game_data.c` / `src/battle.c` を解析し、不足しているスプライト・背景を Stable Diffusion WebUI API 経由で生成＆Visionモデルで品質検査（オプションの `rembg` 導入時はAI透過も適用）。
  - 世代バックアップ → ステージングビルド → C不変条件＆全章オートプレイテスト(`tests/test_battle.c`) → スモーク＆画面Vision検査 → 稼働中exeの安全差し替え再起動をループ実行。
  - 8bit PSG風のレトロ効果音（`se_hit.wav`, `se_heal.wav`, `se_ougi.wav`）を自動合成。
- **Webコントロールダッシュボード（`scripts/dashboard.html`）**
  - `http://127.0.0.1:8080/dashboard.html` でAIの思考・履歴・生成画像を監視し、モデル切替、一時停止／即時実行、人間からの緊急指示（User Directive）注入、手動ロールバックが可能。

## ディレクトリ構成

```text
AICAT_RPG/
├── .github/workflows/ci.yml   # MSYS2(GCC+Raylib)ビルド・単体テスト・Python検証CI
├── src/
│   ├── cat_rpg.c              # メインループ、RPG戦闘UI・演出描画
│   ├── font_loader.c / .h     # TTC→TTF抽出＆動的UTF-8フォント管理
│   ├── pixel_art.c / .h       # 16bit風プロシージャルドット絵・章別背景描画
│   ├── battle.c / .h          # ターン制戦闘ロジック、スキル・アイテム・状態異常
│   ├── game_data.c / .h       # 全5章のストーリー・敵・パーティ・スキル・アイテム定義
│   ├── asset_loader.c / .h    # テクスチャのホットリロード＆外周背景透過
│   ├── ai_monitor.c / .h      # ai_status.json 監視と右ペインモニター描画
│   └── common.h               # 共通定数・データ型・ログ関数
├── scripts/
│   ├── auto_game_dev.py       # 自律開発エンジン＆ダッシュボードサーバー
│   └── dashboard.html         # ブラウザ用監視・制御UI
├── tests/
│   ├── test_battle.c          # battle / game_data のヘッドレス単体テスト
│   └── mock/raylib.h          # 単体テスト用最小スタブヘッダ
├── assets/                    # 実行時生成アセット格納先（画像は除外）
├── docs/
│   └── architecture.md        # 自律開発フロー図解
├── config.example.json        # API接続先・ポート・待機秒数設定サンプル
├── build.bat                  # 単体テスト実行＋ゲーム本体ビルド
└── run_catrpg_dev.bat         # 接続確認＆ゲーム・自律開発ループ一括起動
```

> [!NOTE]
> `stable/`（Stable Diffusion WebUI Forge 本体および数GBのモデルファイル）、`backups/`、ビルド済み `.exe`、実行時ログ `ai_status.json`、`config.json` は `.gitignore` によりリポジトリ対象外としています。

## 動作要件

- **ビルド用**: MinGW-w64 (`gcc`) + [Raylib](https://www.raylib.com/) (`-lraylib -lopengl32 -lgdi32 -lwinmm`)
- **自律開発ループ用（任意）**:
  - Python 3.x（標準ライブラリのみで動作・追加パッケージ不要）
  - LM Studio（ローカルサーバー Port `1234`）
  - Stable Diffusion WebUI / Forge（`--api` オプション付きで起動、Port `7860`）

## ビルド・起動方法

1. ゲーム本体のテスト＆ビルド:
   ```bat
   build.bat
   cat_rpg.exe
   ```
2. 接続先ポートや待機時間を変更したい場合（任意）:
   `config.example.json` を `config.json` としてコピーして編集するか、環境変数（`CATRPG_LMSTUDIO_URL`, `CATRPG_SD_URL`, `CATRPG_DASHBOARD_PORT`, `CATRPG_CYCLE_WAIT`）を設定します。
3. 自律開発ループを含めて起動する場合（LM Studio と SD WebUI を起動した状態で）:
   ```bat
   run_catrpg_dev.bat
   ```
   サーバー起動後、自動で `http://127.0.0.1:8080/dashboard.html` が開きます。

## ライセンス

[MIT License](./LICENSE)
