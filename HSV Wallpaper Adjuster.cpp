// ==WindhawkMod==
// @id              hsv-wallpaper-adjuster
// @name            HSV Wallpaper Adjuster
// @description     A floating widget for customizing the colours of your desktop wallpaper.
// @version         1.3.0
// @author          Community
// @include         windhawk.exe
// @compilerOptions -lgdiplus -luser32 -lcomctl32 -lshell32 -lole32 -lgdi32 -lshlwapi -lcomdlg32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# HSV Wallpaper Adjuster

A floating widget that lets you customize the colours of your desktop wallpaper without needing an external image editor.

## Features

- Fine-tune hue, saturation, and brightness with an instant preview.
- Edit your current wallpaper or load a custom image.
- Save the adjusted image to a file or apply it directly as your wallpaper.
- Adjust the output scale to control the resulting image dimensions, only supports downscaling.

## Theming

- Choose from many different themes (system switches between Light and Dark).
- Accent colour can be customised to use system color or one of 8 presets
- A custom font family can be used (leave blank for default).

## Behaviour

- Open the widget with **Ctrl + Alt + H** (default hotkey).
- Automatically hides when not needed.
- Draggable.
- **Esc** hides the widget.

![HSV Wallpaper Adjuster in action](https://example.com/screenshot.png)
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Theme: dark
  $name: Theme
  $options:
  - dark: Dark
  - light: Light
  - darker: OLED Black
  - nord: Nord
  - gruvbox: Gruvbox
  - solarized: Solarized Dark
  - dracula: Dracula
  - tokyonight: Tokyo Night
  - catppuccin: Catppuccin Mocha
  - rosepine: Rosé Pine
  - kanagawa: Kanagawa
  - everforest: Everforest
  - midnight: Midnight
  - solarized-light: Solarized Light
  - github: GitHub Light
  - latte: Catppuccin Latte
  - gruvbox-light: Gruvbox Light
  - sepia: Sepia
  - system: Follow system (Light or Dark)
- AccentColor: system
  $name: Accent colour
  $options:
  - system: System accent
  - blue: Blue
  - teal: Teal
  - green: Green
  - purple: Purple
  - pink: Pink
  - red: Red
  - orange: Orange
  - yellow: Yellow
- FontFamily: ""
  $name: Font family
  $description: Leave empty to auto-detect Segoe UI Variable Text. Or type any installed font name.
- HotkeyModifiers: ctrl_alt
  $name: Hotkey modifiers
  $options:
  - ctrl_alt: Ctrl+Alt
  - ctrl_shift: Ctrl+Shift
  - alt_shift: Alt+Shift
  - win_alt: Win+Alt
  - ctrl_alt_shift: Ctrl+Alt+Shift
- HotkeyKey: H
  $name: Hotkey key
  $description: Single letter or digit (A-Z, 0-9)
- PreviewScale: 13
  $name: Preview scale (%)
  $description: How large the preview is, as a percentage of the original image.
- InitialScale: 100
  $name: Initial output scale (%)
  $description: Default scale applied when saving.
- ShowOnStart: 0
  $name: Show on startup
  $options:
  - 0: 'Off'
  - 1: 'On'
- SaveDirectory: ""
  $name: Default save directory
  $description: Leave empty to ask for a location each time.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <string>
#include <mutex>
#include <vector>

using namespace Gdiplus;
using namespace std;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")

// ============================================================
//  Layout (96 DPI logical units)
// ============================================================
namespace L {
    const int SHADOW = 20;
    const int W = 520;
    const int H = 604;
    const int WIN_W = W + SHADOW * 2;
    const int WIN_H = H + SHADOW * 2;
    const int PAD = 16;

    const int HEADER_H = 48;
    const int TITLE_X = 16;
    const int CLOSE_SIZE = 32;
    const int CLOSE_X = W - PAD - CLOSE_SIZE;
    const int CLOSE_Y = (HEADER_H - CLOSE_SIZE) / 2;

    const int PREVIEW_X = 16;
    const int PREVIEW_Y = 52;
    const int PREVIEW_W = W - 32;
    const int PREVIEW_H = 240;

    const int ROW_Y0 = 304;
    const int ROW_H = 50;
    const int LABEL_X = 16;
    const int LABEL_W = 90;
    const int SLIDER_X = 112;
    const int SLIDER_W = 260;
    const int SLIDER_TRACK_H = 4;
    const int VALUE_X = 378;
    const int VALUE_W = 55;
    const int RESET_X = 437;
    const int RESET_W = 32;
    const int RESET_H = 32;

    const int ROW1_Y = 516, ROW1_H = 32;
    const int ROW2_Y = 552, ROW2_H = 36;
    const int B1_W = 156;
    const int B1_1_X = 16,  B1_2_X = 182, B1_3_X = 348;
    const int B2_W1 = 236, B2_W2 = 236;
    const int B2_1_X = 16,  B2_2_X = 268;
}

// ============================================================
//  Widget IDs
// ============================================================
enum WidgetId {
    WID_NONE = -1,
    WID_CLOSE = 1,
    WID_BTN_LOAD_IMAGE = 100,
    WID_BTN_LOAD_WP    = 101,
    WID_BTN_RESET_ALL  = 102,
    WID_BTN_SAVE_WP    = 103,
    WID_BTN_SAVE_FILE  = 104,
    WID_RESET_HUE      = 200,
    WID_RESET_SAT      = 201,
    WID_RESET_VAL      = 202,
    WID_RESET_SCALE    = 203,
    WID_SLIDER_HUE     = 300,
    WID_SLIDER_SAT     = 301,
    WID_SLIDER_VAL     = 302,
    WID_SLIDER_SCALE   = 303,
};

static bool IsSlider(int id) { return id >= WID_SLIDER_HUE && id <= WID_SLIDER_SCALE; }

// ============================================================
//  Fluent icon glyphs (Segoe Fluent Icons / MDL2 Assets)
// ============================================================
#define GLYPH_RESET  L"\uE777"   // Undo
#define GLYPH_CLOSE  L"\uE8BB"   // ChromeClose

// ============================================================
//  Theme table — add new themes here, then in the YAML above.
// ============================================================
struct ThemePalette {
    const wchar_t* id;
    const wchar_t* label;
    Color bg, card, cardHover, cardPress, border, text, textDim, track;
};

static const ThemePalette kThemes[] = {
    { L"dark", L"Dark",
      Color(255,32,32,32), Color(255,43,43,43), Color(255,51,51,51),
      Color(255,38,38,38), Color(255,58,58,58), Color(255,255,255,255),
      Color(255,157,157,157), Color(255,64,64,64) },
    { L"darker", L"OLED Black",
      Color(255,10,10,10), Color(255,26,26,26), Color(255,34,34,34),
      Color(255,20,20,20), Color(255,38,38,38), Color(255,255,255,255),
      Color(255,128,128,128), Color(255,48,48,48) },
    { L"light", L"Light",
      Color(255,243,243,243), Color(255,255,255,255), Color(255,249,249,249),
      Color(255,239,239,239), Color(255,212,212,212), Color(255,26,26,26),
      Color(255,107,107,107), Color(255,216,216,216) },
    { L"nord", L"Nord",
      Color(255,46,52,64), Color(255,59,66,82), Color(255,67,76,94),
      Color(255,53,59,72), Color(255,76,86,106), Color(255,236,239,244),
      Color(255,155,165,180), Color(255,76,86,106) },
    { L"gruvbox", L"Gruvbox",
      Color(255,40,40,40), Color(255,60,56,54), Color(255,80,73,69),
      Color(255,50,48,47), Color(255,80,73,69), Color(255,235,219,178),
      Color(255,168,153,132), Color(255,80,73,69) },
      { L"solarized", L"Solarized Dark",
      Color(255,  0, 43, 54), Color(255,  7, 54, 66), Color(255, 20, 70, 82),
      Color(255,  0, 35, 45), Color(255, 30, 88,104), Color(255,238,232,213),
      Color(255,147,161,161), Color(255, 24, 74, 86) },
        // ── Saturated / characterful dark ─────────────────────────────────────
    { L"dracula", L"Dracula",
      Color(255, 40, 42, 54), Color(255, 54, 57, 74), Color(255, 98,114,164),
      Color(255, 34, 35, 46), Color(255, 98,114,164), Color(255,248,248,242),
      Color(255,189,147,249), Color(255, 68, 71, 90) },     // pink dim, purple hover

    { L"tokyonight", L"Tokyo Night",
      Color(255, 26, 27, 38), Color(255, 36, 40, 59), Color(255, 61, 89,161),
      Color(255, 22, 22, 30), Color(255, 61, 89,161), Color(255,192,202,245),
      Color(255,125,207,255), Color(255, 41, 46, 66) },     // sky-blue accents

    { L"catppuccin", L"Catppuccin Mocha",
      Color(255, 30, 30, 46), Color(255, 49, 50, 68), Color(255,203,166,247),
      Color(255, 24, 24, 37), Color(255,203,166,247), Color(255,205,214,244),
      Color(255,245,194,231), Color(255, 69, 71, 90) },     // mauve hover, pink dim

    { L"rosepine", L"Rosé Pine",
      Color(255, 25, 23, 36), Color(255, 38, 35, 58), Color(255,235,188,186),
      Color(255, 18, 17, 26), Color(255,235,188,186), Color(255,224,222,244),
      Color(255,196,167,231), Color(255, 38, 35, 58) },     // rose hover, iris dim

    { L"kanagawa", L"Kanagawa",
      Color(255, 31, 31, 40), Color(255, 42, 42, 55), Color(255,126,156,216),
      Color(255, 22, 22, 30), Color(255,126,156,216), Color(255,220,215,186),
      Color(255,210,126,153), Color(255, 54, 54, 70) },     // wave-blue hover, rose dim

    { L"everforest", L"Everforest",
      Color(255, 45, 53, 59), Color(255, 52, 63, 68), Color(255,167,192,128),
      Color(255, 38, 45, 50), Color(255,127,187,179), Color(255,211,198,170),
      Color(255,219,188,127), Color(255, 61, 72, 77) },     // sage hover, teal border

    { L"midnight", L"Midnight",
      Color(255,  5,  6, 10), Color(255, 13, 16, 24), Color(255, 88,101,242),
      Color(255,  2,  3,  6), Color(255, 88,101,242), Color(255,230,237,247),
      Color(255,124,135,152), Color(255, 20, 26, 42) },     // indigo hover glow

    // ── Saturated / characterful light ────────────────────────────────────
    { L"solarized-light", L"Solarized Light",
      Color(255,253,246,227), Color(255,238,232,213), Color(255,181,137,  0),
      Color(255,222,215,194), Color(255,181,137,  0), Color(255, 88,110,117),
      Color(255,203, 75, 22), Color(255,213,205,182) },     // amber hover, orange dim

    { L"github", L"GitHub Light",
      Color(255,246,248,250), Color(255,255,255,255), Color(255, 84,174,255),
      Color(255,234,238,242), Color(255, 84,174,255), Color(255, 31, 35, 40),
      Color(255, 9,105,218), Color(255,216,222,228) },      // accent-blue hover

    { L"latte", L"Catppuccin Latte",
      Color(255,239,241,245), Color(255,255,255,255), Color(255,136, 57,239),
      Color(255,220,224,232), Color(255,136, 57,239), Color(255, 76, 79,105),
      Color(255,234,118,203), Color(255,204,208,218) },     // mauve hover, pink dim

    { L"gruvbox-light", L"Gruvbox Light",
      Color(255,251,241,199), Color(255,242,229,188), Color(255,215,153, 33),
      Color(255,224,207,166), Color(255,175, 58,  3), Color(255, 60, 56, 54),
      Color(255, 66,123, 88), Color(255,213,196,161) },     // yellow hover, orange press

    { L"sepia", L"Sepia",
      Color(255,244,236,216), Color(255,251,246,234), Color(255,214,158, 92),
      Color(255,230,219,194), Color(255,178,108, 40), Color(255, 59, 50, 38),
      Color(255,160,112, 64), Color(255,220,208,180) },     // amber-brown hover
};

// ============================================================
//  Accent presets
// ============================================================
struct AccentPreset { const wchar_t* id; Color color; };
static const AccentPreset kAccents[] = {
    { L"blue",   Color(255,   0, 120, 212) },
    { L"teal",   Color(255,   0, 153, 188) },
    { L"green",  Color(255,  16, 124,  16) },
    { L"purple", Color(255, 136,  90, 180) },
    { L"pink",   Color(255, 227,   0, 140) },
    { L"red",    Color(255, 232,  17,  35) },
    { L"orange", Color(255, 247,  99,  12) },
    { L"yellow", Color(255, 255, 185,   0) },
};

// ============================================================
//  Resolved current theme (filled by ApplyTheme)
// ============================================================
struct CurrentTheme {
    Color bg, card, cardHover, cardPress, border, text, textDim, track;
    Color accent, accentHover, accentPress;
    wstring fontUi;
    wstring fontIcon;
};
static CurrentTheme T;

// ============================================================
//  Settings
// ============================================================
struct Settings {
    wstring themeId      = L"dark";
    wstring accentId     = L"system";
    wstring fontFamily;                // empty = auto
    UINT    hotkeyModifiers = MOD_CONTROL | MOD_ALT;
    UINT    hotkeyVk     = 'H';
    float   previewScale = 0.13f;
    float   initialScale = 1.0f;
    bool    showOnStart  = false;
    wstring saveDirectory;
};

static Settings g_settings;
static mutex    g_settingsMutex;

static Settings GetSettingsCopy() {
    lock_guard<mutex> lock(g_settingsMutex);
    return g_settings;
}

static void LoadSettings() {
    Settings s;

    PCWSTR p;
    p = Wh_GetStringSetting(L"Theme");      s.themeId  = p ? p : L"dark";     Wh_FreeStringSetting(p);
    p = Wh_GetStringSetting(L"AccentColor");s.accentId = p ? p : L"system";   Wh_FreeStringSetting(p);
    p = Wh_GetStringSetting(L"FontFamily"); s.fontFamily = p ? p : L"";      Wh_FreeStringSetting(p);

    wstring modStr = L"ctrl_alt";
    p = Wh_GetStringSetting(L"HotkeyModifiers"); modStr = p ? p : L"ctrl_alt"; Wh_FreeStringSetting(p);
    if (modStr == L"ctrl_shift")      s.hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    else if (modStr == L"alt_shift")  s.hotkeyModifiers = MOD_ALT | MOD_SHIFT;
    else if (modStr == L"win_alt")    s.hotkeyModifiers = MOD_WIN | MOD_ALT;
    else if (modStr == L"ctrl_alt_shift") s.hotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT;
    else                              s.hotkeyModifiers = MOD_CONTROL | MOD_ALT;

    wstring keyStr = L"H";
    p = Wh_GetStringSetting(L"HotkeyKey"); keyStr = p ? p : L"H"; Wh_FreeStringSetting(p);
    s.hotkeyVk = 'H';
    if (keyStr.size() == 1) {
        wchar_t ch = towupper(keyStr[0]);
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) s.hotkeyVk = (UINT)ch;
    }

    int pv = Wh_GetIntSetting(L"PreviewScale");
    if (pv < 1) pv = 13; if (pv > 100) pv = 100;
    s.previewScale = pv / 100.0f;

    int is = Wh_GetIntSetting(L"InitialScale");
    if (is < 1) is = 100; if (is > 100) is = 100;
    s.initialScale = is / 100.0f;

    s.showOnStart = Wh_GetIntSetting(L"ShowOnStart") != 0;

    p = Wh_GetStringSetting(L"SaveDirectory");
    s.saveDirectory = p ? p : L""; Wh_FreeStringSetting(p);

    lock_guard<mutex> lock(g_settingsMutex);
    g_settings = s;
}

