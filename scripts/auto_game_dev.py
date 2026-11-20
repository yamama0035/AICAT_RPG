import os
import sys
import json
import time
import base64
import shutil
import subprocess
import argparse
import webbrowser
import urllib.request
import urllib.error
import urllib.parse
import re
import threading
import wave
import struct
import math
import random
import zlib
from http.server import HTTPServer, BaseHTTPRequestHandler

if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if hasattr(sys.stderr, "reconfigure"):
    try:
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# どのディレクトリから起動されてもリポジトリルートを基準に動作する
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT_DIR)

def load_runtime_config():
    cfg = {
        "lmstudio_base_url": "http://127.0.0.1:1234",
        "sd_webui_base_url": "http://127.0.0.1:7860",
        "dashboard_port": 8080,
        "cycle_wait_seconds": 30,
    }
    if os.path.exists("config.json"):
        try:
            with open("config.json", "r", encoding="utf-8") as f:
                loaded = json.load(f)
                if isinstance(loaded, dict):
                    cfg.update(loaded)
        except Exception as e:
            print(f"[Config Warning] config.json の読み込みに失敗: {e}")

    if os.environ.get("CATRPG_LMSTUDIO_URL"):
        cfg["lmstudio_base_url"] = os.environ["CATRPG_LMSTUDIO_URL"]
    if os.environ.get("CATRPG_SD_URL"):
        cfg["sd_webui_base_url"] = os.environ["CATRPG_SD_URL"]
    if os.environ.get("CATRPG_DASHBOARD_PORT"):
        try:
            cfg["dashboard_port"] = int(os.environ["CATRPG_DASHBOARD_PORT"])
        except ValueError:
            pass
    if os.environ.get("CATRPG_CYCLE_WAIT"):
        try:
            cfg["cycle_wait_seconds"] = int(os.environ["CATRPG_CYCLE_WAIT"])
        except ValueError:
            pass

    cfg["lmstudio_base_url"] = str(cfg["lmstudio_base_url"]).rstrip("/")
    cfg["sd_webui_base_url"] = str(cfg["sd_webui_base_url"]).rstrip("/")
    return cfg

CONFIG = load_runtime_config()
LMSTUDIO_API = f"{CONFIG['lmstudio_base_url']}/v1/chat/completions"
LMSTUDIO_MODELS_API = f"{CONFIG['lmstudio_base_url']}/v1/models"
SD_WEBUI_API = f"{CONFIG['sd_webui_base_url']}/sdapi/v1/txt2img"
DASHBOARD_PORT = int(CONFIG["dashboard_port"])
CYCLE_WAIT_SEC = max(5, int(CONFIG["cycle_wait_seconds"]))

SRC_FILES = [
    "src/cat_rpg.c",
    "src/font_loader.c",
    "src/pixel_art.c",
    "src/asset_loader.c",
    "src/ai_monitor.c",
    "src/game_data.c",
    "src/battle.c",
]
HEADER_FILES = [
    "src/common.h",
    "src/font_loader.h",
    "src/pixel_art.h",
    "src/asset_loader.h",
    "src/ai_monitor.h",
    "src/game_data.h",
    "src/battle.h",
]
SRC_FILE = "src/battle.c"  # LLMが主に拡張する戦闘ロジックモジュール
DATA_FILE = "src/game_data.c"
MAIN_FILE = "src/cat_rpg.c"
DASHBOARD_FILE = os.path.join("scripts", "dashboard.html")
EXE_FILE = "cat_rpg.exe"
STAGING_EXE = "cat_rpg_next.exe"
ASSET_DIR = "assets"
BACKUP_DIR = "backups"
STATUS_FILE = "ai_status.json"
MEMORY_FILE = "dev_memory.json"
PREVIEW_IMAGE = "build_preview.png"

os.makedirs(ASSET_DIR, exist_ok=True)
os.makedirs(BACKUP_DIR, exist_ok=True)

STATUS_LOCK = threading.Lock()
MEMORY_LOCK = threading.Lock()
CONTROL_STATE = {
    "selected_model": "local-model",
    "pending_instruction": "",
    "is_paused": False,
    "force_next": False,
    "available_models": ["local-model"],
}

def get_lmstudio_models():
    try:
        req = urllib.request.Request(LMSTUDIO_MODELS_API)
        with urllib.request.urlopen(req, timeout=3) as resp:
            data = json.loads(resp.read().decode())
            models = [m.get("id") for m in data.get("data", []) if m.get("id")]
            return models if models else ["local-model"]
    except Exception:
        return [CONTROL_STATE["selected_model"]]

def is_safe_subpath(base_dir, target_path):
    base_abs = os.path.abspath(base_dir)
    target_abs = os.path.abspath(target_path)
    try:
        return os.path.commonpath([base_abs, target_abs]) == base_abs
    except ValueError:
        return False

class DevServerHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # コンソール出力を抑制

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        if path in ["/", "/index.html", "/dashboard.html"]:
            self.serve_file(DASHBOARD_FILE, "text/html; charset=utf-8")
        elif path == "/ai_status.json":
            with STATUS_LOCK:
                self.serve_file(STATUS_FILE, "application/json; charset=utf-8")
        elif path.startswith("/assets/"):
            rel_name = urllib.parse.unquote(path[8:]).lstrip("/\\")
            candidate = os.path.join(ASSET_DIR, rel_name)
            if not is_safe_subpath(ASSET_DIR, candidate) or not os.path.isfile(candidate):
                candidate = None
                for sub in ["characters", "enemies", "backgrounds", "ui", "sounds", "misc"]:
                    sub_path = os.path.join(ASSET_DIR, sub, rel_name)
                    if is_safe_subpath(ASSET_DIR, sub_path) and os.path.isfile(sub_path):
                        candidate = sub_path
                        break
            if candidate and os.path.isfile(candidate):
                ext = os.path.splitext(candidate)[1].lower()
                ctype = "image/png" if ext == ".png" else "audio/wav" if ext == ".wav" else "application/octet-stream"
                self.serve_file(candidate, ctype)
            else:
                self.send_error(404)
        elif path == "/api/models":
            models = get_lmstudio_models()
            CONTROL_STATE["available_models"] = models
            data = {
                "models": models,
                "current_model": CONTROL_STATE["selected_model"],
                "is_paused": CONTROL_STATE["is_paused"],
                "pending_instruction": CONTROL_STATE["pending_instruction"]
            }
            self.send_json(data)
        elif path == "/api/memory":
            self.send_json(load_dev_memory())
        else:
            self.send_error(404)

    def do_POST(self):
        content_len = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_len).decode("utf-8")
        try:
            req_data = json.loads(body) if body else {}
        except Exception:
            req_data = {}

        if self.path == "/api/set_model":
            new_model = req_data.get("model")
            if new_model:
                CONTROL_STATE["selected_model"] = new_model
                print(f"\n[Model Switch] 🤖 LM Studio使用モデルを変更: {new_model}")
            self.send_json({"status": "ok", "current_model": CONTROL_STATE["selected_model"]})

        elif self.path == "/api/instruct":
            instruction = req_data.get("instruction", "").strip()
            if instruction:
                CONTROL_STATE["pending_instruction"] = instruction
                print(f"\n[User Directive 受信] 👨‍💻 人間ディレクターからの緊急指示: {instruction}")
                update_status(
                    phase="directive_queued",
                    status="人間指示を受信・次回サイクル反映待ち",
                    thought=f"【人間ディレクターからの緊急指示を受信】: {instruction}"
                )
            self.send_json({"status": "ok", "pending_instruction": CONTROL_STATE["pending_instruction"]})

        elif self.path == "/api/control":
            action = req_data.get("action")
            if action == "pause":
                CONTROL_STATE["is_paused"] = True
                print("[Loop Control] ⏸️ 自律開発ループを一時停止しました。")
            elif action == "resume":
                CONTROL_STATE["is_paused"] = False
                print("[Loop Control] ▶️ 自律開発ループを再開しました。")
            elif action == "trigger":
                CONTROL_STATE["force_next"] = True
                CONTROL_STATE["is_paused"] = False
                print("[Loop Control] ⚡ 開発サイクルを即時実行します。")
            self.send_json({
                "status": "ok",
                "is_paused": CONTROL_STATE["is_paused"]
            })
        elif self.path == "/api/rollback":
            success = rollback_latest_backup()
            if success:
                update_status("rollback", "手動ロールバック完了", thought="直前のバックアップコードに復元しました。")
            self.send_json({"status": "ok" if success else "error"})
        else:
            self.send_error(404)

    def serve_file(self, filepath, content_type):
        if not os.path.exists(filepath):
            self.send_error(404)
            return
        with open(filepath, "rb") as f:
            content = f.read()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(content)

    def send_json(self, data):
        content = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(content)

