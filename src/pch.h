// pch.h - 预编译头
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX        // 防止 Windows.h 定义 min/max 宏，避免与 std::max 冲突
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <commctrl.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <memory>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")


#define TRAFFICE_MONITOR_PLUGIN
#include "../include/PluginInterface.h"