// ============================================================
//  Theme resolution helpers
// ============================================================
static bool IsSystemLightMode() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD val = 1, sz = sizeof(val), type = 0;
        RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type, (LPBYTE)&val, &sz);
        RegCloseKey(key);
        return val != 0;
    }
    return false;
}

static bool IsFontInstalled(const wstring& name) {
    if (name.empty()) return false;
    InstalledFontCollection fonts;
    INT count = fonts.GetFamilyCount();
    if (count <= 0) return false;
    vector<FontFamily> fams(count);
    INT found = 0;
    fonts.GetFamilies(count, fams.data(), &found);
    WCHAR familyName[LF_FACESIZE];
    for (INT i = 0; i < found; ++i) {
        if (fams[i].GetFamilyName(familyName) == Ok &&
            _wcsicmp(familyName, name.c_str()) == 0) return true;
    }
    return false;
}

static wstring ResolveUiFont(const wstring& requested) {
    if (!requested.empty() && IsFontInstalled(requested)) return requested;
    if (IsFontInstalled(L"Segoe UI Variable Text"))     return L"Segoe UI Variable Text";
    if (IsFontInstalled(L"Segoe UI Variable"))          return L"Segoe UI Variable";
    return L"Segoe UI";
}

static wstring ResolveIconFont() {
    if (IsFontInstalled(L"Segoe Fluent Icons")) return L"Segoe Fluent Icons";
    if (IsFontInstalled(L"Segoe MDL2 Assets"))  return L"Segoe MDL2 Assets";
    return L"Segoe UI";
}

