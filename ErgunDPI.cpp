#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "dpi_engine.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#define IDI_APPICON 101

struct UiBrushes {
    ID2D1SolidColorBrush* bg = nullptr;
    ID2D1SolidColorBrush* card = nullptr;
    ID2D1SolidColorBrush* row_hover = nullptr;
    ID2D1SolidColorBrush* border = nullptr;
    ID2D1SolidColorBrush* sep = nullptr;
    ID2D1SolidColorBrush* text_main = nullptr;
    ID2D1SolidColorBrush* text_sub = nullptr;
    ID2D1SolidColorBrush* text_muted = nullptr;
    ID2D1SolidColorBrush* cyan = nullptr;
    ID2D1SolidColorBrush* badge_bg = nullptr;
    ID2D1SolidColorBrush* emerald = nullptr;
    ID2D1SolidColorBrush* emerald_pill = nullptr;
    ID2D1SolidColorBrush* crimson = nullptr;
    ID2D1SolidColorBrush* idle_pill = nullptr;
    ID2D1SolidColorBrush* orb_idle = nullptr;
    ID2D1SolidColorBrush* orb_active = nullptr;
    ID2D1SolidColorBrush* discord = nullptr;
    ID2D1SolidColorBrush* roblox = nullptr;
    ID2D1SolidColorBrush* toggle_off = nullptr;
    ID2D1SolidColorBrush* toggle_locked = nullptr;
    ID2D1SolidColorBrush* knob = nullptr;
    ID2D1SolidColorBrush* knob_locked = nullptr;
    ID2D1SolidColorBrush* white = nullptr;
};

static HWND g_wnd = nullptr;
static HICON g_icon_big = nullptr;
static HICON g_icon_small = nullptr;
static float g_scale = 1.0f;
static uint64_t g_start_tick = 0;
static uint64_t g_last_frame = 0;
static int g_hover_zone = 0;
static bool g_mouse_tracked = false;

static float g_active_t = 0.0f;
static float g_hover_t = 0.0f;
static float g_dc_t = 1.0f;
static float g_rbx_t = 1.0f;
static float g_wave_phase = 0.0f;
static float g_orbit_deg = 0.0f;

static ID2D1Factory* g_d2d = nullptr;
static IDWriteFactory* g_dw = nullptr;
static ID2D1HwndRenderTarget* g_rt = nullptr;
static ID2D1StrokeStyle* g_round_stroke = nullptr;
static UiBrushes g_br;

static IDWriteTextFormat* g_fmt_title = nullptr;
static IDWriteTextFormat* g_fmt_bold = nullptr;
static IDWriteTextFormat* g_fmt_item_sub = nullptr;
static IDWriteTextFormat* g_fmt_val = nullptr;
static IDWriteTextFormat* g_fmt_micro = nullptr;
static IDWriteTextFormat* g_fmt_orb = nullptr;
static IDWriteTextFormat* g_fmt_tag = nullptr;
static IDWriteTextFormat* g_fmt_foot = nullptr;

static constexpr float kWinW = 360.0f;
static constexpr float kWinH = 414.0f;
static constexpr float kOrbX = 180.0f;
static constexpr float kOrbY = 128.0f;
static constexpr float kOrbR = 52.0f;
static const RECT kDcRect  = { 24, 276, 336, 328 };
static const RECT kRbxRect = { 24, 328, 336, 380 };

static inline D2D1_COLOR_F rgb_col(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f) {
    return { r / 255.0f, g / 255.0f, b / 255.0f, a };
}

static inline D2D1_RECT_F box_f(float x, float y, float w, float h) {
    return { x, y, x + w, y + h };
}

static inline float step_toward(float cur, float target, float speed) {
    if (cur < target) {
        cur += speed;
        return (cur > target) ? target : cur;
    }
    if (cur > target) {
        cur -= speed;
        return (cur < target) ? target : cur;
    }
    return cur;
}

