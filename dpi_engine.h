#pragma once
#include <windows.h>
#include <cstdint>

bool dpi_start(HWND notify_hwnd);
void dpi_stop();
bool dpi_is_running();
uint64_t dpi_packet_count();

void dpi_set_discord(bool enabled);
bool dpi_get_discord();

void dpi_set_roblox(bool enabled);
bool dpi_get_roblox();