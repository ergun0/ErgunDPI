#include <winsock2.h>
#include <windows.h>
#include "dpi_engine.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string_view>

typedef HANDLE (WINAPI *FnOpen)(const char*, int, INT16, UINT64);
typedef BOOL   (WINAPI *FnRecv)(HANDLE, void*, UINT, UINT*, void*);
typedef BOOL   (WINAPI *FnSend)(HANDLE, const void*, UINT, UINT*, const void*);
typedef BOOL   (WINAPI *FnCalc)(void*, UINT, void*, UINT64);
typedef BOOL   (WINAPI *FnSetParam)(HANDLE, int, UINT64);
typedef BOOL   (WINAPI *FnShutdown)(HANDLE, int);
typedef BOOL   (WINAPI *FnClose)(HANDLE);

static HMODULE g_dll = nullptr;
static FnOpen wd_open = nullptr;
static FnRecv wd_recv = nullptr;
static FnSend wd_send = nullptr;
static FnCalc wd_calc = nullptr;
static FnSetParam wd_set_param = nullptr;
static FnShutdown wd_shutdown = nullptr;
static FnClose wd_close = nullptr;

// TLS ClientHello kaydini 2. bayttan bolerek DPI cihazinin SNI okumasini engeller
static constexpr uint32_t kSplitOffset = 2;

// Sadece giden TLS ClientHello el sikismalarini cekirdek seviyesinde yakalar
static constexpr const char* kKernelFilter =
    "outbound and tcp.DstPort == 443 and tcp.PayloadLength >= 48 "
    "and tcp.Payload[0] == 0x16 and tcp.Payload[1] == 0x03 and tcp.Payload[5] == 0x01";

static constexpr std::string_view kDiscordHosts[] = {
    "discord.com", "discord.gg", "discordapp.com", "discordapp.net",
    "discord.media", "discord.co", "discordcdn.com", "dis.gd",
    "discord.new", "discord.gift", "discordstatus.com", "discord.app",
    "discordsays.com", "discord-activities.com", "discordmerch.com"
};

static constexpr std::string_view kRobloxHosts[] = {
    "roblox.com", "rbxcdn.com", "rbx.com", "robloxlabs.com", "rbxinfra.net"
};

static std::atomic<bool> g_running{false};
static std::atomic<bool> g_discord{true};
static std::atomic<bool> g_roblox{true};
static std::atomic<uint64_t> g_packets{0};

static CRITICAL_SECTION g_cs;
static bool g_cs_ready = false;
static HANDLE g_divert = INVALID_HANDLE_VALUE;
static HANDLE g_worker = nullptr;
static HWND g_notify_wnd = nullptr;

static void ensure_cs() {
    if (!g_cs_ready) {
        InitializeCriticalSection(&g_cs);
        g_cs_ready = true;
    }
}

static bool load_windivert() {
    if (g_dll) return true;

    wchar_t dir[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, dir, MAX_PATH)) {
        if (wchar_t* slash = wcsrchr(dir, L'\\')) {
            *(slash + 1) = L'\0';
            wchar_t full_path[MAX_PATH];
            swprintf_s(full_path, L"%sWinDivert.dll", dir);
            g_dll = LoadLibraryW(full_path);
            if (!g_dll) {
                swprintf_s(full_path, L"%sWinDivert64.dll", dir);
                g_dll = LoadLibraryW(full_path);
            }
        }
    }

    if (!g_dll) g_dll = LoadLibraryA("WinDivert.dll");
    if (!g_dll) g_dll = LoadLibraryA("WinDivert64.dll");
    if (!g_dll) return false;

    wd_open      = (FnOpen)GetProcAddress(g_dll, "WinDivertOpen");
    wd_recv      = (FnRecv)GetProcAddress(g_dll, "WinDivertRecv");
    wd_send      = (FnSend)GetProcAddress(g_dll, "WinDivertSend");
    wd_calc      = (FnCalc)GetProcAddress(g_dll, "WinDivertHelperCalcChecksums");
    wd_set_param = (FnSetParam)GetProcAddress(g_dll, "WinDivertSetParam");
    wd_shutdown  = (FnShutdown)GetProcAddress(g_dll, "WinDivertShutdown");
    wd_close     = (FnClose)GetProcAddress(g_dll, "WinDivertClose");

    if (!wd_open || !wd_recv || !wd_send || !wd_calc || !wd_shutdown || !wd_close) {
        FreeLibrary(g_dll);
        g_dll = nullptr;
        return false;
    }
    return true;
}