def start_dev_server():
    server = HTTPServer(("127.0.0.1", DASHBOARD_PORT), DevServerHandler)
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()
    print(f"[Web Monitor] リアルタイム操作ダッシュボード稼働中: http://127.0.0.1:{DASHBOARD_PORT}/dashboard.html")
    return server

def update_status(phase, status, thought=None, parameters=None, history_item=None):
    with STATUS_LOCK:
        current = {
            "phase": phase,
            "status": status,
            "current_thought": thought or "思考を分析中...",
            "parameters": parameters or {
                "filename": "---",
                "steps": 20,
                "cfg_scale": 7.0,
                "sampler_name": "Euler a",
                "width": 768,
                "height": 768
            },
            "history": []
        }
        if os.path.exists(STATUS_FILE):
            try:
                with open(STATUS_FILE, "r", encoding="utf-8") as f:
                    saved = json.load(f)
                    if not thought and "current_thought" in saved:
                        current["current_thought"] = saved["current_thought"]
                    if not parameters and "parameters" in saved:
                        current["parameters"] = saved["parameters"]
                    if "history" in saved and isinstance(saved["history"], list):
                        current["history"] = saved["history"]
            except Exception:
                pass

        if history_item:
            current["history"].insert(0, history_item)
            current["history"] = current["history"][:15]

        tmp_file = f"{STATUS_FILE}.tmp"
        try:
            with open(tmp_file, "w", encoding="utf-8") as f:
                json.dump(current, f, ensure_ascii=False, indent=2)
            os.replace(tmp_file, STATUS_FILE)
        except Exception as e:
            print(f"[Status Write Error] {e}")