static Color ResolveAccent(const wstring& id) {
    for (const auto& a : kAccents) {
        if (id == a.id) return a.color;
    }
    // System accent
    DWORD colorization = 0; BOOL opaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&colorization, &opaque))) {
        BYTE r = (BYTE)((colorization >> 16) & 0xFF);
        BYTE g = (BYTE)((colorization >>  8) & 0xFF);
        BYTE b = (BYTE)( colorization        & 0xFF);
        if (r + g + b < 60) { r = 0; g = 120; b = 212; }
        return Color(255, r, g, b);
    }
    return Color(255, 0, 120, 212);
}

static const ThemePalette* ResolvePalette(const wstring& id) {
    if (id == L"system") {
        const wchar_t* which = IsSystemLightMode() ? L"light" : L"dark";
        for (const auto& th : kThemes) if (wcscmp(th.id, which) == 0) return &th;
    }
    for (const auto& th : kThemes) if (id == th.id) return &th;
    return &kThemes[0];   // dark fallback
}

// Rebuilds T from the current settings. Must be called after GdiplusStartup.
static void ApplyTheme() {
    Settings s = GetSettingsCopy();

    const ThemePalette* p = ResolvePalette(s.themeId);
    T.bg         = p->bg;
    T.card       = p->card;
    T.cardHover  = p->cardHover;
    T.cardPress  = p->cardPress;
    T.border     = p->border;
    T.text       = p->text;
    T.textDim    = p->textDim;
    T.track      = p->track;

    Color a = ResolveAccent(s.accentId);
    T.accent = a;
    T.accentHover = Color(255,
        min(255, (int)a.GetR() + 20),
        min(255, (int)a.GetG() + 20),
        min(255, (int)a.GetB() + 20));
    T.accentPress = Color(255,
        max(0, (int)a.GetR() - 15),
        max(0, (int)a.GetG() - 15),
        max(0, (int)a.GetB() - 15));

    T.fontUi   = ResolveUiFont(s.fontFamily);
    T.fontIcon = ResolveIconFont();
}

// ============================================================
//  Globals
// ============================================================
static HWND   g_hwnd = nullptr;
static HANDLE g_uiThread = nullptr;
static DWORD  g_uiThreadId = 0;
static bool   g_hotkeyRegistered = false;
static float  g_dpiScale = 1.0f;

static Bitmap* g_sourceBitmap    = nullptr;
static Bitmap* g_previewSource   = nullptr;
static Bitmap* g_previewRendered = nullptr;
static mutex   g_imageMutex;

static int  g_hue = 0, g_sat = 0, g_val = 0, g_scale = 100;
static int  g_activeSlider = WID_NONE;
static int  g_hoveredWidget = WID_NONE;
static int  g_pressedWidget = WID_NONE;

static bool  g_dragging = false;
static POINT g_dragStartCursor = {};
static POINT g_dragStartWindow = {};
static POINT g_lastWidgetPos = {-1, -1};
static bool  g_modalOpen = false;

static wstring g_statusMessage;
static DWORD   g_statusUntil = 0;

// ============================================================
//  Image loading
// ============================================================
static Bitmap* LoadImageFromMemory(const wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 250LL*1024*1024) {
        CloseHandle(h); return nullptr;
    }
    vector<BYTE> buf((size_t)sz.QuadPart);
    DWORD read = 0;
    BOOL ok = ReadFile(h, buf.data(), (DWORD)buf.size(), &read, nullptr);
    CloseHandle(h);
    if (!ok || read != buf.size()) return nullptr;

    IStream* stream = SHCreateMemStream(buf.data(), (UINT)buf.size());
    if (!stream) return nullptr;
    Bitmap* bmp = Bitmap::FromStream(stream);
    stream->Release();
    if (!bmp || bmp->GetLastStatus() != Ok || bmp->GetWidth() == 0) { delete bmp; return nullptr; }
    Rect r(0, 0, bmp->GetWidth(), bmp->GetHeight());
    Bitmap* standalone = bmp->Clone(r, PixelFormat32bppARGB);
    delete bmp;
    if (!standalone || standalone->GetLastStatus() != Ok) { delete standalone; return nullptr; }
    return standalone;
}

static Bitmap* LoadImageRobust(const wstring& path) {
    if (path.empty()) return nullptr;
    Wh_Log(L"Loading image: '%s'", path.c_str());
    Bitmap* loaded = Bitmap::FromFile(path.c_str());
    if (loaded && loaded->GetLastStatus() == Ok && loaded->GetWidth() > 0) {
        Rect r(0, 0, loaded->GetWidth(), loaded->GetHeight());
        Bitmap* standalone = loaded->Clone(r, PixelFormat32bppARGB);
        delete loaded;
        if (standalone && standalone->GetLastStatus() == Ok) return standalone;
        delete standalone;
    } else {
        delete loaded;
    }
    Bitmap* bmp = LoadImageFromMemory(path);
    if (bmp) Wh_Log(L"  memory fallback OK: %ux%u", bmp->GetWidth(), bmp->GetHeight());
    else     Wh_Log(L"  memory fallback failed");
    return bmp;
}

static void UpdateSourceAndPreview(Bitmap* newSource, float previewScale) {
    lock_guard<mutex> lock(g_imageMutex);
    delete g_sourceBitmap;  g_sourceBitmap = newSource;
    delete g_previewSource; g_previewSource = nullptr;
    if (g_sourceBitmap && g_sourceBitmap->GetWidth() > 0) {
        int pw = max(1, (int)(g_sourceBitmap->GetWidth()  * previewScale));
        int ph = max(1, (int)(g_sourceBitmap->GetHeight() * previewScale));
        g_previewSource = new Bitmap(pw, ph, PixelFormat32bppARGB);
        Graphics g(g_previewSource);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.DrawImage(g_sourceBitmap, 0, 0, pw, ph);
    }
}

// ============================================================
//  HSV transform
// ============================================================
static Bitmap* ApplyHSV(Bitmap* src, float hueShift, float satMul, float valMul) {
    if (!src) return nullptr;
    UINT w = src->GetWidth(), h = src->GetHeight();
    if (!w || !h) return nullptr;
    Rect rect(0, 0, w, h);
    BitmapData data;
    if (src->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) return nullptr;
    Bitmap* dst = new Bitmap(w, h, PixelFormat32bppARGB);
    BitmapData dd;
    if (dst->LockBits(&rect, ImageLockModeWrite, PixelFormat32bppARGB, &dd) != Ok) {
        src->UnlockBits(&data); delete dst; return nullptr;
    }
    int hueOffset = (int)((hueShift / 360.0f) * 255.0f);
    hueOffset = ((hueOffset % 256) + 256) % 256;
    BYTE* sp = (BYTE*)data.Scan0; BYTE* dp = (BYTE*)dd.Scan0;
    int ss = data.Stride, ds = dd.Stride;
    for (UINT y = 0; y < h; ++y) {
        BYTE* sr = sp + y * ss;
        BYTE* dr = dp + y * ds;
        for (UINT x = 0; x < w; ++x) {
            int i = x * 4;
            float b = sr[i+0]/255.0f, g = sr[i+1]/255.0f, r = sr[i+2]/255.0f;
            BYTE a = sr[i+3];
            float maxC = max(r, max(g,b)), minC = min(r, min(g,b));
            float delta = maxC - minC;
            int hi = 0, si = 0;
            if (delta > 0.0001f) {
                float S = delta / maxC;
                float H;
                if      (maxC == r) H = fmodf((g-b)/delta, 6.0f);
                else if (maxC == g) H = (b-r)/delta + 2.0f;
                else                H = (r-g)/delta + 4.0f;
                H *= 60.0f; if (H < 0) H += 360.0f;
                hi = (int)(H*255.0f/360.0f + 0.5f);
                si = (int)(S*255.0f + 0.5f);
            }
            int vi = (int)(maxC*255.0f + 0.5f);
            hi = (hi + hueOffset) & 0xFF;
            si = (int)(si*satMul + 0.5f); si = si<0?0:(si>255?255:si);
            vi = (int)(vi*valMul + 0.5f); vi = vi<0?0:(vi>255?255:vi);
            float H2 = hi*360.0f/255.0f, S2 = si/255.0f, V2 = vi/255.0f;
            float C = V2*S2;
            float X = C*(1.0f - fabsf(fmodf(H2/60.0f, 2.0f) - 1.0f));
            float m = V2 - C;
            float rp, gp, bp;
            if      (H2 < 60)  { rp=C; gp=X; bp=0; }
            else if (H2 < 120) { rp=X; gp=C; bp=0; }
            else if (H2 < 180) { rp=0; gp=C; bp=X; }
            else if (H2 < 240) { rp=0; gp=X; bp=C; }
            else if (H2 < 300) { rp=X; gp=0; bp=C; }
            else               { rp=C; gp=0; bp=X; }
            dr[i+0] = (BYTE)min(255, max(0, (int)((bp+m)*255.0f + 0.5f)));
            dr[i+1] = (BYTE)min(255, max(0, (int)((gp+m)*255.0f + 0.5f)));
            dr[i+2] = (BYTE)min(255, max(0, (int)((rp+m)*255.0f + 0.5f)));
            dr[i+3] = a;
        }
    }
    src->UnlockBits(&data);
    dst->UnlockBits(&dd);
    return dst;
}