static bool has_admin_rights() {
    BOOL admin = FALSE;
    PSID sid = nullptr;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&auth, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid)) {
        CheckTokenMembership(nullptr, sid, &admin);
        FreeSid(sid);
    }
    return admin == TRUE;
}

// TLS ClientHello uzantilarindan SNI alan adini ayiklar
static std::string_view extract_sni(const uint8_t* data, uint32_t len) {
    if (len < 44 || data[0] != 0x16 || data[5] != 0x01) return {};

    uint32_t pos = 43;
    if (pos + 1 > len) return {};
    pos += 1 + data[pos];

    if (pos + 2 > len) return {};
    uint16_t cipher_len = (uint16_t(data[pos]) << 8) | data[pos + 1];
    pos += 2 + cipher_len;

    if (pos + 1 > len) return {};
    pos += 1 + data[pos];

    if (pos + 2 > len) return {};
    uint16_t ext_len = (uint16_t(data[pos]) << 8) | data[pos + 1];
    pos += 2;

    uint32_t end = pos + ext_len;
    if (end > len) end = len;

    while (pos + 4 <= end) {
        uint16_t type = (uint16_t(data[pos]) << 8) | data[pos + 1];
        uint16_t size = (uint16_t(data[pos + 2]) << 8) | data[pos + 3];
        pos += 4;
        if (pos + size > end) break;

        if (type == 0x0000 && size >= 5) {
            uint16_t name_len = (uint16_t(data[pos + 3]) << 8) | data[pos + 4];
            if (5 + name_len <= size && name_len >= 4) {
                return std::string_view(reinterpret_cast<const char*>(data + pos + 5), name_len);
            }
            return {};
        }
        pos += size;
    }
    return {};
}

static inline char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

static bool host_matches(std::string_view host, std::string_view domain) {
    if (host.size() < domain.size()) return false;
    if (host.size() > domain.size() && host[host.size() - domain.size() - 1] != '.') return false;

    size_t offset = host.size() - domain.size();
    for (size_t i = 0; i < domain.size(); ++i) {
        if (ascii_lower(host[offset + i]) != domain[i]) return false;
    }
    return true;
}