def load_dev_memory():
    default_mem = {
        "completed": [],
        "backlog": [
            "敵AIの行動パターンや属性・状態異常ギミックの強化",
            "新スキルや新アイテム、あるいは第6章以降のストーリー拡張",
            "テンション連携やクリティカル演出・戦略性の向上"
        ],
        "last_cycle_time": ""
    }
    with MEMORY_LOCK:
        if not os.path.exists(MEMORY_FILE):
            return default_mem
        try:
            with open(MEMORY_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                if isinstance(data, dict):
                    default_mem.update(data)
        except Exception:
            pass
    return default_mem

def save_dev_memory(mem):
    with MEMORY_LOCK:
        tmp = f"{MEMORY_FILE}.tmp"
        try:
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(mem, f, ensure_ascii=False, indent=2)
            os.replace(tmp, MEMORY_FILE)
        except Exception as e:
            print(f"[Memory Save Error] {e}")

def record_completed_feature(target, summary, backlog=None):
    mem = load_dev_memory()
    mem["completed"].insert(0, {
        "time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "target": target,
        "summary": summary[:160]
    })
    mem["completed"] = mem["completed"][:20]
    if isinstance(backlog, list) and backlog:
        mem["backlog"] = [str(x)[:120] for x in backlog[:6]]
    mem["last_cycle_time"] = time.strftime("%Y-%m-%d %H:%M:%S")
    save_dev_memory(mem)

def query_lmstudio(prompt, system="", max_tokens=8000):
    model_name = CONTROL_STATE.get("selected_model", "")
    available = [m for m in get_lmstudio_models() if "embed" not in m.lower()]
    if not model_name or model_name == "local-model" or model_name not in available:
        if available:
            model_name = available[0]
            CONTROL_STATE["selected_model"] = model_name

    payload = {
        "model": model_name,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": prompt}
        ],
        "temperature": 0.25,
        "max_tokens": max_tokens,
        "stream": False
    }
    req = urllib.request.Request(
        LMSTUDIO_API,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            data = json.loads(resp.read().decode())
            return data["choices"][0]["message"]["content"]
    except urllib.error.HTTPError as e:
        err_body = ""
        try:
            err_body = e.read().decode("utf-8", errors="ignore")
        except Exception:
            pass
        print(f"[LLMエラー] LM Studio HTTP {e.code}: {err_body if err_body else e.reason}")
        return None
    except Exception as e:
        print(f"[LLMエラー] LM Studioとの通信に失敗: {e}")
        return None

def compile_game(output_exe=EXE_FILE):
    cmd = [
        "gcc", "-O2", "-Isrc", "-o", output_exe
    ] + SRC_FILES + [
        "-lraylib", "-lopengl32", "-lgdi32", "-lwinmm"
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode == 0, result.stderr

def run_unit_tests():
    test_exe = "test_battle_tmp.exe"
    cmd = [
        "gcc", "-Wall", "-Wextra", "-std=c99", "-O2",
        "-Itests/mock", "-Isrc",
        "-o", test_exe,
        "tests/test_battle.c", "src/battle.c", "src/game_data.c"
    ]
    try:
        comp = subprocess.run(cmd, capture_output=True, text=True)
        if comp.returncode != 0:
            return False, f"Unit Test Build Error:\n{comp.stderr}"
        run_res = subprocess.run([os.path.abspath(test_exe)], capture_output=True, text=True, timeout=15)
        if run_res.returncode != 0:
            return False, f"Unit Test Failed:\n{run_res.stdout}\n{run_res.stderr}"
        return True, run_res.stdout.strip()
    except Exception as e:
        return False, f"Unit Test Exception: {e}"
    finally:
        if os.path.exists(test_exe):
            try:
                os.remove(test_exe)
            except Exception:
                pass

def _encode_png_rgba(width, height, rgba_bytes):
    def chunk(tag, data):
        return struct.pack("!I", len(data)) + tag + data + struct.pack("!I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    stride = width * 4
    rows = b"".join(b"\x00" + rgba_bytes[y * stride:(y + 1) * stride] for y in range(height))
    ihdr = struct.pack("!IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b"")

def capture_window_png(pid, out_path=PREVIEW_IMAGE):
    if os.name != "nt":
        return False
    try:
        import ctypes
        from ctypes import wintypes

        user32 = ctypes.windll.user32
        gdi32 = ctypes.windll.gdi32
        target_hwnd = []

        @ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
        def enum_cb(hwnd, lparam):
            if not user32.IsWindowVisible(hwnd):
                return True
            win_pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(win_pid))
            if win_pid.value == pid:
                rect = wintypes.RECT()
                if user32.GetClientRect(hwnd, ctypes.byref(rect)):
                    if rect.right - rect.left >= 300 and rect.bottom - rect.top >= 200:
                        target_hwnd.append((hwnd, rect.right - rect.left, rect.bottom - rect.top))
                        return False
            return True

        user32.EnumWindows(enum_cb, 0)
        if not target_hwnd:
            return False

        hwnd, w, h = target_hwnd[0]
        hdc_win = user32.GetWindowDC(hwnd)
        hdc_mem = gdi32.CreateCompatibleDC(hdc_win)
        hbmp = gdi32.CreateCompatibleBitmap(hdc_win, w, h)
        old = gdi32.SelectObject(hdc_mem, hbmp)

        # PW_RENDERFULLCONTENT (0x2) | PW_CLIENTONLY (0x1)
        user32.PrintWindow(hwnd, hdc_mem, 3)

        class BITMAPINFOHEADER(ctypes.Structure):
            _fields_ = [
                ("biSize", wintypes.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                ("biPlanes", wintypes.WORD), ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", ctypes.c_long), ("biYPelsPerMeter", ctypes.c_long),
                ("biClrUsed", wintypes.DWORD), ("biClrImportant", wintypes.DWORD),
            ]

        bmi = BITMAPINFOHEADER()
        bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        bmi.biWidth = w
        bmi.biHeight = -h  # top-down
        bmi.biPlanes = 1
        bmi.biBitCount = 32
        bmi.biCompression = 0

        buf = (ctypes.c_ubyte * (w * h * 4))()
        gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), 0)

        gdi32.SelectObject(hdc_mem, old)
        gdi32.DeleteObject(hbmp)
        gdi32.DeleteDC(hdc_mem)
        user32.ReleaseDC(hwnd, hdc_win)

        bgra = bytearray(buf)
        # BGRA -> RGBA (alpha = 255)
        rgba = bytearray(len(bgra))
        rgba[0::4] = bgra[2::4]
        rgba[1::4] = bgra[1::4]
        rgba[2::4] = bgra[0::4]
        rgba[3::4] = b"\xff" * (w * h)

        # すべて真っ黒な場合はスキップ（ドライバ未描画環境対策）
        if max(rgba[0::4]) == 0 and max(rgba[1::4]) == 0 and max(rgba[2::4]) == 0:
            return False

        with open(out_path, "wb") as f:
            f.write(_encode_png_rgba(w, h, bytes(rgba)))
        return True
    except Exception:
        return False

def inspect_ingame_screenshot(img_path):
    if not os.path.exists(img_path):
        return True, "スクリーンショットなし（スキップ）"
    try:
        with open(img_path, "rb") as f:
            img_b64 = base64.b64encode(f.read()).decode("ascii")
    except Exception:
        return True, "画像読込スキップ"

    model_name = CONTROL_STATE.get("selected_model", "")
    available = [m for m in get_lmstudio_models() if "embed" not in m.lower()]
    if not model_name or model_name not in available:
        if available:
            model_name = available[0]

    prompt_text = (
        "あなたはRPGゲームのUIテスト担当官です。起動したゲーム画面のスクリーンショットを確認してください。\n"
        "1. 画面内の日本語テキストが大量の「???」になって文字化けしていないか\n"
        "2. ボタンや画面レイアウトが完全に崩れて操作不能になっていないか\n"
        "問題なければ pass: true、致命的な文字化けや画面崩壊があれば pass: false を返してください。\n"
        "JSONのみで出力: {\"pass\": true/false, \"reason\": \"理由1文\"}"
    )
    payload = {
        "model": model_name,
        "messages": [{"role": "user", "content": [
            {"type": "text", "text": prompt_text},
            {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{img_b64}"}}
        ]}],
        "temperature": 0.1,
        "max_tokens": 256,
        "stream": False
    }
    req = urllib.request.Request(LMSTUDIO_API, data=json.dumps(payload).encode("utf-8"),
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=25) as resp:
            content = json.loads(resp.read().decode())["choices"][0]["message"]["content"]
            m = re.search(r"\{.*\}", content, re.DOTALL)
            if m:
                res_json = json.loads(m.group())
                return bool(res_json.get("pass", True)), res_json.get("reason", "OK")
    except Exception as e:
        return True, f"Vision検査スキップ: {e}"
    return True, "OK"

def run_smoke_test(exe_path=EXE_FILE):
    if not os.path.exists(exe_path):
        return False, "Executable not found."
    try:
        proc = subprocess.Popen([os.path.abspath(exe_path)])
        time.sleep(4)
        if proc.poll() is not None:
            return False, "Runtime Crash: Window closed immediately after launch."

        visual_ok = True
        visual_msg = "OK"
        if capture_window_png(proc.pid, PREVIEW_IMAGE):
            visual_ok, visual_msg = inspect_ingame_screenshot(PREVIEW_IMAGE)
            print(f"[Visual Smoke Test] {visual_ok}: {visual_msg}")

        proc.terminate()
        try:
            proc.wait(timeout=3)
        except Exception:
            proc.kill()
        if not visual_ok:
            return False, f"Visual UI Check Failed: {visual_msg}"
        return True, visual_msg
    except Exception as e:
        return False, f"Smoke Test Exception: {e}"

def is_game_running():
    if os.name != "nt":
        return False
    try:
        res = subprocess.run(
            ["tasklist", "/FI", f"IMAGENAME eq {EXE_FILE}", "/FO", "CSV", "/NH"],
            capture_output=True, text=True, check=False
        )
        return EXE_FILE.lower() in res.stdout.lower()
    except Exception:
        return False

def stop_running_game():
    if os.name != "nt":
        return
    try:
        subprocess.run(["taskkill", "/IM", EXE_FILE, "/F"], capture_output=True, check=False)
        time.sleep(0.5)
    except Exception:
        pass

def start_game_detached():
    if not os.path.exists(EXE_FILE):
        return
    try:
        kwargs = {}
        if os.name == "nt":
            kwargs["creationflags"] = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
        subprocess.Popen([os.path.abspath(EXE_FILE)], cwd=ROOT_DIR, **kwargs)
    except Exception as e:
        print(f"[Game Launch Warning] {e}")

def promote_staged_exe(staging_exe=STAGING_EXE, target_exe=EXE_FILE):
    was_running = is_game_running()
    if was_running:
        print(f"[Hot-Swap] 実行中の {target_exe} を一時停止して新ビルドへ差し替えます...")
        stop_running_game()

    for _ in range(5):
        try:
            os.replace(staging_exe, target_exe)
            if was_running:
                start_game_detached()
            return True
        except PermissionError:
            time.sleep(0.5)
    return False

def get_asset_subfolder(filename):
    fn = filename.lower()
    if fn.startswith("party_") or "chara" in fn or "cat" in fn or "player" in fn:
        return "characters"
    elif fn.startswith("enemy_") or fn.startswith("boss_") or "monster" in fn or "drone" in fn or "sweeper" in fn or "vacuum" in fn:
        return "enemies"
    elif fn.startswith("bg_") or "background" in fn or "stage" in fn:
        return "backgrounds"
    elif fn.startswith("ui_") or "gauge" in fn or "icon" in fn or "btn" in fn or fn.startswith("item_"):
        return "ui"
    elif fn.endswith(".wav") or fn.endswith(".mp3"):
        return "sounds"
    else:
        return "misc"

def postprocess_sprite_transparency(raw_bytes):
    """オプションの rembg が導入されていればAI背景切り抜きを適用（未導入時はC側の外周透過に委任）"""
    try:
        import rembg
        return rembg.remove(raw_bytes)
    except Exception:
        return raw_bytes

def organize_existing_assets():
    """既存の assets/ 内のファイルを種類別サブフォルダに整理コピー"""
    if not os.path.exists(ASSET_DIR):
        return
    for f in os.listdir(ASSET_DIR):
        fpath = os.path.join(ASSET_DIR, f)
        if os.path.isfile(fpath):
            sub = get_asset_subfolder(f)
            sub_dir = os.path.join(ASSET_DIR, sub)
            os.makedirs(sub_dir, exist_ok=True)
            dest = os.path.join(sub_dir, f)
            if not os.path.exists(dest):
                try:
                    shutil.copy2(fpath, dest)
                except Exception:
                    pass

def inspect_image_quality(image_base64, filename, thought=""):
    if not image_base64 or len(image_base64) < 1000:
        return False, "画像データが空または破損しています"

    model_name = CONTROL_STATE.get("selected_model", "")
    available = [m for m in get_lmstudio_models() if "embed" not in m.lower()]
    if not model_name or model_name not in available:
        if available:
            model_name = available[0]

    prompt_text = (
        f"画像ファイル名: {filename}\n"
        f"設計意図: {thought}\n\n"
        "あなたはレトロRPGのドット絵（Pixel Art）品質検査官です。提示された画像を厳格に検査してください。\n"
        "【検査基準】\n"
        "1. 画像全体が真っ黒、真っ白、または単色ベタ塗りではないか\n"
        "2. レトロゲームのドット絵（Pixel Art）として成立しているか（ボケボケの実写風、3Dポリゴン崩れ、酷いノイズがないか）\n"
        "3. キャラクターや背景としての造形が著しく破綻・グチャグチャになっていないか\n"
        "4. 【重要】キャラクター／敵スプライトの場合、スプライトシート・アイコン一覧・グリッド分割・複数体コラージュになっていないか（1体単体の全身キャラクターが大きく中央に描かれているか）\n"
        "上記のいずれかに違反していれば pass: false としてください。\n"
        "必ず以下のJSON形式のみで出力してください（前置き・解説禁止）：\n"
        "{\"pass\": true または false, \"reason\": \"判定理由（日本語1文）\", \"fix_keyword\": \"再生成時に補強すべき英語プロンプト\"}"
    )

    payload = {
        "model": model_name,
        "messages": [
            {
                "role": "user",
                "content": [
                    {"type": "text", "text": prompt_text},
                    {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{image_base64}"}}
                ]
            }
        ],
        "temperature": 0.1,
        "max_tokens": 512,
        "stream": False
    }

    req = urllib.request.Request(
        LMSTUDIO_API,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            data = json.loads(resp.read().decode())
            content = data["choices"][0]["message"]["content"]
            m = re.search(r"\{.*\}", content, re.DOTALL)
            if m:
                res_json = json.loads(m.group())
                is_pass = res_json.get("pass", True)
                reason = res_json.get("reason", "合格")
                fix = res_json.get("fix_keyword", "")
                return bool(is_pass), f"{reason} (改善案: {fix})" if fix else reason
            return True, "JSONパース不可のため合格"
    except Exception as e:
        return True, f"Vision未対応または検査スキップ: {e}"

def generate_pixel_image(filename, positive_prompt, negative_prompt="", steps=35, cfg_scale=7.5, sampler_name="Euler a", width=768, height=768, seed=-1, thought=""):
    is_background = filename.lower().startswith("bg_") or "background" in filename.lower()
    if is_background:
        required_pos = "wide retro 16-bit RPG battle background scenery, pixel art, clean composition, no characters"
        base_neg = "character, text, watermark, grid, split screen, blurry, 3d, realistic, photo, deformed"
    else:
        required_pos = "solo, single character, full body, centered, standing pose, pixel art, 16-bit retro RPG sprite, clean dark outline, pure solid white background"
        base_neg = "sprite sheet, icon pack, grid, multiple characters, multiple views, collage, tileset, split screen, text, watermark, blurry, smooth, 3d, realistic, photo, antialiasing, noise, deformed, ugly"

    current_prompt = f"{positive_prompt}, {required_pos}" if positive_prompt else required_pos
    negative_prompt = f"{negative_prompt}, {base_neg}" if negative_prompt else base_neg

    steps_val = max(28, min(int(steps if steps else 35), 40))
    cfg_val = max(5.0, min(float(cfg_scale if cfg_scale else 7.5), 10.0))
    default_w = 1024 if is_background else 768
    default_h = 512 if is_background else 768
    w_val = max(512, min(int(width if width and int(width) >= 768 else default_w), 1024))
    h_val = max(512, min(int(height if height and int(height) >= 512 else default_h), 1024))
    sampler_val = str(sampler_name) if sampler_name else "Euler a"
    seed_val = int(seed) if seed is not None else -1

    subfolder = get_asset_subfolder(filename)
    subfolder_dir = os.path.join(ASSET_DIR, subfolder)
    os.makedirs(subfolder_dir, exist_ok=True)
    subfolder_path = os.path.join(subfolder_dir, filename)
    root_output_path = os.path.join(ASSET_DIR, filename)

    max_retries = 2

    for attempt in range(max_retries + 1):
        param_dict = {
            "filename": f"{subfolder}/{filename}",
            "steps": steps_val,
            "cfg_scale": cfg_val,
            "sampler_name": sampler_val,
            "width": w_val,
            "height": h_val,
            "seed": seed_val
        }

        status_msg = f"画像生成中: {subfolder}/{filename} (Steps: {steps_val}, CFG: {cfg_val})"
        if attempt > 0:
            status_msg = f"画像再生成中(試行 {attempt+1}): {subfolder}/{filename}"

        update_status(
            phase="asset_gen",
            status=status_msg,
            thought=f"【画像生成の意図】{thought or 'ドット絵アセットの生成を実行中'}",
            parameters=param_dict
        )

        payload = {
            "prompt": current_prompt,
            "negative_prompt": negative_prompt,
            "steps": steps_val,
            "width": w_val,
            "height": h_val,
            "cfg_scale": cfg_val,
            "sampler_name": sampler_val,
            "seed": seed_val,
            "enable_hr": False,
        }

        req = urllib.request.Request(
            SD_WEBUI_API,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        try:
            with urllib.request.urlopen(req, timeout=300) as res:
                res_data = json.loads(res.read().decode())
                image_base64 = res_data["images"][0]

                is_pass, inspect_reason = inspect_image_quality(image_base64, filename, thought)
                if not is_pass and attempt < max_retries:
                    print(f"[Qwen Inspection RETRY ({attempt+1}/{max_retries})] {filename}: {inspect_reason}")
                    update_status(
                        phase="asset_retry",
                        status=f"画像再生成判定: {filename}",
                        thought=f"【Qwen検査による再生成】{inspect_reason}。シード値を変更して再試行します..."
                    )
                    seed_val = -1
                    current_prompt += ", crisp pixel art sprite, sharp edges"
                    time.sleep(2)
                    continue

                if is_pass:
                    print(f"[Qwen Inspection PASS] {filename}: {inspect_reason}")

                raw_bytes = base64.b64decode(image_base64)
                if not is_background:
                    raw_bytes = postprocess_sprite_transparency(raw_bytes)
                with open(subfolder_path, "wb") as f:
                    f.write(raw_bytes)
                with open(root_output_path, "wb") as f:
                    f.write(raw_bytes)

                print(f"[SD WebUI] ドット絵を生成・保存完了: assets/{subfolder}/{filename} (Steps:{steps_val}, CFG:{cfg_val})")
                now_str = time.strftime("%H:%M:%S")
                update_status(
                    phase="asset_done",
                    status=f"画像保存完了: {subfolder}/{filename}",
                    thought=f"【画像生成完了】{subfolder}/{filename} を配備しました（検査: {inspect_reason}）。思考: {thought}",
                    parameters=param_dict,
                    history_item={
                        "time": now_str,
                        "type": "asset",
                        "title": f"画像生成: {subfolder}/{filename}",
                        "detail": f"Steps: {steps_val}, CFG: {cfg_val} | 検査: {inspect_reason[:60]}"
                    }
                )
                return True
        except Exception as e:
            print(f"[SD WebUI] 画像生成エラー: {e}")
            update_status(
                phase="asset_error",
                status=f"画像生成失敗: {filename}",
                thought=f"エラーが発生しました: {e}"
            )
            return False

    return False

def backup_code():
    ts = time.strftime("%Y%m%d_%H%M%S")
    backup_sub = os.path.join(BACKUP_DIR, f"rev_{ts}")
    try:
        os.makedirs(backup_sub, exist_ok=True)
        for s in SRC_FILES + HEADER_FILES:
            if os.path.exists(s):
                dst = os.path.join(backup_sub, s)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(s, dst)
        dirs = sorted([d for d in os.listdir(BACKUP_DIR) if os.path.isdir(os.path.join(BACKUP_DIR, d)) and d.startswith("rev_")])
        if len(dirs) > 15:
            for old in dirs[:-15]:
                shutil.rmtree(os.path.join(BACKUP_DIR, old), ignore_errors=True)
    except Exception as e:
        print(f"[Backup Error] {e}")

def rollback_latest_backup():
    if not os.path.exists(BACKUP_DIR):
        return False
    dirs = sorted([d for d in os.listdir(BACKUP_DIR) if os.path.isdir(os.path.join(BACKUP_DIR, d)) and d.startswith("rev_")])
    if not dirs:
        return False

    target_dir = os.path.join(BACKUP_DIR, dirs[-1])
    try:
        for root, _, files in os.walk(target_dir):
            for fname in files:
                src_path = os.path.join(root, fname)
                rel = os.path.relpath(src_path, target_dir)
                # 旧形式バックアップとの互換: src/ がなければ src/ 配下へ戻す
                if not os.path.dirname(rel) and os.path.exists(os.path.join("src", fname)):
                    dst_path = os.path.join("src", fname)
                else:
                    dst_path = rel
                os.makedirs(os.path.dirname(dst_path) or ".", exist_ok=True)
                shutil.copy2(src_path, dst_path)

        ok, _ = compile_game(STAGING_EXE)
        if ok:
            promote_staged_exe(STAGING_EXE, EXE_FILE)
        print(f"[Time Machine] 直前の全モジュールバックアップへ復元しました: {target_dir}")
        return True
    except Exception as e:
        print(f"[Rollback Error] {e}")
        return False

def generate_retro_wav(filepath, sound_type="hit"):
    if os.path.exists(filepath):
        return
    sample_rate = 22050

    if sound_type == "hit":
        duration = 0.18
        n_samples = int(sample_rate * duration)
        with wave.open(filepath, "w") as wav:
            wav.setparams((1, 2, sample_rate, n_samples, "NONE", "not compressed"))
            for i in range(n_samples):
                t = i / sample_rate
                decay = math.exp(-t * 22.0)
                freq = max(60.0, 520.0 - t * 2400.0)
                phase = (t * freq) % 1.0
                square = 1.0 if phase < 0.5 else -1.0
                noise = (random.random() * 2.0 - 1.0) * max(0.0, 1.0 - t * 25.0)
                val = int((square * 0.6 + noise * 0.4) * decay * 26000.0)
                wav.writeframes(struct.pack("<h", max(-32767, min(32767, val))))

    elif sound_type == "heal":
        duration = 0.35
        n_samples = int(sample_rate * duration)
        notes = [523.25, 659.25, 783.99, 1046.50]
        with wave.open(filepath, "w") as wav:
            wav.setparams((1, 2, sample_rate, n_samples, "NONE", "not compressed"))
            for i in range(n_samples):
                t = i / sample_rate
                note_idx = min(3, int(t / 0.08))
                freq = notes[note_idx]
                decay = math.exp(-(t % 0.08) * 12.0)
                phase = (t * freq) % 1.0
                square = 1.0 if phase < 0.25 else -1.0
                val = int(square * decay * 22000.0)
                wav.writeframes(struct.pack("<h", max(-32767, min(32767, val))))

    elif sound_type == "ougi":
        duration = 0.45
        n_samples = int(sample_rate * duration)
        with wave.open(filepath, "w") as wav:
            wav.setparams((1, 2, sample_rate, n_samples, "NONE", "not compressed"))
            for i in range(n_samples):
                t = i / sample_rate
                decay = math.exp(-t * 6.0)
                noise = (random.random() * 2.0 - 1.0)
                freq = max(40.0, 300.0 - t * 600.0)
                phase = (t * freq) % 1.0
                square = 1.0 if phase < 0.5 else -1.0
                val = int((noise * 0.7 + square * 0.3) * decay * 28000.0)
                wav.writeframes(struct.pack("<h", max(-32767, min(32767, val))))

    print(f"[Chiptune SE] 本格8bitレトロ音声を生成配置: {filepath} ({sound_type})")

def extract_required_assets():
    required = []
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, "r", encoding="utf-8") as f:
            data_text = f.read()
        for m in re.findall(r'"assets/([^"]+\.png)"', data_text):
            if m not in required:
                required.append(m)
    return required

def autonomous_asset_director():
    with open(SRC_FILE, "r", encoding="utf-8") as f:
        code_text = f.read()
    game_data_text = ""
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, "r", encoding="utf-8") as f:
            game_data_text = f.read()

    existing_files = [f for f in os.listdir(ASSET_DIR) if os.path.isfile(os.path.join(ASSET_DIR, f))]
    required_assets = extract_required_assets()
    missing_assets = [a for a in required_assets if not os.path.exists(os.path.join(ASSET_DIR, a))]

    update_status(
        phase="asset_planning",
        status="アセット不足分析・パラメータ構想中",
        thought="src/game_data.c と src/battle.c を照合し、不足している味方・敵・背景アセットと最適なSDパラメータを計算しています..."
    )

    system_prompt = (
        "あなたはレトロRPGのドット絵（Pixel Art）専門アートディレクターです。"
        "ゲームデータ(src/game_data.c)と既存のアセット一覧を確認し、ゲームが実際に参照しているのにまだ存在しない画像を最優先で生成してください。\n"
        "【厳守ルール】\n"
        "1. 勝手に未定義のファイル名を作らず、まずは『不足している必須アセット一覧』にあるファイル名（party_*.png, enemy_*.png, boss_*.png, bg_ch*.png）をそのまま指定すること。\n"
        "2. キャラクター／敵は必ず『1体単体・全身・中央配置・純白背景』にすること（スプライトシートやアイコン集は絶対NG）。\n"
        "3. SDXL解像度対応: キャラ/敵は width: 768, height: 768、背景(bg_ch*.png)は width: 1024, height: 512 を指定すること。\n"
        "4. positive_prompt（キャラ/敵）には必ず「solo, single character, full body, centered, pixel art, 16-bit, clean outline, pure white background」を含めること。\n"
        "5. negative_prompt には必ず「sprite sheet, grid, icon pack, multiple characters, collage, blurry, 3d, realistic, photo, deformed」を含めること。\n"
        "画像ごとに、なぜこの画像が必要なのか、なぜこのステップ数やCFGスケールにしたのかの【思考（thought）】を必ず明記してください。\n"
        "出力は必ず以下のJSON配列形式のみで行ってください（前置き・解説は一切禁止）：\n"
        "[\n"
        "  {\n"
        "    \"filename\": \"ファイル名.png\",\n"
        "    \"description\": \"用途の説明\",\n"
        "    \"thought\": \"なぜこの画像が必要で、なぜこのsteps/cfgにしたのかの思考・理由\",\n"
        "    \"positive_prompt\": \"英語プロンプト\",\n"
        "    \"negative_prompt\": \"除外用英語プロンプト\",\n"
        "    \"steps\": 35,\n"
        "    \"cfg_scale\": 7.5,\n"
        "    \"sampler_name\": \"Euler a\",\n"
        "    \"width\": 768,\n"
        "    \"height\": 768,\n"
        "    \"seed\": -1\n"
        "  }\n"
        "]\n"
        "不足がなければ空配列 [] を返してください。"
    )

    pending_inst = CONTROL_STATE.get("pending_instruction", "")
    instruction_header = ""
    if pending_inst:
        instruction_header = f"\n【★最重要・人間ディレクターからの緊急命令★】\n「{pending_inst}」\nこの命令を最優先で満たすようなアセットと最適なパラメータを必ず構想してください！\n"

    user_prompt = f"""{instruction_header}
【ゲームが参照している不足アセット（最優先ターゲット）】
{json.dumps(missing_assets, ensure_ascii=False)}

【src/game_data.c (キャラクター・章設定)】
{game_data_text[:2500]}

【src/battle.c (戦闘ロジック一部)】
{code_text[:1500]}

【現在 assets/ に存在するファイル一覧】
{json.dumps(existing_files, ensure_ascii=False)}

上記の『不足アセット』から1〜2件を選び、単体ドット絵（または背景）を生成するためのプロンプトとパラメータをJSON配列で出力してください。
"""

    print("[Director] Qwenが画像アセットの不足とプロンプト・パラメータを分析中...")
    resp = query_lmstudio(user_prompt, system=system_prompt)
    if not resp:
        return

    try:
        json_match = re.search(r"\[.*\]", resp, re.DOTALL)
        if not json_match:
            return
        tasks = json.loads(json_match.group(0))

        for t in tasks:
            fname = os.path.basename(t.get("filename", ""))
            if not fname:
                continue
            if not fname.endswith(".png"):
                fname += ".png"
            target = os.path.join(ASSET_DIR, fname)
            thought = t.get("thought", t.get("description", ""))
            steps = t.get("steps", 35)
            cfg_scale = t.get("cfg_scale", 7.5)
            sampler_name = t.get("sampler_name", "Euler a")
            width = t.get("width", 768)
            height = t.get("height", 768)
            seed = t.get("seed", -1)

            if not os.path.exists(target):
                print(f"\n[Director 決定] ドット絵生成: {fname} (Steps:{steps}, CFG:{cfg_scale}, Seed:{seed})")
                print(f"思考: {thought}")
                generate_pixel_image(
                    filename=fname,
                    positive_prompt=t.get("positive_prompt", ""),
                    negative_prompt=t.get("negative_prompt", ""),
                    steps=steps,
                    cfg_scale=cfg_scale,
                    sampler_name=sampler_name,
                    width=width,
                    height=height,
                    seed=seed,
                    thought=thought
                )
    except Exception as e:
        print(f"[Director 解析エラー] {e}")

def plan_next_improvement(pending_inst, missing_assets, memory):
    """これまでの改修履歴とバックログから、このサイクルで本当に実施すべき改善を決定する"""
    recent_completed = memory.get("completed", [])[:6]
    backlog = memory.get("backlog", [])

    system_prompt = (
        "あなたはCatRPG自律開発プロジェクトのテックリードです。\n"
        "これまでの改修履歴、現在のバックログ、人間からの指示を踏まえ、今回実施すべきアクションを1つだけ決めてください。\n"
        "【選択できるアクション】\n"
        "- \"battle\": src/battle.c の戦闘ロジック・AI行動・状態異常・バランスを改善する\n"
        "- \"game_data\": src/game_data.c の章・ストーリー・既存数値バランスを改善する\n"
        "- \"both\": 新スキル・新アイテム・新ギミックのように src/game_data.c と src/battle.c の両方を連携拡張する\n"
        "- \"skip\": 人間からの指示がなく、直近ですでに十分な改修が行われているため無駄な書き換えをせず待機する\n"
        "※ 人間からの指示がある場合は \"skip\" を選ばず、必ず実現に最適なアクションを選んでください。\n"
        "出力は必ず以下のJSONのみ（解説不要）：\n"
        "{\"action\": \"battle|game_data|both|skip\", \"goal\": \"今回の具体的な1〜2文の目標\", \"reason\": \"選定理由\", \"backlog\": [\"今後の改善案1\", \"今後の改善案2\", \"今後の改善案3\"]}"
    )

    user_prompt = f"""
【人間ディレクターからの指示】
{pending_inst if pending_inst else "（特になし）"}

【不足している必須アセット数】
{len(missing_assets)} 件 ({json.dumps(missing_assets[:5], ensure_ascii=False)})

【直近で完了した改修履歴（重複回避用）】
{json.dumps(recent_completed, ensure_ascii=False)}

【現在の改善バックログ】
{json.dumps(backlog, ensure_ascii=False)}
"""
    resp = query_lmstudio(user_prompt, system=system_prompt, max_tokens=1024)
    if resp:
        m = re.search(r"\{.*\}", resp, re.DOTALL)
        if m:
            try:
                plan = json.loads(m.group())
                action = plan.get("action", "battle")
                if action not in ("battle", "game_data", "both", "skip"):
                    action = "battle"
                if pending_inst and action == "skip":
                    action = "both"
                return {
                    "action": action,
                    "goal": plan.get("goal", "戦闘システムとバランスを洗練する"),
                    "reason": plan.get("reason", ""),
                    "backlog": plan.get("backlog", backlog),
                }
            except Exception:
                pass

    return {
        "action": "both" if pending_inst else "battle",
        "goal": pending_inst or "戦闘の駆け引きと状態異常・AI行動のバリエーションを強化する",
        "reason": "フォールバック計画",
        "backlog": backlog,
    }

def extract_code_blocks(resp, default_target):
    """LLM応答から複数ファイル（### FILE: path）または単一コードブロックを抽出する"""
    extracted = {}
    file_sections = re.split(r"###\s*FILE:\s*([^\r\n]+)", resp)
    if len(file_sections) >= 3:
        for i in range(1, len(file_sections), 2):
            fpath = file_sections[i].strip().replace("\\", "/")
            body = file_sections[i + 1]
            m = re.search(r"```(?:c)?\s*(.*?)```", body, re.DOTALL)
            if m and fpath in (SRC_FILE, DATA_FILE):
                extracted[fpath] = m.group(1).strip()

    if not extracted:
        if "```c" in resp:
            code = resp.split("```c")[1].split("```")[0].strip()
        elif "```" in resp:
            code = resp.split("```")[1].split("```")[0].strip()
        else:
            code = resp.strip()
        if code:
            extracted[default_target] = code
    return extracted

def validate_generated_c_contracts(files_map):
    """LLMが誤って必須シンボルや構造を消していないか事前検査する"""
    if not files_map:
        return False, "出力からC言語コードブロックを抽出できませんでした。"
    if DATA_FILE in files_map:
        d = files_map[DATA_FILE]
        for sym in ["GetChapter", "GetTotalChapters", "GetPartyMember", "GetSkill", "GetTotalSkills", "GetItemList", "GetTotalItems", "ResetItems", "GetEpilogueLineCount", "GetEpilogueLine"]:
            if sym not in d:
                return False, f"{DATA_FILE} に必須関数 {sym} が見つかりません。"
    if SRC_FILE in files_map:
        b = files_map[SRC_FILE]
        for sym in ["ClearLogs", "InitBattle", "ExecutePlayerAction", "UseBattleItem", "ExecuteEnemyTurn", "ProcessTurnEnd", "AddLog", "GetLogLine", "GetLogCount"]:
            if sym not in b:
                return False, f"{SRC_FILE} に必須関数 {sym} が見つかりません。"
    return True, ""

def autonomous_code_expansion():
    pending_inst = CONTROL_STATE.get("pending_instruction", "")
    missing_assets = [a for a in extract_required_assets() if not os.path.exists(os.path.join(ASSET_DIR, a))]
    memory = load_dev_memory()

    update_status(
        phase="planning",
        status="改善プラン策定中（履歴＆バックログ照合）",
        thought="過去の改修履歴と未解決課題を確認し、このサイクルの改善方針を決定しています..."
    )
    plan = plan_next_improvement(pending_inst, missing_assets, memory)
    action = plan["action"]
    goal = plan["goal"]

    if action == "skip":
        print(f"[Planner] 今回は変更をスキップします: {plan.get('reason', '安定状態')}")
        update_status(
            phase="standby",
            status="現状維持（スキップ判定）",
            thought=f"【計画判定】{plan.get('reason', 'システムが安定しているため不要な書き換えを見送りました。')}"
        )
        return

    with open(SRC_FILE, "r", encoding="utf-8") as f:
        orig_battle_code = f.read()
    with open(DATA_FILE, "r", encoding="utf-8") as f:
        orig_data_code = f.read()

    headers_context = ""
    for hfile in ["src/common.h", "src/battle.h", "src/game_data.h"]:
        if os.path.exists(hfile):
            with open(hfile, "r", encoding="utf-8") as hf:
                headers_context += f"\n--- {hfile} ---\n{hf.read()}\n"

    existing_assets = os.listdir(ASSET_DIR)
    default_target = DATA_FILE if action == "game_data" else SRC_FILE

    update_status(
        phase="coding",
        status=f"Cコード改修中 ({action}): {goal[:32]}",
        thought=f"【目標】{goal}\n【選定理由】{plan.get('reason', '')}"
    )

    if action == "both":
        target_guide = (
            "今回は【src/game_data.c】と【src/battle.c】の両方を連携更新できます。\n"
            "新スキルや新アイテム、新章などを game_data.c に追加し、対応する戦闘ロジックを battle.c に実装してください。\n"
            "必ず各ファイルの前に `### FILE: src/game_data.c` と `### FILE: src/battle.c` を付け、それぞれ ```c ... ``` で全文を出力してください。"
        )
    elif action == "game_data":
        target_guide = (
            "今回は【src/game_data.c】の拡張を担当してください（章・敵ステータス・スキル説明・アイテム・エピローグ等）。\n"
            "既存の関数シグネチャを維持し、```c ... ``` で src/game_data.c の全文を出力してください。"
        )
    else:
        target_guide = (
            "今回は【src/battle.c】の戦闘ロジック・AI行動・状態異常・ギミック拡張を担当してください。\n"
            "```c ... ``` で src/battle.c の全文を出力してください。"
        )

    system_prompt = (
        "あなたはRaylib RPGゲームの卓越したC言語(C99)プログラマーです。\n"
        f"{target_guide}\n"
        "【厳格な遵守ルール】\n"
        "1. common.h / battle.h / game_data.h に定義された構造体メンバ・関数プロトタイプを破壊・削除しないこと。\n"
        "2. 既存の全章がクリア可能（ゲームバランスが破綻しない）であることを保つこと。\n"
        "3. -Wall -Wextra -std=c99 でエラー・警告が出ない正しいC99コードを書くこと。\n"
        "【重要フォーマット】\n"
        "コードの前に必ず以下のように改修意図を日本語で書いてください：\n"
        "【改修の思考・意図】\n"
        "（2〜3文で説明）\n"
    )

    instruction_header = ""
    if pending_inst:
        instruction_header = f"\n【★最重要・人間ディレクターからの緊急命令★】\n「{pending_inst}」\nこの命令を最優先で具現化してください！\n"

    recent_summary = "\n".join(
        f"- [{c.get('time','')}] ({c.get('target','')}) {c.get('summary','')}"
        for c in memory.get("completed", [])[:5]
    ) or "（まだ履歴なし）"

    base_user_prompt = f"""{instruction_header}
【今回の目標】
{goal}

【過去の改修履歴（重複禁止）】
{recent_summary}

【参照ヘッダファイル群】
{headers_context}

【現在の src/game_data.c】
{orig_data_code}

【現在の src/battle.c】
{orig_battle_code}

【現在利用可能なアセット一覧】
{json.dumps(existing_assets, ensure_ascii=False)}

目標に沿って安全かつ面白い拡張を実装してください。
"""

    user_prompt = base_user_prompt
    max_repair = 3
    backup_created = False

    for attempt in range(max_repair):
        print(f"[Coder] LLMがコード改修案を策定中 ({action})... (試行 {attempt + 1}/{max_repair})")
        resp = query_lmstudio(user_prompt, system=system_prompt, max_tokens=8000)
        if not resp:
            return

        code_thought = goal
        if "【改修の思考・意図】" in resp:
            part = resp.split("【改修の思考・意図】")[1]
            code_thought = part.split("### FILE:")[0].split("```")[0].strip() or goal

        extracted = extract_code_blocks(resp, default_target)
        valid, val_err = validate_generated_c_contracts(extracted)
        if not valid:
            stderr = val_err
            new_dump = resp[:3000]
        else:
            if not backup_created:
                backup_code()
                backup_created = True

            for fpath, content in extracted.items():
                with open(fpath, "w", encoding="utf-8") as f:
                    f.write(content)

            update_status(
                phase="compiling",
                status=f"GCCコンパイル中 (試行 {attempt + 1}/{max_repair})",
                thought=f"【コード改修思考】{code_thought}"
            )

            success, stderr = compile_game(STAGING_EXE)
            if success:
                unit_ok, unit_msg = run_unit_tests()
                if not unit_ok:
                    stderr = unit_msg
                else:
                    print(f"✔ 単体テスト通過: {unit_msg}")
                    update_status(
                        phase="smoke_test",
                        status="スモークテスト＆画面検査中",
                        thought="ビルドと全章オートプレイテストを通過。実機起動検証を実行中..."
                    )
                    smoke_ok, smoke_msg = run_smoke_test(STAGING_EXE)
                    if smoke_ok:
                        promote_staged_exe(STAGING_EXE, EXE_FILE)
                        print(f"✔ ビルド＆全テスト通過: {EXE_FILE} を更新しました。")
                        now_str = time.strftime("%H:%M:%S")

                        changed_list = ", ".join(extracted.keys())
                        title_text = f"自律拡張 ({changed_list})"
                        if pending_inst:
                            title_text = f"👨‍💻 人間指示を具現化: {pending_inst[:25]}"
                            CONTROL_STATE["pending_instruction"] = ""

                        record_completed_feature(changed_list, code_thought, plan.get("backlog"))
                        update_status(
                            phase="ready",
                            status="ゲーム拡張・ビルド完了",
                            thought=f"【改修完了】{code_thought}",
                            history_item={
                                "time": now_str,
                                "type": "code",
                                "title": title_text,
                                "detail": f"{code_thought[:80]}"
                            }
                        )
                        return
                    else:
                        stderr = smoke_msg
            new_dump = "\n\n".join(f"### FILE: {k}\n{v}" for k, v in extracted.items())

        if os.path.exists(STAGING_EXE):
            try:
                os.remove(STAGING_EXE)
            except Exception:
                pass

        print(f"✖ ビルド／検証エラー発生:\n{stderr}")
        update_status(
            phase="repairing",
            status=f"エラー自己修復中 ({attempt + 1}/{max_repair})",
            thought=f"エラー検知: {stderr[:120]}... 修正コードを再生成しています。"
        )
        user_prompt = f"""{instruction_header}
【参照ヘッダファイル群】
{headers_context}

先ほどのコードで検証エラーが発生しました。
【エラー内容】
{stderr}

【エラーが出たコード】
{new_dump}

上記のエラーを完全に解消し、【改修の思考・意図】を添えて完全なC言語コード全文を出力してください。
"""
        time.sleep(2)

    print("[Rollback] 修復上限に達したため、直前のコードに巻き戻します。")
    with open(SRC_FILE, "w", encoding="utf-8") as f:
        f.write(orig_battle_code)
    with open(DATA_FILE, "w", encoding="utf-8") as f:
        f.write(orig_data_code)
    if pending_inst:
        CONTROL_STATE["pending_instruction"] = ""
    ok, _ = compile_game(STAGING_EXE)
    if ok:
        promote_staged_exe(STAGING_EXE, EXE_FILE)
    update_status(
        phase="rollback",
        status="安定コードへロールバック",
        thought="自己修復の上限に達したため、直前の安定したコードへ自動復旧しました。"
    )

def autonomous_loop(open_browser=False):
    print("==================================================")
    print(" Qwen制御型 自律ゲーム開発デーモン稼働 (SD WebUI版)")
    print(f" - LM Studio API: {LMSTUDIO_API}")
    print(f" - SD WebUI API : {SD_WEBUI_API}")
    print(f" - ゲーム本体   : C言語 (Raylib) -> {EXE_FILE}")
    print(f" - ダッシュボード: http://127.0.0.1:{DASHBOARD_PORT}/dashboard.html")
    print("==================================================")

    start_dev_server()
    if open_browser:
        try:
            webbrowser.open(f"http://127.0.0.1:{DASHBOARD_PORT}/dashboard.html")
        except Exception:
            pass

    generate_retro_wav(os.path.join(ASSET_DIR, "se_hit.wav"), "hit")
    generate_retro_wav(os.path.join(ASSET_DIR, "se_heal.wav"), "heal")
    generate_retro_wav(os.path.join(ASSET_DIR, "se_ougi.wav"), "ougi")

    organize_existing_assets()

    if not os.path.exists(EXE_FILE):
        print("[初期化] 初回コンパイルを実行します...")
        compile_game(EXE_FILE)

    update_status("ready", "自律開発エンジン待機中", thought="システム準備完了。自律サイクルを開始します。")

    cycle = 1
    while True:
        while CONTROL_STATE["is_paused"] and not CONTROL_STATE["force_next"]:
            update_status("paused", "⏸️ 一時停止中（ダッシュボードから再開可能）", thought="開発ループを一時停止しています。命令や設定を変更できます。")
            time.sleep(1)

        CONTROL_STATE["force_next"] = False
        print(f"\n===== [開発サイクル {cycle}] =====")
        autonomous_asset_director()
        autonomous_code_expansion()
        cycle += 1

        print(f"\n[待機] 次のイテレーションまで{CYCLE_WAIT_SEC}秒待機（ダッシュボードから即時実行可能）...")
        update_status(
            phase="standby",
            status=f"待機中（次回サイクル: #{cycle}）",
            thought=f"サイクル #{cycle-1} が完了しました。{CYCLE_WAIT_SEC}秒間ゲームの様子を監視し、次回サイクルへ進みます。"
        )

        for _ in range(CYCLE_WAIT_SEC):
            if CONTROL_STATE["force_next"] or CONTROL_STATE["is_paused"]:
                break
            time.sleep(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CatRPG AI Autonomous Development Engine")
    parser.add_argument("--open-browser", action="store_true", help="ダッシュボードをブラウザで自動で開く")
    args = parser.parse_args()
    autonomous_loop(open_browser=args.open_browser)