static void UpdatePreview() {
    float satMul = (g_sat + 100) / 100.0f;
    float valMul = (g_val + 100) / 100.0f;
    lock_guard<mutex> lock(g_imageMutex);
    if (g_previewSource) {
        Bitmap* nr = ApplyHSV(g_previewSource, (float)g_hue, satMul, valMul);
        if (nr) { delete g_previewRendered; g_previewRendered = nr; }
    }
}

// ============================================================
//  Widget layout
// ============================================================
static RECT GetButtonRect(int id) {
    switch (id) {
        case WID_CLOSE:          return { L::CLOSE_X, L::CLOSE_Y, L::CLOSE_X + L::CLOSE_SIZE, L::CLOSE_Y + L::CLOSE_SIZE };
        case WID_BTN_LOAD_IMAGE: return { L::B1_1_X, L::ROW1_Y, L::B1_1_X + L::B1_W, L::ROW1_Y + L::ROW1_H };
        case WID_BTN_LOAD_WP:    return { L::B1_2_X, L::ROW1_Y, L::B1_2_X + L::B1_W, L::ROW1_Y + L::ROW1_H };
        case WID_BTN_RESET_ALL:  return { L::B1_3_X, L::ROW1_Y, L::B1_3_X + L::B1_W, L::ROW1_Y + L::ROW1_H };
        case WID_BTN_SAVE_WP:    return { L::B2_1_X, L::ROW2_Y, L::B2_1_X + L::B2_W1, L::ROW2_Y + L::ROW2_H };
        case WID_BTN_SAVE_FILE:  return { L::B2_2_X, L::ROW2_Y, L::B2_2_X + L::B2_W2, L::ROW2_Y + L::ROW2_H };
    }
    int rowIdx = -1;
    switch (id) {
        case WID_RESET_HUE:   rowIdx = 0; break;
        case WID_RESET_SAT:   rowIdx = 1; break;
        case WID_RESET_VAL:   rowIdx = 2; break;
        case WID_RESET_SCALE: rowIdx = 3; break;
    }
    if (rowIdx >= 0) {
        int rowY = L::ROW_Y0 + rowIdx * L::ROW_H;
        int y = rowY + (L::ROW_H - L::RESET_H) / 2;
        return { L::RESET_X, y, L::RESET_X + L::RESET_W, y + L::RESET_H };
    }
    return {0,0,0,0};
}

static RECT GetSliderTrackRect(int id) {
    int rowIdx = -1;
    switch (id) {
        case WID_SLIDER_HUE:   rowIdx = 0; break;
        case WID_SLIDER_SAT:   rowIdx = 1; break;
        case WID_SLIDER_VAL:   rowIdx = 2; break;
        case WID_SLIDER_SCALE: rowIdx = 3; break;
        default: return {0,0,0,0};
    }
    int rowY = L::ROW_Y0 + rowIdx * L::ROW_H;
    int ty = rowY + (L::ROW_H - L::SLIDER_TRACK_H) / 2;
    return { L::SLIDER_X, ty, L::SLIDER_X + L::SLIDER_W, ty + L::SLIDER_TRACK_H };
}

static RECT GetSliderHitRect(int id) {
    RECT tr = GetSliderTrackRect(id);
    return { tr.left - 12, tr.top - 22, tr.right + 12, tr.bottom + 22 };
}

static void GetSliderRange(int id, int* lo, int* hi) {
    switch (id) {
        case WID_SLIDER_HUE:   *lo = -180; *hi = 180; break;
        case WID_SLIDER_SAT:   *lo = -100; *hi = 100; break;
        case WID_SLIDER_VAL:   *lo = -100; *hi = 100; break;
        case WID_SLIDER_SCALE: *lo = 1;    *hi = 100; break;
    }
}

static int* GetSliderValuePtr(int id) {
    switch (id) {
        case WID_SLIDER_HUE:   return &g_hue;
        case WID_SLIDER_SAT:   return &g_sat;
        case WID_SLIDER_VAL:   return &g_val;
        case WID_SLIDER_SCALE: return &g_scale;
    }
    return nullptr;
}

static int HitTestWidget(int x, int y) {
    RECT c = GetButtonRect(WID_CLOSE);
    if (x >= c.left && x < c.right && y >= c.top && y < c.bottom) return WID_CLOSE;

    static const int buttons[] = {
        WID_BTN_LOAD_IMAGE, WID_BTN_LOAD_WP, WID_BTN_RESET_ALL,
        WID_BTN_SAVE_WP, WID_BTN_SAVE_FILE,
        WID_RESET_HUE, WID_RESET_SAT, WID_RESET_VAL, WID_RESET_SCALE
    };
    for (int id : buttons) {
        RECT r = GetButtonRect(id);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return id;
    }
    static const int sliders[] = {
        WID_SLIDER_HUE, WID_SLIDER_SAT, WID_SLIDER_VAL, WID_SLIDER_SCALE
    };
    for (int id : sliders) {
        RECT r = GetSliderHitRect(id);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return id;
    }
    return WID_NONE;
}

static bool InHeaderDragArea(int x, int y) {
    if (y < 0 || y >= L::HEADER_H) return false;
    RECT c = GetButtonRect(WID_CLOSE);
    if (x >= c.left && x < c.right && y >= c.top && y < c.bottom) return false;
    return true;
}

// ============================================================
//  Drawing helpers
// ============================================================
static void AddRoundedRect(GraphicsPath* p, REAL x, REAL y, REAL w, REAL h, REAL r) {
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    if (r < 0)   r = 0;
    REAL d = r * 2;
    p->AddArc(x,         y,         d, d, 180.0f, 90.0f);
    p->AddArc(x + w - d, y,         d, d, 270.0f, 90.0f);
    p->AddArc(x + w - d, y + h - d, d, d,   0.0f, 90.0f);
    p->AddArc(x,         y + h - d, d, d,  90.0f, 90.0f);
    p->CloseFigure();
}

static void DrawLabel(Graphics& g, REAL x, REAL y, REAL w, REAL h, const wchar_t* text,
                      const Color& color, float sizePt, bool bold,
                      StringAlignment align = StringAlignmentNear) {
    const wchar_t* family = T.fontUi.empty() ? L"Segoe UI" : T.fontUi.c_str();
    Font font(family, sizePt, bold ? FontStyleBold : FontStyleRegular, UnitPoint);
    SolidBrush brush(color);
    StringFormat sf;
    sf.SetAlignment(align);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(text, -1, &font, RectF(x, y, w, h), &sf, &brush);
}

static void DrawIcon(Graphics& g, REAL x, REAL y, REAL w, REAL h, const wchar_t* glyph,
                     const Color& color, float sizePt) {
    const wchar_t* family = T.fontIcon.empty() ? L"Segoe MDL2 Assets" : T.fontIcon.c_str();
    Font font(family, sizePt, FontStyleRegular, UnitPoint);
    SolidBrush brush(color);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(glyph, -1, &font, RectF(x, y, w, h), &sf, &brush);
}

static void DrawShadow(Graphics& g, REAL x, REAL y, REAL w, REAL h, REAL radius) {
    const int STEPS = 10;
    const REAL spread = (REAL)L::SHADOW;
    for (int i = STEPS; i >= 1; --i) {
        REAL t = (REAL)i / (REAL)STEPS;
        REAL grow = t * spread;
        int alpha = (int)((1.0f - t) * (1.0f - t) * 90.0f);
        if (alpha <= 2) continue;
        SolidBrush b(Color((BYTE)alpha, 0, 0, 0));
        GraphicsPath p;
        AddRoundedRect(&p, x - grow, y - grow + 2.0f, w + grow*2, h + grow*2, radius + grow);
        g.FillPath(&b, &p);
    }
}