static bool payload_contains(const uint8_t* data, uint32_t len, std::string_view domain) {
    if (len < domain.size()) return false;
    uint32_t limit = len - static_cast<uint32_t>(domain.size());
    char first = domain[0];

    for (uint32_t i = 0; i <= limit; ++i) {
        if (ascii_lower(static_cast<char>(data[i])) != first) continue;
        bool matched = true;
        for (size_t j = 1; j < domain.size(); ++j) {
            if (ascii_lower(static_cast<char>(data[i + j])) != domain[j]) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }
    return false;
}

static bool is_target_packet(const uint8_t* payload, uint32_t payload_len) {
    bool dc = g_discord.load(std::memory_order_relaxed);
    bool rbx = g_roblox.load(std::memory_order_relaxed);
    std::string_view sni = extract_sni(payload, payload_len);

    if (!sni.empty()) {
        if (dc) {
            for (auto d : kDiscordHosts) {
                if (host_matches(sni, d)) return true;
            }
        }
        if (rbx) {
            for (auto d : kRobloxHosts) {
                if (host_matches(sni, d)) return true;
            }
        }
        return false;
    }

    if (dc) {
        for (auto d : kDiscordHosts) {
            if (payload_contains(payload, payload_len, d)) return true;
        }
    }
    if (rbx) {
        for (auto d : kRobloxHosts) {
            if (payload_contains(payload, payload_len, d)) return true;
        }
    }

    // Post-quantum (Kyber/ML-KEM) ile 2. TCP segmentine tasan buyuk ClientHello paketlerini yakala
    uint32_t tls_rec_len = (static_cast<uint32_t>(payload[3]) << 8) | payload[4];
    if (tls_rec_len + 5 > payload_len) {
        return true;
    }
    return false;
}

// IPv6 uzanti basliklarini zincirleme atlayarak gercek TCP baslangic ofsetini bulur
static uint32_t find_ipv6_tcp(const uint8_t* pkt, uint32_t len) {
    if (len < 60) return 0;
    uint8_t next = pkt[6];
    uint32_t off = 40;

    while (off < len) {
        if (next == 6) return off;
        if (next == 0 || next == 43 || next == 60 || next == 135) {
            if (off + 2 > len) return 0;
            uint32_t ext_len = (static_cast<uint32_t>(pkt[off + 1]) + 1) * 8;
            next = pkt[off];
            off += ext_len;
        } else if (next == 44) {
            if (off + 8 > len) return 0;
            next = pkt[off];
            off += 8;
        } else if (next == 51) {
            if (off + 2 > len) return 0;
            uint32_t ext_len = (static_cast<uint32_t>(pkt[off + 1]) + 2) * 4;
            next = pkt[off];
            off += ext_len;
        } else {
            return 0;
        }
    }
    return 0;
}

static DWORD WINAPI capture_thread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    static thread_local uint8_t pkt[65535];
    static thread_local uint8_t seg1[65535];
    static thread_local uint8_t seg2[65535];
    alignas(8) uint8_t addr[128] = {};
    alignas(8) uint8_t addr1[128] = {};
    alignas(8) uint8_t addr2[128] = {};

    HANDLE h = wd_open(kKernelFilter, 0, 0, 0);
    if (h == INVALID_HANDLE_VALUE) {
        g_running = false;
        if (g_notify_wnd) PostMessageW(g_notify_wnd, WM_APP + 1, GetLastError(), 0);
        return 1;
    }

    if (wd_set_param) {
        wd_set_param(h, 0, 8192);
        wd_set_param(h, 1, 2000);
        wd_set_param(h, 2, 16777216);
    }

    EnterCriticalSection(&g_cs);
    if (!g_running.load(std::memory_order_relaxed)) {
        LeaveCriticalSection(&g_cs);
        wd_close(h);
        if (g_notify_wnd) PostMessageW(g_notify_wnd, WM_APP + 4, 0, 0);
        return 0;
    }
    g_divert = h;
    LeaveCriticalSection(&g_cs);

    UINT len = 0;
    while (g_running.load(std::memory_order_relaxed)) {
        if (!wd_recv(h, pkt, sizeof(pkt), &len, addr)) break;

        bool was_split = false;
        uint8_t ip_ver = pkt[0] >> 4;

        if (ip_ver == 4 && len >= 88 && pkt[9] == 6) {
            uint32_t ip_hdr = (pkt[0] & 0x0F) * 4;
            if (ip_hdr >= 20 && len > ip_hdr + 20) {
                uint8_t* tcp = pkt + ip_hdr;
                uint32_t tcp_hdr = ((tcp[12] >> 4) & 0x0F) * 4;
                uint32_t hdr_total = ip_hdr + tcp_hdr;

                if (tcp_hdr >= 20 && len > hdr_total + 44) {
                    uint32_t pay_len = len - hdr_total;
                    if (is_target_packet(pkt + hdr_total, pay_len)) {
                        uint32_t len1 = hdr_total + kSplitOffset;
                        uint32_t rem = pay_len - kSplitOffset;
                        uint32_t len2 = hdr_total + rem;

                        memcpy(addr1, addr, sizeof(addr));
                        memcpy(addr2, addr, sizeof(addr));

                        // 1. parca: ilk 2 bayt
                        memcpy(seg1, pkt, len1);
                        uint16_t tot1 = htons(static_cast<uint16_t>(len1));
                        memcpy(seg1 + 2, &tot1, 2);
                        seg1[10] = 0;
                        seg1[11] = 0;
                        seg1[ip_hdr + 16] = 0;
                        seg1[ip_hdr + 17] = 0;
                        wd_calc(seg1, len1, addr1, 14);
                        wd_send(h, seg1, len1, nullptr, addr1);

                        // 2. parca: kalan veri + guncellenmis TCP sequence (IP ID korunur)
                        memcpy(seg2, pkt, hdr_total);
                        memcpy(seg2 + hdr_total, pkt + hdr_total + kSplitOffset, rem);
                        uint16_t tot2 = htons(static_cast<uint16_t>(len2));
                        memcpy(seg2 + 2, &tot2, 2);

                        uint32_t seq;
                        memcpy(&seq, seg2 + ip_hdr + 4, 4);
                        seq = htonl(ntohl(seq) + kSplitOffset);
                        memcpy(seg2 + ip_hdr + 4, &seq, 4);

                        seg2[10] = 0;
                        seg2[11] = 0;
                        seg2[ip_hdr + 16] = 0;
                        seg2[ip_hdr + 17] = 0;
                        wd_calc(seg2, len2, addr2, 14);
                        wd_send(h, seg2, len2, nullptr, addr2);

                        g_packets.fetch_add(1, std::memory_order_relaxed);
                        was_split = true;
                    }
                }
            }
        } else if (ip_ver == 6 && len >= 108) {
            uint32_t tcp_off = find_ipv6_tcp(pkt, len);
            if (tcp_off >= 40 && len > tcp_off + 20) {
                uint8_t* tcp = pkt + tcp_off;
                uint32_t tcp_hdr = ((tcp[12] >> 4) & 0x0F) * 4;
                uint32_t hdr_total = tcp_off + tcp_hdr;

                if (tcp_hdr >= 20 && len > hdr_total + 44) {
                    uint32_t pay_len = len - hdr_total;
                    if (is_target_packet(pkt + hdr_total, pay_len)) {
                        uint32_t len1 = hdr_total + kSplitOffset;
                        uint32_t rem = pay_len - kSplitOffset;
                        uint32_t len2 = hdr_total + rem;

                        memcpy(addr1, addr, sizeof(addr));
                        memcpy(addr2, addr, sizeof(addr));

                        memcpy(seg1, pkt, len1);
                        uint16_t p1 = htons(static_cast<uint16_t>(len1 - 40));
                        memcpy(seg1 + 4, &p1, 2);
                        seg1[tcp_off + 16] = 0;
                        seg1[tcp_off + 17] = 0;
                        wd_calc(seg1, len1, addr1, 14);
                        wd_send(h, seg1, len1, nullptr, addr1);

                        memcpy(seg2, pkt, hdr_total);
                        memcpy(seg2 + hdr_total, pkt + hdr_total + kSplitOffset, rem);
                        uint16_t p2 = htons(static_cast<uint16_t>(len2 - 40));
                        memcpy(seg2 + 4, &p2, 2);

                        uint32_t seq;
                        memcpy(&seq, seg2 + tcp_off + 4, 4);
                        seq = htonl(ntohl(seq) + kSplitOffset);
                        memcpy(seg2 + tcp_off + 4, &seq, 4);

                        seg2[tcp_off + 16] = 0;
                        seg2[tcp_off + 17] = 0;
                        wd_calc(seg2, len2, addr2, 14);
                        wd_send(h, seg2, len2, nullptr, addr2);

                        g_packets.fetch_add(1, std::memory_order_relaxed);
                        was_split = true;
                    }
                }
            }
        }

        if (!was_split) {
            wd_send(h, pkt, len, nullptr, addr);
        }
    }

    EnterCriticalSection(&g_cs);
    g_divert = INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&g_cs);

    // Tutamac yalnizca bu noktada kapatilarak yaris durumu onlenir
    wd_close(h);
    g_running = false;
    if (g_notify_wnd) PostMessageW(g_notify_wnd, WM_APP + 4, 0, 0);
    return 0;
}

