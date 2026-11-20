@echo off
title CatRPG Autonomous Dev Engine (SD WebUI + LM Studio)
echo ==================================================
echo 自律開発エンジン 接続確認
echo ==================================================

:: 1. LM Studio (1234)
curl -s http://127.0.0.1:1234/v1/models >nul 2>&1
if errorlevel 1 goto ERR_LMSTUDIO
echo [OK] LM Studio 接続成功 (Port: 1234)

:: 2. SD WebUI (7860)
curl -s http://127.0.0.1:7860/sdapi/v1/options >nul 2>&1
if errorlevel 1 goto ERR_SD
echo [OK] SD WebUI 接続成功 (Port: 7860)

:: 3. GCC コンパイラ
where gcc >nul 2>&1
if errorlevel 1 goto ERR_GCC
echo [OK] GCC コンパイラ確認成功

:: 4. 実行開始（サーバー起動後にダッシュボードを自動で開く）
echo ==================================================
echo 全接続完了。ゲームと自律開発ループ・Webモニターを起動します。
echo ==================================================
if exist "cat_rpg.exe" start "" "cat_rpg.exe"
python scripts\auto_game_dev.py --open-browser
goto END

:ERR_LMSTUDIO
echo.
echo [接続失敗] LM Studio が起動していません。
echo Developer タブから Start Server を押してください。
pause
exit /b

:ERR_SD
echo.
echo [接続失敗] SD WebUI が起動していません。
echo webui-user.bat に --api を追加して起動してください。
pause
exit /b

:ERR_GCC
echo.
echo [確認失敗] gcc が見つかりません。
echo MinGW または w64devkit に PATH を通してください。
pause
exit /b

:END
pause