static void DrawWidgetBackground(Graphics& g) {
    DrawShadow(g, 0, 0, (REAL)L::W, (REAL)L::H, 8.0f);
    GraphicsPath bgPath;
    AddRoundedRect(&bgPath, 0, 0, (REAL)L::W, (REAL)L::H, 8.0f);
    SolidBrush bg(T.bg);
    g.FillPath(&bg, &bgPath);
    Pen border(T.border, 1.0f);
    g.DrawPath(&border, &bgPath);
}

static void DrawHeader(Graphics& g) {
    DrawLabel(g, L::TITLE_X, 0, 300, L::HEADER_H,
              L"HSV Wallpaper Adjuster", T.text, 11.0f, true);

    RECT c = GetButtonRect(WID_CLOSE);
    bool hover = (g_hoveredWidget == WID_CLOSE);
    bool press = (g_pressedWidget == WID_CLOSE);
    if (hover || press) {
        Color bg = press ? Color(255, 196, 43, 28) : Color(255, 220, 50, 40);
        GraphicsPath p;
        AddRoundedRect(&p, (REAL)c.left, (REAL)c.top,
                       (REAL)(c.right - c.left), (REAL)(c.bottom - c.top), 6.0f);
        SolidBrush b(bg);
        g.FillPath(&b, &p);
    }
    DrawIcon(g, (REAL)c.left, (REAL)c.top,
             (REAL)(c.right - c.left), (REAL)(c.bottom - c.top),
             GLYPH_CLOSE, T.text, 9.0f);
}

static void DrawPreview(Graphics& g) {
    REAL x = L::PREVIEW_X, y = L::PREVIEW_Y;
    REAL w = L::PREVIEW_W, h = L::PREVIEW_H;

    GraphicsPath cardPath;
    AddRoundedRect(&cardPath, x, y, w, h, 8.0f);
    SolidBrush cardBg(T.card);
    g.FillPath(&cardBg, &cardPath);
    Pen border(T.border, 1.0f);
    g.DrawPath(&border, &cardPath);

    Bitmap* toDraw = nullptr;
    int srcW = 0, srcH = 0;
    {
        lock_guard<mutex> lock(g_imageMutex);
        toDraw = g_previewRendered;
        if (!toDraw) toDraw = g_previewSource;
        if (g_sourceBitmap) {
            srcW = (int)g_sourceBitmap->GetWidth();
            srcH = (int)g_sourceBitmap->GetHeight();
        }

        if (toDraw && toDraw->GetWidth() > 0) {
            REAL pw = (REAL)toDraw->GetWidth(), ph = (REAL)toDraw->GetHeight();
            REAL pad = 8.0f;
            REAL maxW = w - pad * 2, maxH = h - pad * 2;
            REAL s = min(maxW / pw, maxH / ph);
            REAL dw = pw * s, dh = ph * s;
            REAL dx = x + (w - dw) * 0.5f;
            REAL dy = y + (h - dh) * 0.5f;
            g.SetClip(&cardPath);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            g.DrawImage(toDraw, dx, dy, dw, dh);
            g.ResetClip();
        } else {
            DrawLabel(g, x, y, w, h,
                      L"No image loaded — click \"Current Wallpaper\"",
                      T.textDim, 10.5f, false, StringAlignmentCenter);
        }
    }

    // ── Size overlay (bottom-right of preview) ────────────────
    if (srcW > 0 && srcH > 0) {
        float scale = g_scale / 100.0f;
        int dstW = max(1, (int)(srcW * scale + 0.5f));
        int dstH = max(1, (int)(srcH * scale + 0.5f));

        wchar_t sizeBuf[96];
        if (g_scale == 100) {
            swprintf_s(sizeBuf, L"%d \u00D7 %d", srcW, srcH);
        } else {
            swprintf_s(sizeBuf, L"%d \u00D7 %d  \u2192  %d \u00D7 %d",
                       srcW, srcH, dstW, dstH);
        }

        const wchar_t* family = T.fontUi.empty() ? L"Segoe UI" : T.fontUi.c_str();
        Font sizeFont(family, 8.5f, FontStyleRegular, UnitPoint);
        RectF measure;
        g.MeasureString(sizeBuf, -1, &sizeFont, PointF(0, 0), &measure);

        REAL padX = 8.0f, padY = 3.0f;
        REAL pillW = measure.Width  + padX * 2;
        REAL pillH = measure.Height + padY * 2;
        REAL pillX = x + w - pillW - 12;
        REAL pillY = y + h - pillH - 12;

        GraphicsPath pillPath;
        AddRoundedRect(&pillPath, pillX, pillY, pillW, pillH, pillH * 0.5f);
        SolidBrush pillBg(Color(210, 0, 0, 0));
        g.FillPath(&pillBg, &pillPath);

        SolidBrush textBrush(Color(255, 255, 255, 255));
        g.DrawString(sizeBuf, -1, &sizeFont,
                     PointF(pillX + padX, pillY + padY), &textBrush);
    }

    if (!g_statusMessage.empty() && GetTickCount() < g_statusUntil) {
        DrawLabel(g, x, y + h - 26, w, 20, g_statusMessage.c_str(),
                  T.accent, 10.0f, true, StringAlignmentCenter);
    }
}

static void DrawButton(Graphics& g, int id, const wchar_t* text, bool accent) {
    RECT r = GetButtonRect(id);
    bool hover = (g_hoveredWidget == id);
    bool press = (g_pressedWidget == id);

    Color bg = accent ? T.accent : T.card;
    if (press)      bg = accent ? T.accentPress : T.cardPress;
    else if (hover) bg = accent ? T.accentHover : T.cardHover;

    REAL x = (REAL)r.left, y = (REAL)r.top;
    REAL w = (REAL)(r.right - r.left), h = (REAL)(r.bottom - r.top);

    GraphicsPath p;
    AddRoundedRect(&p, x, y, w, h, 4.0f);
    SolidBrush bgBrush(bg);
    g.FillPath(&bgBrush, &p);

    if (!accent) {
        Pen border(T.border, 1.0f);
        g.DrawPath(&border, &p);
    }
    // Text colour: on accent fill we always want bright text; on light themes
    // the accent fill is dark enough that white text works too.
    DrawLabel(g, x, y, w, h, text, T.text, 10.0f, false, StringAlignmentCenter);
}

static void DrawResetButton(Graphics& g, int resetId) {
    RECT r = GetButtonRect(resetId);
    bool hover = (g_hoveredWidget == resetId);
    bool press = (g_pressedWidget == resetId);
    REAL x = (REAL)r.left, y = (REAL)r.top;
    REAL w = (REAL)(r.right - r.left), h = (REAL)(r.bottom - r.top);

    if (hover || press) {
        GraphicsPath p;
        AddRoundedRect(&p, x, y, w, h, 4.0f);
        SolidBrush b(press ? T.cardPress : T.cardHover);
        g.FillPath(&b, &p);
    }
    Color c = (hover || press) ? T.text : T.textDim;
    DrawIcon(g, x, y, w, h, GLYPH_RESET, c, 11.0f);
}

static void DrawSlider(Graphics& g, int sliderId, const wchar_t* label, int resetId, int rowIdx) {
    RECT tr = GetSliderTrackRect(sliderId);
    int lo, hi; GetSliderRange(sliderId, &lo, &hi);
    int val = *GetSliderValuePtr(sliderId);
    int rowY = L::ROW_Y0 + rowIdx * L::ROW_H;

    DrawLabel(g, L::LABEL_X, rowY, L::LABEL_W, L::ROW_H,
              label, T.text, 10.5f, false, StringAlignmentNear);

    REAL trackX = (REAL)tr.left, trackY = (REAL)tr.top;
    REAL trackW = (REAL)(tr.right - tr.left), trackH = (REAL)(tr.bottom - tr.top);
    REAL trackCy = trackY + trackH * 0.5f;

    GraphicsPath bgPath;
    AddRoundedRect(&bgPath, trackX, trackY, trackW, trackH, trackH * 0.5f);
    SolidBrush bgBrush(T.track);
    g.FillPath(&bgBrush, &bgPath);

    float t = (float)(val - lo) / (float)(hi - lo);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    REAL thumbX = trackX + t * trackW;

    if (thumbX > trackX + 0.5f) {
        GraphicsPath fillPath;
        AddRoundedRect(&fillPath, trackX, trackY, thumbX - trackX, trackH, trackH * 0.5f);
        SolidBrush fillBrush(T.accent);
        g.FillPath(&fillBrush, &fillPath);
    }

    bool active = (g_activeSlider == sliderId);
    bool hover  = (g_hoveredWidget == sliderId);
    REAL thumbR = active ? 8.0f : (hover ? 7.5f : 7.0f);
    SolidBrush thumbBrush(T.text);
    g.FillEllipse(&thumbBrush, thumbX - thumbR, trackCy - thumbR, thumbR*2, thumbR*2);
    Pen ring(T.accent, 2.0f);
    g.DrawEllipse(&ring, thumbX - thumbR, trackCy - thumbR, thumbR*2, thumbR*2);

    wchar_t buf[32];
    switch (sliderId) {
        case WID_SLIDER_HUE:   swprintf_s(buf, L"%+d\u00B0", val); break;
        case WID_SLIDER_SAT:
        case WID_SLIDER_VAL:   swprintf_s(buf, L"%+d%%", val);     break;
        case WID_SLIDER_SCALE: swprintf_s(buf, L"%d%%",  val);     break;
        default: buf[0] = 0;
    }
    DrawLabel(g, L::VALUE_X, rowY, L::VALUE_W, L::ROW_H,
              buf, T.text, 10.5f, true, StringAlignmentFar);

    DrawResetButton(g, resetId);
}

