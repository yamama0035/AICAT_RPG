# アーキテクチャ・自律開発サイクル解説

`scripts/auto_game_dev.py` を中心に、ローカルLLM・Stable Diffusion WebUI・Raylib Cゲーム本体・Webダッシュボードが以下のように連携して動作します。

```text
[ scripts/auto_game_dev.py ]
  │
  ├─ 1. コード＆アセット分析 ＋ 思考（Thought）の言語化
  │     src/game_data.c と src/battle.c を読み、不足している画像アセットを特定。
  │     なぜその画像が必要なのか、開発意図を言語化して ai_status.json へ記録。
  │
  ├─ 2. SDパラメータの自律最適化＆プロンプト生成 (LM Studio)
  │     被写体や用途（キャラ/敵 vs 背景）に応じて以下を決定・調整：
  │     - steps (28〜40: 緻密さに応じたステップ数)
  │     - cfg_scale (5.0〜10.0: 輪郭の明瞭度)
  │     - sampler_name ('Euler a' / 'DPM++ 2M Karras')
  │     - 単体ドット絵強制プロンプト / スプライトシート除外ネガティブプロンプト
  │
  ├─ 3. SD WebUI での画像生成・品質検査・ホットリロード配備
  │     SD WebUI API (Port 7860) で生成 → Visionモデルで破綻・グリッド検査。
  │     assets/ へ保存後、cat_rpg.exe の WatchedTexture が更新検知して
  │     外周背景の自動透過を適用し即座にゲーム画面へ反映。
  │
  ├─ 4. 計画・Cコード自動拡張・全章オートプレイテスト・自己修復
  │     `dev_memory.json` で過去の改修履歴とバックログを参照し、重複や無駄な書き換えを回避。
  │     `src/battle.c` および `src/game_data.c` を連携拡張し、`cat_rpg_next.exe` へビルド。
  │     `tests/test_battle.c`（不変条件＋全章クリア可能性オートプレイテスト）と
  │     4秒スモークテスト（＋Windows上の画面キャプチャVision検査）を通過後に
  │     稼働中の cat_rpg.exe を安全に差し替え再起動（失敗時は最大3回自己修復→自動ロールバック）。
  │
  ├─ 5. AI思考・活動のリアルタイム可視化
  │     - ゲーム画面内: 右側320pxの「AI LIVE DIRECTOR パネル」(`src/ai_monitor.c`)
  │     - ブラウザ: `scripts/dashboard.html` (http://127.0.0.1:8080/dashboard.html)
  │
  └─ 6. 人間ディレクターによるリアルタイム制御
        - LM Studio 使用モデルの切り替え
        - 人間指示（User Directive）注入：次回サイクルで最優先反映
        - 一時停止／再開／即時実行／直前バックアップへの1クリック復元
```