static void release_brushes() {
    ID2D1SolidColorBrush** list[] = {
        &g_br.bg, &g_br.card, &g_br.row_hover, &g_br.border, &g_br.sep,
        &g_br.text_main, &g_br.text_sub, &g_br.text_muted, &g_br.cyan, &g_br.badge_bg,
        &g_br.emerald, &g_br.emerald_pill, &g_br.crimson, &g_br.idle_pill,
        &g_br.orb_idle, &g_br.orb_active, &g_br.discord, &g_br.roblox,
        &g_br.toggle_off, &g_br.toggle_locked, &g_br.knob, &g_br.knob_locked, &g_br.white
    };
    for (auto** b : list) {
        if (*b) {
            (*b)->Release();
            *b = nullptr;
        }
    }
}

static void discard_rt() {
    release_brushes();
    if (g_rt) {
        g_rt->Release();
        g_rt = nullptr;
    }
}

static bool ensure_rt(HWND hwnd) {
    if (g_rt) return true;

    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = { static_cast<UINT32>(rc.right), static_cast<UINT32>(rc.bottom) };
    float dpi = 96.0f * g_scale;

    D2D1_RENDER_TARGET_PROPERTIES props = {
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        { DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN },
        dpi, dpi,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT
    };
    D2D1_HWND_RENDER_TARGET_PROPERTIES hprops = { hwnd, size, D2D1_PRESENT_OPTIONS_NONE };

    if (FAILED(g_d2d->CreateHwndRenderTarget(&props, &hprops, &g_rt))) {
        return false;
    }

    g_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    g_rt->CreateSolidColorBrush(rgb_col(15, 17, 21), &g_br.bg);
    g_rt->CreateSolidColorBrush(rgb_col(22, 25, 31), &g_br.card);
    g_rt->CreateSolidColorBrush(rgb_col(28, 32, 40), &g_br.row_hover);
    g_rt->CreateSolidColorBrush(rgb_col(37, 42, 52), &g_br.border);
    g_rt->CreateSolidColorBrush(rgb_col(32, 36, 45), &g_br.sep);
    g_rt->CreateSolidColorBrush(rgb_col(242, 245, 250), &g_br.text_main);
    g_rt->CreateSolidColorBrush(rgb_col(136, 144, 160), &g_br.text_sub);
    g_rt->CreateSolidColorBrush(rgb_col(90, 98, 114), &g_br.text_muted);
    g_rt->CreateSolidColorBrush(rgb_col(68, 168, 255), &g_br.cyan);
    g_rt->CreateSolidColorBrush(rgb_col(24, 34, 48), &g_br.badge_bg);
    g_rt->CreateSolidColorBrush(rgb_col(30, 206, 148), &g_br.emerald);
    g_rt->CreateSolidColorBrush(rgb_col(20, 52, 43), &g_br.emerald_pill);
    g_rt->CreateSolidColorBrush(rgb_col(226, 76, 92), &g_br.crimson);
    g_rt->CreateSolidColorBrush(rgb_col(32, 36, 44), &g_br.idle_pill);
    g_rt->CreateSolidColorBrush(rgb_col(24, 29, 38), &g_br.orb_idle);
    g_rt->CreateSolidColorBrush(rgb_col(18, 46, 39), &g_br.orb_active);
    g_rt->CreateSolidColorBrush(rgb_col(114, 137, 218), &g_br.discord);
    g_rt->CreateSolidColorBrush(rgb_col(58, 166, 248), &g_br.roblox);
    g_rt->CreateSolidColorBrush(rgb_col(46, 51, 63), &g_br.toggle_off);
    g_rt->CreateSolidColorBrush(rgb_col(34, 38, 47), &g_br.toggle_locked);
    g_rt->CreateSolidColorBrush(rgb_col(248, 250, 252), &g_br.knob);
    g_rt->CreateSolidColorBrush(rgb_col(145, 152, 166), &g_br.knob_locked);
    g_rt->CreateSolidColorBrush(rgb_col(255, 255, 255), &g_br.white);

    return true;
}

static void fill_card(float x, float y, float w, float h, float r,
                      ID2D1SolidColorBrush* fill, ID2D1SolidColorBrush* stroke = nullptr) {
    D2D1_ROUNDED_RECT rr = { box_f(x, y, w, h), r, r };
    g_rt->FillRoundedRectangle(&rr, fill);
    if (stroke) {
        g_rt->DrawRoundedRectangle(&rr, stroke, 1.0f);
    }
}