static void DrawWidget(Graphics& g) {
    DrawWidgetBackground(g);
    DrawHeader(g);
    DrawPreview(g);
    DrawSlider(g, WID_SLIDER_HUE,   L"Hue",        WID_RESET_HUE,   0);
    DrawSlider(g, WID_SLIDER_SAT,   L"Saturation", WID_RESET_SAT,   1);
    DrawSlider(g, WID_SLIDER_VAL,   L"Value",      WID_RESET_VAL,   2);
    DrawSlider(g, WID_SLIDER_SCALE, L"Scale",      WID_RESET_SCALE, 3);
    DrawButton(g, WID_BTN_LOAD_IMAGE, L"Load Image",         false);
    DrawButton(g, WID_BTN_LOAD_WP,    L"Current Wallpaper",  false);
    DrawButton(g, WID_BTN_RESET_ALL,  L"Reset All",          false);
    DrawButton(g, WID_BTN_SAVE_WP,    L"Apply as Wallpaper", true);
    DrawButton(g, WID_BTN_SAVE_FILE,  L"Save File\u2026",    false);
}

// ============================================================
//  Layered rendering
// ============================================================
static void RenderLayered(HWND hwnd) {
    if (!hwnd) return;
    int pxW = (int)(L::WIN_W * g_dpiScale);
    int pxH = (int)(L::WIN_H * g_dpiScale);

    HDC screen = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!memDc) return;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = pxW;
    bi.bmiHeader.biHeight      = -pxH;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(memDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib) { DeleteDC(memDc); return; }
    HGDIOBJ oldBmp = SelectObject(memDc, dib);
    memset(bits, 0, (size_t)pxW * pxH * 4);

    {
        Graphics g(memDc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.ScaleTransform(g_dpiScale, g_dpiScale);
        g.TranslateTransform((REAL)L::SHADOW, (REAL)L::SHADOW);
        DrawWidget(g);
    }

    POINT ptSrc = {0, 0};
    SIZE  size  = {pxW, pxH};
    BLENDFUNCTION blend = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, nullptr, nullptr, &size, memDc, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDc, oldBmp);
    DeleteObject(dib);
    DeleteDC(memDc);
}

// ============================================================
//  Actions
// ============================================================
static void ShowStatus(const wstring& msg, DWORD ms = 3500) {
    g_statusMessage = msg;
    g_statusUntil = GetTickCount() + ms;
    if (g_hwnd) RenderLayered(g_hwnd);
}

static void ForceForeground(HWND hwnd) {
    DWORD fg = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD us = GetCurrentThreadId();
    if (fg != us) {
        AttachThreadInput(fg, us, TRUE);
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
        AttachThreadInput(fg, us, FALSE);
    } else {
        SetForegroundWindow(hwnd);
    }
    SetFocus(hwnd);
}

static void PositionOnActiveMonitor(HWND hwnd) {
    POINT pt; GetCursorPos(&pt);
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);
    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - (int)(L::WIN_W * g_dpiScale)) / 2;
    int y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - (int)(L::WIN_H * g_dpiScale)) / 2;
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

static void ShowWidget(HWND hwnd) {
    if (g_lastWidgetPos.x < 0) {
        PositionOnActiveMonitor(hwnd);
        RECT wr; GetWindowRect(hwnd, &wr);
        g_lastWidgetPos = { wr.left, wr.top };
    } else {
        SetWindowPos(hwnd, HWND_TOPMOST, g_lastWidgetPos.x, g_lastWidgetPos.y,
                     0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    ShowWindow(hwnd, SW_SHOW);
    ForceForeground(hwnd);
    RenderLayered(hwnd);
}

static void HideWidget(HWND hwnd) {
    RECT wr; GetWindowRect(hwnd, &wr);
    g_lastWidgetPos = { wr.left, wr.top };
    ShowWindow(hwnd, SW_HIDE);
}

static bool SaveProcessed(const wstring& outPath, int hueVal, int satVal, int valVal, int scaleVal) {
    Bitmap* source = nullptr;
    {
        lock_guard<mutex> lock(g_imageMutex);
        if (g_sourceBitmap) {
            Rect r(0, 0, g_sourceBitmap->GetWidth(), g_sourceBitmap->GetHeight());
            source = g_sourceBitmap->Clone(r, PixelFormat32bppARGB);
        }
    }
    if (!source) { ShowStatus(L"Load an image first."); return false; }

    Bitmap* processed = ApplyHSV(source, (float)hueVal,
                                 (satVal + 100) / 100.0f,
                                 (valVal + 100) / 100.0f);
    delete source;
    if (!processed) { ShowStatus(L"HSV processing failed."); return false; }

    Bitmap* final = processed;
    float outScale = scaleVal / 100.0f;
    if (outScale < 0.999f) {
        int nw = max(1, (int)(processed->GetWidth()  * outScale));
        int nh = max(1, (int)(processed->GetHeight() * outScale));
        final = new Bitmap(nw, nh, PixelFormat32bppARGB);
        Graphics g(final);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.DrawImage(processed, 0, 0, nw, nh);
        delete processed;
    }

    CLSID clsid;
    const wchar_t* ext = PathFindExtensionW(outPath.c_str());
    bool jpeg = ext && (_wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0);
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    bool found = false;
    if (size) {
        vector<BYTE> bbuf(size);
        ImageCodecInfo* info = (ImageCodecInfo*)bbuf.data();
        GetImageEncoders(num, size, info);
        const wchar_t* want = jpeg ? L"image/jpeg" : L"image/png";
        for (UINT i = 0; i < num; ++i) {
            if (wcscmp(info[i].MimeType, want) == 0) { clsid = info[i].Clsid; found = true; break; }
        }
    }
    if (!found) { delete final; ShowStatus(L"No image encoder available."); return false; }

    Status st = final->Save(outPath.c_str(), &clsid, nullptr);
    delete final;
    if (st != Ok) { ShowStatus(L"Failed to save image."); return false; }
    return true;
}

static void DoLoadImage(HWND hwnd) {
    wchar_t path[MAX_PATH] = {};
    Settings s = GetSettingsCopy();

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Image files\0*.png;*.jpg;*.jpeg;*.bmp;*.webp\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!s.saveDirectory.empty()) ofn.lpstrInitialDir = s.saveDirectory.c_str();

    g_modalOpen = true;
    BOOL ok = GetOpenFileNameW(&ofn);
    g_modalOpen = false;
    if (!ok) { RenderLayered(hwnd); return; }

    Bitmap* loaded = LoadImageRobust(path);
    if (!loaded) {
        g_modalOpen = true;
        MessageBoxW(hwnd, L"Failed to load image.", L"Error", MB_ICONERROR);
        g_modalOpen = false;
        RenderLayered(hwnd);
        return;
    }
    UpdateSourceAndPreview(loaded, s.previewScale);
    g_hue = 0; g_sat = 0; g_val = 0;
    UpdatePreview();
    RenderLayered(hwnd);
}

static void DoLoadWallpaper(HWND hwnd, bool silent) {
    wchar_t path[MAX_PATH] = {};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0)) {
        if (!silent) {
            g_modalOpen = true;
            MessageBoxW(hwnd, L"Could not query the current wallpaper.", L"Error", MB_ICONERROR);
            g_modalOpen = false;
        }
        return;
    }
    if (!path[0]) {
        if (!silent) {
            g_modalOpen = true;
            MessageBoxW(hwnd, L"No current wallpaper file (this is normal for a solid-colour background).",
                        L"No wallpaper", MB_ICONWARNING);
            g_modalOpen = false;
        }
        return;
    }
    Bitmap* loaded = LoadImageRobust(path);
    if (!loaded) {
        if (!silent) {
            wstring msg = L"Failed to load the current wallpaper.\n\nPath:\n";
            msg += path; msg += L"\n\nSee the Windhawk mod log for details.";
            g_modalOpen = true;
            MessageBoxW(hwnd, msg.c_str(), L"Error", MB_ICONERROR);
            g_modalOpen = false;
        }
        return;
    }
    Settings s = GetSettingsCopy();
    UpdateSourceAndPreview(loaded, s.previewScale);
    g_hue = 0; g_sat = 0; g_val = 0;
    UpdatePreview();
    RenderLayered(hwnd);
    if (!silent) ShowStatus(L"Loaded current wallpaper.");
}