bool dpi_start(HWND notify_hwnd) {
    if (g_running) return true;
    ensure_cs();
    g_notify_wnd = notify_hwnd;

    if (g_worker) {
        WaitForSingleObject(g_worker, 500);
        CloseHandle(g_worker);
        g_worker = nullptr;
    }

    if (!has_admin_rights()) {
        MessageBoxW(notify_hwnd, L"Yonetici olarak calistirman gerekiyor.", L"ErgunDPI", MB_ICONERROR);
        return false;
    }
    if (!g_discord && !g_roblox) {
        MessageBoxW(notify_hwnd, L"En az bir hedef secili olmali.", L"ErgunDPI", MB_ICONWARNING);
        return false;
    }
    if (!load_windivert()) {
        MessageBoxW(notify_hwnd, L"WinDivert 2.2 (WinDivert.dll / WinDivert64.sys) bulunamadi.", L"ErgunDPI", MB_ICONERROR);
        return false;
    }

    g_packets = 0;
    g_running = true;
    g_worker = CreateThread(nullptr, 0, capture_thread, nullptr, 0, nullptr);
    if (!g_worker) {
        g_running = false;
        return false;
    }
    return true;
}

void dpi_stop() {
    if (!g_cs_ready) return;
    g_running = false;

    EnterCriticalSection(&g_cs);
    if (g_divert != INVALID_HANDLE_VALUE && wd_shutdown) {
        wd_shutdown(g_divert, 3);
    }
    LeaveCriticalSection(&g_cs);

    if (g_worker) {
        WaitForSingleObject(g_worker, 1500);
        CloseHandle(g_worker);
        g_worker = nullptr;
    }
}

bool dpi_is_running() { return g_running.load(std::memory_order_relaxed); }
uint64_t dpi_packet_count() { return g_packets.load(std::memory_order_relaxed); }

void dpi_set_discord(bool enabled) { g_discord = enabled; }
bool dpi_get_discord() { return g_discord.load(std::memory_order_relaxed); }

void dpi_set_roblox(bool enabled) { g_roblox = enabled; }
bool dpi_get_roblox() { return g_roblox.load(std::memory_order_relaxed); }