static void draw_str(const wchar_t* text, IDWriteTextFormat* fmt,
                     float x, float y, float w, float h, ID2D1SolidColorBrush* br) {
    D2D1_RECT_F r = box_f(x, y, w, h);
    g_rt->DrawText(text, static_cast<UINT32>(wcslen(text)), fmt, &r, br);
}

static void draw_arc(float cx, float cy, float radius, float start_deg, float sweep_deg,
                     ID2D1SolidColorBrush* br, float stroke_w) {
    ID2D1PathGeometry* geo = nullptr;
    if (FAILED(g_d2d->CreatePathGeometry(&geo))) return;

    ID2D1GeometrySink* sink = nullptr;
    if (SUCCEEDED(geo->Open(&sink))) {
        constexpr float kDeg2Rad = 3.14159265f / 180.0f;
        float a1 = start_deg * kDeg2Rad;
        float a2 = (start_deg + sweep_deg) * kDeg2Rad;

        D2D1_POINT_2F p1 = { cx + radius * cosf(a1), cy + radius * sinf(a1) };
        D2D1_POINT_2F p2 = { cx + radius * cosf(a2), cy + radius * sinf(a2) };

        sink->BeginFigure(p1, D2D1_FIGURE_BEGIN_HOLLOW);
        D2D1_ARC_SEGMENT arc = {
            p2,
            { radius, radius },
            0.0f,
            (sweep_deg >= 0.0f) ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
            (fabsf(sweep_deg) > 180.0f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
        };
        sink->AddArc(&arc);
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        sink->Release();
        g_rt->DrawGeometry(geo, br, stroke_w, g_round_stroke);
    }
    geo->Release();
}

static void draw_switch(float x, float y, float anim_t, ID2D1SolidColorBrush* active_br, bool locked) {
    ID2D1SolidColorBrush* track = locked ? g_br.toggle_locked : ((anim_t > 0.5f) ? active_br : g_br.toggle_off);
    fill_card(x, y, 42.0f, 22.0f, 11.0f, track);

    float kx = x + 11.0f + (20.0f * anim_t);
    D2D1_ELLIPSE knob = { { kx, y + 11.0f }, 8.5f, 8.5f };
    g_rt->FillEllipse(&knob, locked ? g_br.knob_locked : g_br.knob);
}

static void render_ui(HWND hwnd) {
    if (!ensure_rt(hwnd)) return;

    g_rt->BeginDraw();
    D2D1_MATRIX_3X2_F id = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    g_rt->SetTransform(&id);
    g_rt->Clear(rgb_col(15, 17, 21));

    bool on = dpi_is_running();

    draw_str(L"ErgunDPI", g_fmt_title, 24.0f, 20.0f, 102.0f, 28.0f, g_br.text_main);
    fill_card(126.0f, 25.0f, 38.0f, 18.0f, 5.0f, g_br.badge_bg, g_br.border);
    draw_str(L"v2.2", g_fmt_tag, 126.0f, 25.0f, 38.0f, 18.0f, g_br.cyan);

    fill_card(248.0f, 21.0f, 88.0f, 26.0f, 13.0f, on ? g_br.emerald_pill : g_br.idle_pill);
    D2D1_ELLIPSE dot = { { 265.0f, 34.0f }, 3.5f, 3.5f };
    g_rt->FillEllipse(&dot, on ? g_br.emerald : g_br.text_muted);
    draw_str(on ? L"AKTİF" : L"HAZIR", g_fmt_tag, 271.0f, 21.0f, 56.0f, 26.0f,
             on ? g_br.emerald : g_br.text_sub);

    if (g_active_t > 0.01f) {
        for (int i = 0; i < 2; ++i) {
            float p = g_wave_phase + (i * 0.5f);
            if (p >= 1.0f) p -= 1.0f;
            float r = kOrbR + 4.0f + (p * 30.0f);
            float alpha = (1.0f - p) * 0.34f * g_active_t;
            g_br.emerald->SetOpacity(alpha);
            D2D1_ELLIPSE wave = { { kOrbX, kOrbY }, r, r };
            g_rt->DrawEllipse(&wave, g_br.emerald, 1.6f);
        }
        g_br.emerald->SetOpacity(1.0f);
    }

    float breath = sinf(g_wave_phase * 6.2831853f) * 1.5f;
    float orb_r = kOrbR + (g_hover_t * 2.5f) + (on ? breath : 0.0f);

    g_br.orb_idle->SetOpacity(1.0f - g_active_t);
    D2D1_ELLIPSE base_orb = { { kOrbX, kOrbY }, orb_r, orb_r };
    g_rt->FillEllipse(&base_orb, g_br.orb_idle);
    g_br.orb_idle->SetOpacity(1.0f);

    if (g_active_t > 0.01f) {
        g_br.orb_active->SetOpacity(g_active_t);
        g_rt->FillEllipse(&base_orb, g_br.orb_active);
        g_br.orb_active->SetOpacity(1.0f);
    }

    ID2D1SolidColorBrush* ring_br = on ? g_br.emerald : g_br.border;
    g_rt->DrawEllipse(&base_orb, ring_br, 1.5f + (g_hover_t * 0.6f));

    if (g_active_t > 0.01f) {
        g_br.emerald->SetOpacity(0.85f * g_active_t);
        draw_arc(kOrbX, kOrbY, orb_r + 5.0f, g_orbit_deg, 110.0f, g_br.emerald, 2.2f);
        draw_arc(kOrbX, kOrbY, orb_r + 5.0f, g_orbit_deg + 180.0f, 110.0f, g_br.emerald, 2.2f);
        g_br.emerald->SetOpacity(1.0f);
    } else if (g_hover_t > 0.01f) {
        g_br.cyan->SetOpacity(0.55f * g_hover_t);
        draw_arc(kOrbX, kOrbY, orb_r + 5.0f, -90.0f, 360.0f * g_hover_t, g_br.cyan, 1.8f);
        g_br.cyan->SetOpacity(1.0f);
    }

    ID2D1SolidColorBrush* icon_br = on ? g_br.emerald : ((g_hover_zone == 1) ? g_br.cyan : g_br.text_main);
    draw_arc(kOrbX, kOrbY - 10.0f, 10.5f, -52.0f, 284.0f, icon_br, 2.4f);
    g_rt->DrawLine({ kOrbX, kOrbY - 22.5f }, { kOrbX, kOrbY - 11.5f }, icon_br, 2.4f, g_round_stroke);
    draw_str(on ? L"DURDUR" : L"BAŞLAT", g_fmt_orb, kOrbX - 44.0f, kOrbY + 10.0f, 88.0f, 20.0f, icon_br);

    fill_card(24.0f, 198.0f, 150.0f, 58.0f, 12.0f, g_br.card, g_br.border);
    draw_str(L"AŞILAN ENGEL", g_fmt_micro, 38.0f, 206.0f, 122.0f, 16.0f, g_br.text_muted);

    wchar_t cnt_buf[32];
    swprintf_s(cnt_buf, L"%llu", dpi_packet_count());
    draw_str(cnt_buf, g_fmt_val, 38.0f, 224.0f, 122.0f, 24.0f, g_br.cyan);

    fill_card(186.0f, 198.0f, 150.0f, 58.0f, 12.0f, g_br.card, g_br.border);
    draw_str(L"AKTİF SÜRE", g_fmt_micro, 200.0f, 206.0f, 122.0f, 16.0f, g_br.text_muted);

    wchar_t time_buf[32];
    if (on && g_start_tick > 0) {
        uint64_t sec = (GetTickCount64() - g_start_tick) / 1000;
        uint64_t m = (sec / 60) % 60;
        uint64_t s = sec % 60;
        uint64_t hr = sec / 3600;
        if (hr > 0) swprintf_s(time_buf, L"%llu:%02llu:%02llu", hr, m, s);
        else swprintf_s(time_buf, L"%02llu:%02llu", m, s);
    } else {
        wcscpy_s(time_buf, L"Beklemede");
    }
    draw_str(time_buf, g_fmt_val, 200.0f, 224.0f, 122.0f, 24.0f, on ? g_br.emerald : g_br.text_sub);

    fill_card(24.0f, 276.0f, 312.0f, 104.0f, 12.0f, g_br.card, g_br.border);

    if (!on && g_hover_zone == 2) fill_card(26.0f, 278.0f, 308.0f, 49.0f, 10.0f, g_br.row_hover);
    if (!on && g_hover_zone == 3) fill_card(26.0f, 329.0f, 308.0f, 49.0f, 10.0f, g_br.row_hover);

    g_rt->DrawLine({ 38.0f, 328.0f }, { 322.0f, 328.0f }, g_br.sep, 1.0f);

    fill_card(38.0f, 289.0f, 4.0f, 26.0f, 2.0f, g_br.discord);
    draw_str(L"Discord", g_fmt_bold, 52.0f, 283.0f, 180.0f, 20.0f, g_br.text_main);
    draw_str(L"Ses, yayın ve mesajlaşma", g_fmt_item_sub, 52.0f, 302.0f, 190.0f, 16.0f, g_br.text_sub);
    draw_switch(278.0f, 291.0f, g_dc_t, g_br.discord, on);

    fill_card(38.0f, 341.0f, 4.0f, 26.0f, 2.0f, g_br.roblox);
    draw_str(L"Roblox", g_fmt_bold, 52.0f, 335.0f, 180.0f, 20.0f, g_br.text_main);
    draw_str(L"Oyun istemcisi ve sunucular", g_fmt_item_sub, 52.0f, 354.0f, 190.0f, 16.0f, g_br.text_sub);
    draw_switch(278.0f, 343.0f, g_rbx_t, g_br.roblox, on);

    draw_str(L"Simge durumuna küçültüldüğünde sistem tepsisinde çalışır",
             g_fmt_foot, 24.0f, 388.0f, 312.0f, 20.0f, g_br.text_muted);

    if (g_rt->EndDraw() == D2DERR_RECREATE_TARGET) {
        discard_rt();
    }
}

static void update_tray(HWND hwnd) {
    if (!hwnd) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP + 3;
    nid.hIcon = g_icon_small ? g_icon_small : g_icon_big;
    swprintf_s(nid.szTip, L"ErgunDPI - %s (Asilan Engel: %llu)",
               dpi_is_running() ? L"Aktif" : L"Hazir", dpi_packet_count());
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void handle_start(HWND hwnd) {
    if (dpi_start(hwnd)) {
        g_start_tick = GetTickCount64();
        InvalidateRect(hwnd, nullptr, FALSE);
        update_tray(hwnd);
    }
}

static void handle_stop(HWND hwnd) {
    dpi_stop();
    g_start_tick = 0;
    InvalidateRect(hwnd, nullptr, FALSE);
    update_tray(hwnd);
}

static inline POINT to_logical(int x, int y) {
    return { static_cast<LONG>(x / g_scale), static_cast<LONG>(y / g_scale) };
}

static int hit_zone(POINT pt) {
    float dx = pt.x - kOrbX;
    float dy = pt.y - kOrbY;
    if ((dx * dx + dy * dy) <= (kOrbR + 6.0f) * (kOrbR + 6.0f)) return 1;

    if (!dpi_is_running()) {
        if (PtInRect(&kDcRect, pt)) return 2;
        if (PtInRect(&kRbxRect, pt)) return 3;
    }
    return 0;
}

static bool init_graphics() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr, reinterpret_cast<void**>(&g_d2d)))) {
        return false;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_dw)))) {
        return false;
    }

    D2D1_STROKE_STYLE_PROPERTIES sp = {};
    sp.startCap = D2D1_CAP_STYLE_ROUND;
    sp.endCap = D2D1_CAP_STYLE_ROUND;
    sp.lineJoin = D2D1_LINE_JOIN_ROUND;
    g_d2d->CreateStrokeStyle(&sp, nullptr, 0, &g_round_stroke);

    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"", &g_fmt_title);
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, 13.5f, L"", &g_fmt_bold);
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"", &g_fmt_item_sub);
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, 15.5f, L"", &g_fmt_val);
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"", &g_fmt_micro);
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"", &g_fmt_orb);
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"", &g_fmt_tag);
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"", &g_fmt_foot);

    IDWriteTextFormat* all[] = {
        g_fmt_title, g_fmt_bold, g_fmt_item_sub,
        g_fmt_val, g_fmt_micro, g_fmt_orb, g_fmt_tag, g_fmt_foot
    };
    for (auto* f : all) {
        if (f) f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (g_fmt_orb) g_fmt_orb->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (g_fmt_tag) g_fmt_tag->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (g_fmt_foot) g_fmt_foot->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    return true;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_last_frame = GetTickCount64();
        SetTimer(hwnd, 1, 16, nullptr);
        SetTimer(hwnd, 2, 1000, nullptr);
        PostMessageW(hwnd, WM_APP + 2, 0, 0);

        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_APP + 3;
        nid.hIcon = g_icon_small ? g_icon_small : g_icon_big;
        wcscpy_s(nid.szTip, L"ErgunDPI");
        Shell_NotifyIconW(NIM_ADD, &nid);
        return 0;
    }

    case WM_DPICHANGED: {
        UINT new_dpi = HIWORD(wp);
        g_scale = new_dpi / 96.0f;
        if (g_rt) {
            g_rt->SetDpi(static_cast<float>(new_dpi), static_cast<float>(new_dpi));
        }
        const RECT* rc = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd, nullptr, rc->left, rc->top,
                     rc->right - rc->left, rc->bottom - rc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) {
            if (g_hover_zone != 0 || g_hover_t > 0.0f) {
                g_hover_zone = 0;
                g_hover_t = 0.0f;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        } else {
            g_last_frame = GetTickCount64();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
        } else if (g_rt) {
            D2D1_SIZE_U sz = { LOWORD(lp), HIWORD(lp) };
            g_rt->Resize(&sz);
        }
        return 0;

    case WM_APP + 4:
        // Motor beklenmedik sekilde durdugunda arayuzu ve tepsiyi senkronize et
        if (!dpi_is_running()) {
            g_start_tick = 0;
            g_active_t = 0.0f;
            InvalidateRect(hwnd, nullptr, FALSE);
            update_tray(hwnd);
        }
        return 0;

    case WM_APP + 3: {
        if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_SHOW);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            bool on = dpi_is_running();
            AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, on ? L"ErgunDPI (Aktif)" : L"ErgunDPI (Hazır)");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, 101, L"Pencereyi Aç");
            AppendMenuW(menu, MF_STRING, 102, on ? L"Durdur" : L"Başlat");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, 103, L"Çıkış");

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);

            if (cmd == 101) {
                ShowWindow(hwnd, SW_SHOW);
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            } else if (cmd == 102) {
                if (on) handle_stop(hwnd);
                else handle_start(hwnd);
            } else if (cmd == 103) {
                DestroyWindow(hwnd);
            }
        }
        return 0;
    }

    case WM_APP + 2:
        handle_start(hwnd);
        return 0;

    case WM_APP + 1: {
        g_start_tick = 0;
        g_active_t = 0.0f;
        wchar_t err[128];
        swprintf_s(err, L"WinDivert 2.2 surucusu acilamadi (Kod: %lu).", (DWORD)wp);
        MessageBoxW(hwnd, err, L"ErgunDPI", MB_ICONERROR);
        InvalidateRect(hwnd, nullptr, FALSE);
        update_tray(hwnd);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER: {
        if (wp == 2) {
            if (dpi_is_running()) {
                update_tray(hwnd);
                // Pencere arka plandayken GPU'yu yormadan yalnizca saniyede bir sayaci tazele
                if (IsWindowVisible(hwnd) && !IsIconic(hwnd) && GetForegroundWindow() != hwnd) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        if (wp == 1 && IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
            if (GetForegroundWindow() != hwnd) return 0;

            uint64_t now = GetTickCount64();
            float dt = static_cast<float>(now - g_last_frame) / 1000.0f;
            if (dt > 0.05f) dt = 0.05f;
            g_last_frame = now;

            bool on = dpi_is_running();
            float prev_act = g_active_t;
            float prev_hov = g_hover_t;
            float prev_dc  = g_dc_t;
            float prev_rbx = g_rbx_t;

            g_active_t = step_toward(g_active_t, on ? 1.0f : 0.0f, dt * 4.5f);
            g_hover_t  = step_toward(g_hover_t, (g_hover_zone == 1) ? 1.0f : 0.0f, dt * 6.0f);
            g_dc_t     = step_toward(g_dc_t, dpi_get_discord() ? 1.0f : 0.0f, dt * 7.5f);
            g_rbx_t    = step_toward(g_rbx_t, dpi_get_roblox() ? 1.0f : 0.0f, dt * 7.5f);

            bool animating = (g_active_t > 0.001f) || (prev_act != g_active_t) ||
                             (prev_hov != g_hover_t) || (prev_dc != g_dc_t) || (prev_rbx != g_rbx_t);

            if (animating) {
                g_wave_phase += dt * 0.42f;
                if (g_wave_phase >= 1.0f) g_wave_phase -= 1.0f;
                g_orbit_deg += dt * 115.0f;
                if (g_orbit_deg >= 360.0f) g_orbit_deg -= 360.0f;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!g_mouse_tracked) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            g_mouse_tracked = true;
        }
        POINT pt = to_logical((int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        int zone = hit_zone(pt);
        if (zone != g_hover_zone) {
            g_hover_zone = zone;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        g_mouse_tracked = false;
        if (g_hover_zone != 0) {
            g_hover_zone = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        if (hit_zone(to_logical(pt.x, pt.y)) != 0) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = to_logical((int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        int zone = hit_zone(pt);
        if (zone == 1) {
            if (dpi_is_running()) handle_stop(hwnd);
            else handle_start(hwnd);
        } else if (zone == 2) {
            dpi_set_discord(!dpi_get_discord());
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (zone == 3) {
            dpi_set_roblox(!dpi_get_roblox());
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        render_ui(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY: {
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        dpi_stop();
        discard_rt();
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    if (HMODULE u32 = GetModuleHandleW(L"user32.dll")) {
        typedef BOOL (WINAPI *FnDpiCtx)(void*);
        typedef BOOL (WINAPI *FnDpiAware)(void);
        if (auto f = (FnDpiCtx)GetProcAddress(u32, "SetProcessDpiAwarenessContext")) {
            f(reinterpret_cast<void*>(-4));
        } else if (auto f2 = (FnDpiAware)GetProcAddress(u32, "SetProcessDPIAware")) {
            f2();
        }
    }

    HDC sdc = GetDC(nullptr);
    g_scale = GetDeviceCaps(sdc, LOGPIXELSX) / 96.0f;
    ReleaseDC(nullptr, sdc);

    g_icon_big = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    g_icon_small = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                     GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (!g_icon_big) g_icon_big = LoadIconW(nullptr, IDI_APPLICATION);
    if (!g_icon_small) g_icon_small = g_icon_big;

    if (!init_graphics()) return 1;

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hIcon = g_icon_big;
    wc.hIconSm = g_icon_small;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ErgunDPIWnd";
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, static_cast<LONG>(kWinW * g_scale), static_cast<LONG>(kWinH * g_scale) };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rc, style, FALSE);

    int ww = rc.right - rc.left;
    int wh = rc.bottom - rc.top;
    int px = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int py = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;

    g_wnd = CreateWindowExW(0, wc.lpszClassName, L"ErgunDPI", style,
                            px, py, ww, wh, nullptr, nullptr, inst, nullptr);

    SendMessageW(g_wnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_icon_big));
    SendMessageW(g_wnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_icon_small));

    BOOL dark = TRUE;
    DWORD corner = DWMWCP_ROUND;
    COLORREF border = RGB(37, 42, 52);
    DwmSetWindowAttribute(g_wnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(g_wnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    DwmSetWindowAttribute(g_wnd, DWMWA_BORDER_COLOR, &border, sizeof(border));

    ShowWindow(g_wnd, show);
    UpdateWindow(g_wnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_round_stroke) g_round_stroke->Release();
    if (g_fmt_title) g_fmt_title->Release();
    if (g_fmt_bold) g_fmt_bold->Release();
    if (g_fmt_item_sub) g_fmt_item_sub->Release();
    if (g_fmt_val) g_fmt_val->Release();
    if (g_fmt_micro) g_fmt_micro->Release();
    if (g_fmt_orb) g_fmt_orb->Release();
    if (g_fmt_tag) g_fmt_tag->Release();
    if (g_fmt_foot) g_fmt_foot->Release();
    if (g_dw) g_dw->Release();
    if (g_d2d) g_d2d->Release();
    return 0;
}