static void DoSaveWallpaper(HWND hwnd) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    wstring out = wstring(tempPath) + L"hsv_wallpaper_adjusted.png";
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    bool ok = SaveProcessed(out, g_hue, g_sat, g_val, g_scale);
    SetCursor(old);
    if (!ok) { RenderLayered(hwnd); return; }
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (void*)out.c_str(),
                               SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
        g_modalOpen = true;
        MessageBoxW(hwnd, L"Failed to set wallpaper.", L"Error", MB_ICONERROR);
        g_modalOpen = false;
        RenderLayered(hwnd);
        return;
    }
    ShowStatus(L"Wallpaper applied.");
}

static void DoSaveFile(HWND hwnd) {
    wchar_t path[MAX_PATH] = {};
    Settings s = GetSettingsCopy();
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"PNG Image\0*.png\0JPEG Image\0*.jpg\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!s.saveDirectory.empty()) ofn.lpstrInitialDir = s.saveDirectory.c_str();
    g_modalOpen = true;
    BOOL ok = GetSaveFileNameW(&ofn);
    g_modalOpen = false;
    if (!ok) { RenderLayered(hwnd); return; }
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    bool saved = SaveProcessed(path, g_hue, g_sat, g_val, g_scale);
    SetCursor(old);
    if (saved) ShowStatus(L"Saved.");
    else RenderLayered(hwnd);
}

static void DoResetAll(HWND hwnd) {
    g_hue = 0; g_sat = 0; g_val = 0;
    Settings s = GetSettingsCopy();
    g_scale = (int)(s.initialScale * 100);
    UpdatePreview();
    RenderLayered(hwnd);
}

static void ApplyHotkey(HWND hwnd) {
    if (g_hotkeyRegistered) { UnregisterHotKey(hwnd, 1); g_hotkeyRegistered = false; }
    Settings s = GetSettingsCopy();
    if (s.hotkeyVk != 0) {
        if (RegisterHotKey(hwnd, 1, s.hotkeyModifiers | MOD_NOREPEAT, s.hotkeyVk))
            g_hotkeyRegistered = true;
        else
            Wh_Log(L"Hotkey registration FAILED (error %lu)", GetLastError());
    }
}

