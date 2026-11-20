@echo off
setlocal
where gcc >nul 2>&1
if errorlevel 1 (
    if exist "%USERPROFILE%\mingw64\bin\gcc.exe" set PATH=%USERPROFILE%\mingw64\bin;%PATH%
)

echo [TEST] Running CatRPG battle logic unit tests...
gcc -Wall -Wextra -std=c99 -O2 -Itests/mock -Isrc -o test_battle.exe tests/test_battle.c src/battle.c src/game_data.c
if %ERRORLEVEL% NEQ 0 (
    echo [TEST BUILD ERROR] Unit test compilation failed!
    exit /b 1
)
test_battle.exe
set TEST_ERR=%ERRORLEVEL%
del /q test_battle.exe >nul 2>&1
if %TEST_ERR% NEQ 0 (
    echo [TEST ERROR] Unit tests failed!
    exit /b 1
)

echo [BUILD] Compiling CatRPG modular source files...
gcc -O2 -Isrc -o cat_rpg.exe src/cat_rpg.c src/font_loader.c src/pixel_art.c src/asset_loader.c src/ai_monitor.c src/game_data.c src/battle.c -lraylib -lopengl32 -lgdi32 -lwinmm
if %ERRORLEVEL% EQU 0 (
    echo [BUILD SUCCESS] cat_rpg.exe generated successfully!
) else (
    echo [BUILD ERROR] Compilation failed with error code %ERRORLEVEL%
    exit /b 1
)