// ============================================================
//  Window proc
// ============================================================
static void UpdateSliderFromMouse(int sliderId, int mouseX) {
    RECT tr = GetSliderTrackRect(sliderId);
    int lo, hi; GetSliderRange(sliderId, &lo, &hi);
    float t = (float)(mouseX - tr.left) / (float)(tr.right - tr.left);
    if (t < 0) t = 0; if (t > 1) t = 1;
    int v = lo + (int)(t * (hi - lo) + 0.5f);
    int* p = GetSliderValuePtr(sliderId);
    if (*p != v) {
        *p = v;
        UpdatePreview();
        RenderLayered(g_hwnd);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            ApplyHotkey(hwnd);
            PostMessageW(hwnd, WM_APP + 1, 0, 0);
            return 0;

        case WM_APP + 1:
            DoLoadWallpaper(hwnd, /*silent=*/true);
            return 0;

        case WM_APP + 2: {  // settings reload
            Settings s = GetSettingsCopy();
            Bitmap* clone = nullptr;
            {
                lock_guard<mutex> lock(g_imageMutex);
                if (g_sourceBitmap) {
                    Rect r(0, 0, g_sourceBitmap->GetWidth(), g_sourceBitmap->GetHeight());
                    clone = g_sourceBitmap->Clone(r, PixelFormat32bppARGB);
                }
            }
            if (clone) {
                UpdateSourceAndPreview(clone, s.previewScale);
                UpdatePreview();
            }
            ApplyTheme();        // <-- re-resolve palette / accent / fonts
            ApplyHotkey(hwnd);
            RenderLayered(hwnd);
            return 0;
        }

        case WM_SETTINGCHANGE: {
            // System theme switched (ImmersiveColorSet); re-resolve in case
            // the user picked "Follow system".
            if (lParam && wcscmp((const wchar_t*)lParam, L"ImmersiveColorSet") == 0) {
                Settings s = GetSettingsCopy();
                if (s.themeId == L"system" || s.accentId == L"system") {
                    ApplyTheme();
                    if (g_hwnd) RenderLayered(g_hwnd);
                }
            }
            return 0;
        }

        case WM_HOTKEY:
            if (wParam == 1) {
                if (IsWindowVisible(hwnd)) HideWidget(hwnd);
                else                        ShowWidget(hwnd);
            }
            return 0;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && !g_modalOpen && !g_dragging)
                HideWidget(hwnd);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) HideWidget(hwnd);
            return 0;

        case WM_LBUTTONDOWN: {
            int wx = (int)(GET_X_LPARAM(lParam) / g_dpiScale) - L::SHADOW;
            int wy = (int)(GET_Y_LPARAM(lParam) / g_dpiScale) - L::SHADOW;
            if (InHeaderDragArea(wx, wy)) {
                g_dragging = true;
                SetCapture(hwnd);
                GetCursorPos(&g_dragStartCursor);
                RECT wr; GetWindowRect(hwnd, &wr);
                g_dragStartWindow = { wr.left, wr.top };
                return 0;
            }
            int w = HitTestWidget(wx, wy);
            if (w != WID_NONE) {
                if (IsSlider(w)) {
                    g_activeSlider = w;
                    SetCapture(hwnd);
                    UpdateSliderFromMouse(w, wx);
                } else {
                    g_pressedWidget = w;
                    SetCapture(hwnd);
                    RenderLayered(hwnd);
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            if (g_dragging) {
                POINT cur; GetCursorPos(&cur);
                int dx = cur.x - g_dragStartCursor.x;
                int dy = cur.y - g_dragStartCursor.y;
                SetWindowPos(hwnd, HWND_TOPMOST,
                             g_dragStartWindow.x + dx, g_dragStartWindow.y + dy,
                             0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
                return 0;
            }
            int wx = (int)(GET_X_LPARAM(lParam) / g_dpiScale) - L::SHADOW;
            int wy = (int)(GET_Y_LPARAM(lParam) / g_dpiScale) - L::SHADOW;
            if (g_activeSlider != WID_NONE) {
                UpdateSliderFromMouse(g_activeSlider, wx);
                return 0;
            }
            int hover = HitTestWidget(wx, wy);
            if (hover != g_hoveredWidget) {
                g_hoveredWidget = hover;
                RenderLayered(hwnd);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (g_hoveredWidget != WID_NONE) {
                g_hoveredWidget = WID_NONE;
                RenderLayered(hwnd);
            }
            return 0;

        case WM_LBUTTONUP: {
            int wx = (int)(GET_X_LPARAM(lParam) / g_dpiScale) - L::SHADOW;
            int wy = (int)(GET_Y_LPARAM(lParam) / g_dpiScale) - L::SHADOW;
            if (g_dragging) {
                g_dragging = false;
                ReleaseCapture();
                RECT wr; GetWindowRect(hwnd, &wr);
                g_lastWidgetPos = { wr.left, wr.top };
                return 0;
            }
            if (g_activeSlider != WID_NONE) {
                g_activeSlider = WID_NONE;
                ReleaseCapture();
                RenderLayered(hwnd);
                return 0;
            }
            if (g_pressedWidget != WID_NONE) {
                int widget = g_pressedWidget;
                g_pressedWidget = WID_NONE;
                ReleaseCapture();
                if (HitTestWidget(wx, wy) == widget) {
                    switch (widget) {
                        case WID_CLOSE:          HideWidget(hwnd); break;
                        case WID_BTN_LOAD_IMAGE: DoLoadImage(hwnd); break;
                        case WID_BTN_LOAD_WP:    DoLoadWallpaper(hwnd, false); break;
                        case WID_BTN_RESET_ALL:  DoResetAll(hwnd); break;
                        case WID_BTN_SAVE_WP:    DoSaveWallpaper(hwnd); break;
                        case WID_BTN_SAVE_FILE:  DoSaveFile(hwnd); break;
                        case WID_RESET_HUE:      g_hue = 0; UpdatePreview(); RenderLayered(hwnd); break;
                        case WID_RESET_SAT:      g_sat = 0; UpdatePreview(); RenderLayered(hwnd); break;
                        case WID_RESET_VAL:      g_val = 0; UpdatePreview(); RenderLayered(hwnd); break;
                        case WID_RESET_SCALE:    { Settings s = GetSettingsCopy();
                                                   g_scale = (int)(s.initialScale * 100);
                                                   UpdatePreview(); RenderLayered(hwnd); } break;
                    }
                } else {
                    RenderLayered(hwnd);
                }
            }
            return 0;
        }

        case WM_CAPTURECHANGED:
            if ((HWND)lParam != hwnd) {
                g_activeSlider  = WID_NONE;
                g_pressedWidget = WID_NONE;
                g_dragging      = false;
                RenderLayered(hwnd);
            }
            return 0;

        case WM_DESTROY:
            if (g_hotkeyRegistered) { UnregisterHotKey(hwnd, 1); g_hotkeyRegistered = false; }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================
//  UI thread
// ============================================================
static const wchar_t* kClass = L"Windhawk.HSVWallpaperAdjuster.Flyout";

static DWORD WINAPI UIThreadProc(void*) {
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    GdiplusStartupInput gdiIn;
    ULONG_PTR token = 0;
    GdiplusStartup(&token, &gdiIn, nullptr);

    { HDC dc = GetDC(nullptr); g_dpiScale = GetDeviceCaps(dc, LOGPIXELSX) / 96.0f; ReleaseDC(nullptr, dc); }

    ApplyTheme();   // needs GDI+ running (for IsFontInstalled)

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc)) {
        Wh_Log(L"RegisterClassExW failed: %lu", GetLastError());
        GdiplusShutdown(token);
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return 0;
    }

    int pxW = (int)(L::WIN_W * g_dpiScale);
    int pxH = (int)(L::WIN_H * g_dpiScale);

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kClass, L"HSV Wallpaper Adjuster", WS_POPUP,
        0, 0, pxW, pxH, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {
        Wh_Log(L"CreateWindowExW failed: %lu", GetLastError());
        GdiplusShutdown(token);
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return 0;
    }

    g_hwnd = hwnd;
    g_uiThreadId = GetCurrentThreadId();
    Wh_Log(L"Flyout hwnd=%p  dpi=%.2f  font='%s'  iconFont='%s'",
           hwnd, g_dpiScale, T.fontUi.c_str(), T.fontIcon.c_str());

    ApplyHotkey(hwnd);

    Settings s = GetSettingsCopy();
    g_scale = (int)(s.initialScale * 100);

    RenderLayered(hwnd);

    if (s.showOnStart) {
        ShowWidget(hwnd);
    } else {
        PositionOnActiveMonitor(hwnd);
        RECT wr; GetWindowRect(hwnd, &wr);
        g_lastWidgetPos = { wr.left, wr.top };
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_APP + 2 && msg.hwnd == nullptr) {
            if (g_hwnd) PostMessageW(g_hwnd, WM_APP + 2, 0, 0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_hwnd = nullptr;
    g_uiThreadId = 0;

    {
        lock_guard<mutex> lock(g_imageMutex);
        delete g_sourceBitmap;    g_sourceBitmap = nullptr;
        delete g_previewSource;   g_previewSource = nullptr;
        delete g_previewRendered; g_previewRendered = nullptr;
    }

    UnregisterClassW(kClass, wc.hInstance);
    GdiplusShutdown(token);
    if (SUCCEEDED(hrCo)) CoUninitialize();
    return 0;
}

// ============================================================
//  Mod lifecycle
// ============================================================
BOOL WhTool_ModInit() {
    LoadSettings();
    g_uiThread = CreateThread(nullptr, 0, UIThreadProc, nullptr, 0, &g_uiThreadId);
    if (!g_uiThread) { Wh_Log(L"CreateThread failed."); return FALSE; }
    Wh_Log(L"HSV Wallpaper Adjuster initialized.");
    return TRUE;
}

void WhTool_ModSettingsChanged() {
    LoadSettings();
    if (g_uiThreadId != 0) PostThreadMessageW(g_uiThreadId, WM_APP + 2, 0, 0);
}

void WhTool_ModUninit() {
    if (g_uiThreadId != 0) PostThreadMessageW(g_uiThreadId, WM_QUIT, 0, 0);
    if (g_uiThread) {
        WaitForSingleObject(g_uiThread, 3000);
        CloseHandle(g_uiThread);
        g_uiThread = nullptr;
    }
    Wh_Log(L"HSV Wallpaper Adjuster unloaded.");
}

//////////////////////////////////////////////////////////////////////////////////
// Windhawk tool-mod boilerplate
//////////////////////////////////////////////////////////////////////////////////
bool g_isToolModProcessLauncher;
HANDLE g_toolModProcessMutex;

void WINAPI EntryPoint_Hook() { Wh_Log(L">"); ExitThread(0); }

BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) && sessionId == 0) return FALSE;
    bool isExcluded = false, isToolModProcess = false, isCurrentToolModProcess = false;
    int argc; LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) { Wh_Log(L"CommandLineToArgvW failed"); return FALSE; }
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-service") == 0 || wcscmp(argv[i], L"-service-start") == 0 ||
            wcscmp(argv[i], L"-service-stop") == 0) { isExcluded = true; break; }
    }
    for (int i = 1; i < argc - 1; i++) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            isToolModProcess = true;
            if (wcscmp(argv[i+1], WH_MOD_ID) == 0) isCurrentToolModProcess = true;
            break;
        }
    }
    LocalFree(argv);
    if (isExcluded) return FALSE;

    if (isCurrentToolModProcess) {
        g_toolModProcessMutex = CreateMutex(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (!g_toolModProcessMutex) { Wh_Log(L"CreateMutex failed"); ExitProcess(1); }
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Wh_Log(L"Tool mod already running (%s)", WH_MOD_ID); ExitProcess(1);
        }
        if (!WhTool_ModInit()) ExitProcess(1);
        IMAGE_DOS_HEADER* dh = (IMAGE_DOS_HEADER*)GetModuleHandle(nullptr);
        IMAGE_NT_HEADERS* nh = (IMAGE_NT_HEADERS*)((BYTE*)dh + dh->e_lfanew);
        void* entry = (BYTE*)dh + nh->OptionalHeader.AddressOfEntryPoint;
        Wh_SetFunctionHook(entry, (void*)EntryPoint_Hook, nullptr);
        return TRUE;
    }
    if (isToolModProcess) return FALSE;
    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) return;
    WCHAR path[MAX_PATH];
    switch (GetModuleFileName(nullptr, path, ARRAYSIZE(path))) {
        case 0: case ARRAYSIZE(path): Wh_Log(L"GetModuleFileName failed"); return;
    }
    WCHAR cmd[MAX_PATH + 2 + (sizeof(L" -tool-mod \"" WH_MOD_ID "\"") / sizeof(WCHAR)) - 1];
    swprintf_s(cmd, L"\"%s\" -tool-mod \"%s\"", path, WH_MOD_ID);
    HMODULE km = GetModuleHandle(L"kernelbase.dll");
    if (!km) { km = GetModuleHandle(L"kernel32.dll"); if (!km) { Wh_Log(L"No kernel"); return; } }
    using CP_t = BOOL(WINAPI*)(HANDLE, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES,
        LPSECURITY_ATTRIBUTES, WINBOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW,
        LPPROCESS_INFORMATION, PHANDLE);
    CP_t fn = (CP_t)GetProcAddress(km, "CreateProcessInternalW");
    if (!fn) { Wh_Log(L"No CreateProcessInternalW"); return; }
    STARTUPINFO si{ .cb = sizeof(STARTUPINFO), .dwFlags = STARTF_FORCEOFFFEEDBACK };
    PROCESS_INFORMATION pi;
    if (!fn(nullptr, path, cmd, nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS,
            nullptr, nullptr, &si, &pi, nullptr)) { Wh_Log(L"CreateProcess failed"); return; }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

void Wh_ModSettingsChanged() {
    if (g_isToolModProcessLauncher) return;
    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) return;
    WhTool_ModUninit();
    ExitProcess(0);
}