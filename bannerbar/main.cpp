#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <thread>
#include <mutex>
#include <iomanip>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

// AppBar messages
#define ABM_NEW 0x00000000
#define ABM_REMOVE 0x00000001
#define ABM_SETPOS 0x00000003
#define ABE_TOP 1
#define ABE_BOTTOM 3

// Custom messages
#define WM_TRAYICON (WM_USER + 1)
#define WM_UPDATE_DISPLAY (WM_USER + 2)
#define WM_RECREATE_WINDOWS (WM_USER + 3)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_STATUS_BASE 2000

// Configuration file
const wchar_t* CONFIG_FILE = L"config.json";

struct BannerConfig {
    bool enable = true;
    std::wstring centerText;
    std::wstring leftText;
    std::wstring rightText;
    int size = 20;
};

struct TextOptions {
    std::wstring font = L"Arial";
    int size = 15;
};

struct BorderConfig {
    bool enable = true;
    int size = 3;
};

struct NetSyncConfig {
    bool enable = false;
    std::wstring ip = L"127.0.0.1";
    int port = 51320;
};

struct ClassificationStatus {
    std::wstring id;
    std::wstring color;
    int updateInterval = 10000;
    BannerConfig topBanner;
    BannerConfig bottomBanner;
    TextOptions textOptions;
    BorderConfig border;
};

struct Config {
    int display = -1;
    NetSyncConfig netsync;
    std::vector<ClassificationStatus> status;
    std::wstring currentStatusId;
};

struct MonitorWindows {
    HWND bannerTop;
    HWND bannerBottom;
    HWND borderTop;
    HWND borderBottom;
    HWND borderLeft;
    HWND borderRight;
    RECT monitorRect;
};

// Global variables
std::vector<MonitorWindows> g_monitorWindows;
Config g_config;
std::mutex g_configMutex;
SOCKET g_udpSocket = INVALID_SOCKET;
SOCKET g_tcpSocket = INVALID_SOCKET;
std::thread g_udpThread;
std::thread g_tcpThread;
std::thread g_updateThread;
bool g_running = true;
NOTIFYICONDATA g_nid = {};
HWND g_mainWindow = NULL;

// Forward declarations
std::wstring ExpandTemplate(const std::wstring& text);
COLORREF HexToRGB(const std::wstring& hex);
std::wstring UTF8ToWide(const std::string& str);
std::string WideToUTF8(const std::wstring& wstr);
std::wstring GetOSVersion();
std::wstring GetKernelVersion();

// Utility functions
std::wstring UTF8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::vector<wchar_t> buffer(size);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer.data(), size);
    return buffer.data();
}

std::string WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::vector<char> buffer(size);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer.data(), size, NULL, NULL);
    return buffer.data();
}

COLORREF HexToRGB(const std::wstring& hex) {
    std::wstring h = hex;
    if (h[0] == L'#') h = h.substr(1);
    unsigned int value = std::stoul(h, nullptr, 16);
    return RGB((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
}

std::wstring Trim(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\n\r");
    if (first == std::wstring::npos) return L"";
    size_t last = str.find_last_not_of(L" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// JSON parsing helpers
std::wstring GetJsonString(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return L"";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return L"";
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return L"";
    size_t end = json.find("\"", pos + 1);
    if (end == std::string::npos) return L"";
    return UTF8ToWide(json.substr(pos + 1, end - pos - 1));
}

int GetJsonInt(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0;
    pos = json.find_first_not_of(" \t\n\r", pos + 1);
    if (pos == std::string::npos) return 0;
    return std::stoi(json.substr(pos));
}

bool GetJsonBool(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return false;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return false;
    pos = json.find_first_not_of(" \t\n\r", pos + 1);
    if (pos == std::string::npos) return false;
    return json.substr(pos, 4) == "true";
}

// System information functions
std::wstring GetHostname() {
    wchar_t buffer[256];
    DWORD size = 256;
    if (GetComputerNameW(buffer, &size)) {
        return buffer;
    }
    return L"UNKNOWN";
}

std::wstring GetUsername() {
    wchar_t buffer[256];
    DWORD size = 256;
    if (GetUserNameW(buffer, &size)) {
        return buffer;
    }
    return L"UNKNOWN";
}

std::wstring GetLocalIP() {
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
    ULONG bufLen = sizeof(IP_ADAPTER_INFO);

    if (GetAdaptersInfo(pAdapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(bufLen);
    }

    std::wstring ip = L"0.0.0.0";
    if (GetAdaptersInfo(pAdapterInfo, &bufLen) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET || pAdapter->Type == IF_TYPE_IEEE80211) {
                ip = UTF8ToWide(pAdapter->IpAddressList.IpAddress.String);
                if (ip != L"0.0.0.0") break;
            }
            pAdapter = pAdapter->Next;
        }
    }

    free(pAdapterInfo);
    return ip;
}

std::wstring GetOSVersion() {
    OSVERSIONINFOEXW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOEXW);
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion) {
            RtlGetVersion((PRTL_OSVERSIONINFOEXW)&osvi);
            wchar_t buf[64];
            swprintf(buf, 64, L"Windows %d.%d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
            return buf;
        }
    }
    return L"Windows";
}

std::wstring GetKernelVersion() {
    OSVERSIONINFOEXW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOEXW);
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion) {
            RtlGetVersion((PRTL_OSVERSIONINFOEXW)&osvi);
            wchar_t buf[64];
            swprintf(buf, 64, L"NT %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
            return buf;
        }
    }
    return L"NT";
}

std::wstring GetCPUUsage() {
    static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
    static int numProcessors = 0;
    static bool initialized = false;

    if (!initialized) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;

        FILETIME ftime, fsys, fuser;
        GetSystemTimeAsFileTime(&ftime);
        memcpy(&lastCPU, &ftime, sizeof(FILETIME));

        HANDLE self = GetCurrentProcess();
        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));

        initialized = true;
        return L"0%";
    }

    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));

    HANDLE self = GetCurrentProcess();
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));

    double percent = 0.0;
    if (now.QuadPart != lastCPU.QuadPart) {
        percent = (sys.QuadPart - lastSysCPU.QuadPart) +
            (user.QuadPart - lastUserCPU.QuadPart);
        percent /= (now.QuadPart - lastCPU.QuadPart);
        percent /= numProcessors;
        percent *= 100;
    }

    lastCPU = now;
    lastUserCPU = user;
    lastSysCPU = sys;

    wchar_t buf[32];
    swprintf(buf, 32, L"%.1f%%", percent);
    return buf;
}

std::wstring GetMemoryUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    DWORDLONG totalMB = memInfo.ullTotalPhys / (1024 * 1024);
    DWORDLONG usedMB = (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024);

    wchar_t buf[64];
    swprintf(buf, 64, L"%llu/%llu MB", usedMB, totalMB);
    return buf;
}

std::wstring ExpandTemplate(const std::wstring& text) {
    std::wstring result = text;

    auto replace = [&](const std::wstring& token, const std::wstring& value) {
        size_t pos = 0;
        while ((pos = result.find(token, pos)) != std::wstring::npos) {
            result.replace(pos, token.length(), value);
            pos += value.length();
        }
        };

    replace(L"<HOST>", GetHostname());
    replace(L"<USER>", GetUsername());
    replace(L"<IP>", GetLocalIP());
    replace(L"<LOCALIP>", GetLocalIP());
    replace(L"<CPU_USE>", GetCPUUsage());
    replace(L"<MEMORY_CURRENT>", GetMemoryUsage());
    replace(L"<OS>", GetOSVersion());
    replace(L"<KERNEL>", GetKernelVersion());

    // Date and time
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t dateStr[64], timeStr[64];
    swprintf(dateStr, 64, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    swprintf(timeStr, 64, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    replace(L"<DATE>", dateStr);
    replace(L"<TIME>", timeStr);

    return result;
}

// Get current classification status
ClassificationStatus GetCurrentStatus() {
    std::lock_guard<std::mutex> lock(g_configMutex);
    for (const auto& st : g_config.status) {
        if (st.id == g_config.currentStatusId) {
            return st;
        }
    }
    if (!g_config.status.empty()) {
        return g_config.status[0];
    }
    ClassificationStatus def;
    def.id = L"unclassified";
    def.color = L"#008000";
    def.topBanner.centerText = L"UNCLASSIFIED";
    return def;
}

// Save default configuration
void SaveDefaultConfig() {
    std::ofstream file(CONFIG_FILE);
    if (!file.is_open()) return;

    file << "{\n";
    file << "\t\"display\": -1,\n";
    file << "\t\"netsync\": {\n";
    file << "\t\t\"enable\": false,\n";
    file << "\t\t\"ip\": \"127.0.0.1\",\n";
    file << "\t\t\"port\": 51320\n";
    file << "\t},\n";
    file << "\t\"status\": [\n";
    file << "\t\t{\n";
    file << "\t\t\t\"id\": \"unclassified\",\n";
    file << "\t\t\t\"color\": \"#008000\",\n";
    file << "\t\t\t\"update_interval\": 10000,\n";
    file << "\t\t\t\"top_banner\": {\n";
    file << "\t\t\t\t\"enable\": true,\n";
    file << "\t\t\t\t\"center_text\": \"UNCLASSIFIED\",\n";
    file << "\t\t\t\t\"left_text\": \"<IP> | <LOCALIP> | <CPU_USE> | <MEMORY_CURRENT>\",\n";
    file << "\t\t\t\t\"right_text\": \"CALLSIGN\",\n";
    file << "\t\t\t\t\"size\": 20\n";
    file << "\t\t\t},\n";
    file << "\t\t\t\"bottom_banner\": {\n";
    file << "\t\t\t\t\"enable\": false,\n";
    file << "\t\t\t\t\"center_text\": \"UNCLASSIFIED\",\n";
    file << "\t\t\t\t\"left_text\": \"<HOST> | <USER> | <KERNEL> | <OS>\",\n";
    file << "\t\t\t\t\"right_text\": \"FPCON\",\n";
    file << "\t\t\t\t\"size\": 20\n";
    file << "\t\t\t},\n";
    file << "\t\t\t\"text_options\": {\n";
    file << "\t\t\t\t\"font\": \"Arial\",\n";
    file << "\t\t\t\t\"size\": 15\n";
    file << "\t\t\t},\n";
    file << "\t\t\t\"border\": {\n";
    file << "\t\t\t\t\"enable\": true,\n";
    file << "\t\t\t\t\"size\": 3\n";
    file << "\t\t\t}\n";
    file << "\t\t},\n";
    file << "\t\t{\n";
    file << "\t\t\t\"id\": \"secret\",\n";
    file << "\t\t\t\"color\": \"#FF0000\",\n";
    file << "\t\t\t\"update_interval\": 10000,\n";
    file << "\t\t\t\"top_banner\": {\n";
    file << "\t\t\t\t\"enable\": true,\n";
    file << "\t\t\t\t\"center_text\": \"SECRET\",\n";
    file << "\t\t\t\t\"left_text\": \"<IP> | <LOCALIP> | <CPU_USE> | <MEMORY_CURRENT>\",\n";
    file << "\t\t\t\t\"right_text\": \"CLASSIFIED\",\n";
    file << "\t\t\t\t\"size\": 20\n";
    file << "\t\t\t},\n";
    file << "\t\t\t\"bottom_banner\": {\n";
    file << "\t\t\t\t\"enable\": true,\n";
    file << "\t\t\t\t\"center_text\": \"SECRET\",\n";
    file << "\t\t\t\t\"left_text\": \"<HOST> | <USER> | <KERNEL> | <OS>\",\n";
    file << "\t\t\t\t\"right_text\": \"TOP SECRET\",\n";
    file << "\t\t\t\t\"size\": 20\n";
    file << "\t\t\t},\n";
    file << "\t\t\t\"text_options\": {\n";
    file << "\t\t\t\t\"font\": \"Arial\",\n";
    file << "\t\t\t\t\"size\": 15\n";
    file << "\t\t\t},\n";
    file << "\t\t\t\"border\": {\n";
    file << "\t\t\t\t\"enable\": true,\n";
    file << "\t\t\t\t\"size\": 5\n";
    file << "\t\t\t}\n";
    file << "\t\t}\n";
    file << "\t]\n";
    file << "}\n";

    file.close();
}

// Load configuration
bool LoadConfig() {
    std::ifstream file(CONFIG_FILE);
    if (!file.is_open()) {
        SaveDefaultConfig();
        file.open(CONFIG_FILE);
        if (!file.is_open()) return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    g_config.display = GetJsonInt(json, "display");

    // Parse netsync
    size_t netsyncPos = json.find("\"netsync\"");
    if (netsyncPos != std::string::npos) {
        size_t objStart = json.find("{", netsyncPos);
        size_t objEnd = json.find("}", objStart);
        std::string netsyncObj = json.substr(objStart, objEnd - objStart + 1);

        g_config.netsync.enable = GetJsonBool(netsyncObj, "enable");
        g_config.netsync.ip = GetJsonString(netsyncObj, "ip");
        g_config.netsync.port = GetJsonInt(netsyncObj, "port");
    }

    // Parse status array
    size_t statusPos = json.find("\"status\"");
    if (statusPos != std::string::npos) {
        size_t arrayStart = json.find("[", statusPos);
        size_t arrayEnd = json.rfind("]");
        std::string statusArray = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

        size_t objStart = 0;
        while ((objStart = statusArray.find("{", objStart)) != std::string::npos) {
            int braceCount = 1;
            size_t objEnd = objStart + 1;
            while (objEnd < statusArray.length() && braceCount > 0) {
                if (statusArray[objEnd] == '{') braceCount++;
                if (statusArray[objEnd] == '}') braceCount--;
                objEnd++;
            }

            std::string obj = statusArray.substr(objStart, objEnd - objStart);

            ClassificationStatus st;
            st.id = GetJsonString(obj, "id");
            st.color = GetJsonString(obj, "color");
            st.updateInterval = GetJsonInt(obj, "update_interval");
            if (st.updateInterval == 0) st.updateInterval = 10000;

            // Parse top_banner
            size_t topBannerPos = obj.find("\"top_banner\"");
            if (topBannerPos != std::string::npos) {
                size_t tbStart = obj.find("{", topBannerPos);
                size_t tbEnd = obj.find("}", tbStart);
                std::string tbObj = obj.substr(tbStart, tbEnd - tbStart + 1);

                st.topBanner.enable = GetJsonBool(tbObj, "enable");
                st.topBanner.centerText = GetJsonString(tbObj, "center_text");
                st.topBanner.leftText = GetJsonString(tbObj, "left_text");
                st.topBanner.rightText = GetJsonString(tbObj, "right_text");
                st.topBanner.size = GetJsonInt(tbObj, "size");
                if (st.topBanner.size == 0) st.topBanner.size = 20;
            }

            // Parse bottom_banner
            size_t bottomBannerPos = obj.find("\"bottom_banner\"");
            if (bottomBannerPos != std::string::npos) {
                size_t bbStart = obj.find("{", bottomBannerPos);
                size_t bbEnd = obj.find("}", bbStart);
                std::string bbObj = obj.substr(bbStart, bbEnd - bbStart + 1);

                st.bottomBanner.enable = GetJsonBool(bbObj, "enable");
                st.bottomBanner.centerText = GetJsonString(bbObj, "center_text");
                st.bottomBanner.leftText = GetJsonString(bbObj, "left_text");
                st.bottomBanner.rightText = GetJsonString(bbObj, "right_text");
                st.bottomBanner.size = GetJsonInt(bbObj, "size");
                if (st.bottomBanner.size == 0) st.bottomBanner.size = 20;
            }

            // Parse text_options
            size_t textOptsPos = obj.find("\"text_options\"");
            if (textOptsPos != std::string::npos) {
                size_t toStart = obj.find("{", textOptsPos);
                size_t toEnd = obj.find("}", toStart);
                std::string toObj = obj.substr(toStart, toEnd - toStart + 1);

                st.textOptions.font = GetJsonString(toObj, "font");
                if (st.textOptions.font.empty()) st.textOptions.font = L"Arial";
                st.textOptions.size = GetJsonInt(toObj, "size");
                if (st.textOptions.size == 0) st.textOptions.size = 15;
            }

            // Parse border
            size_t borderPos = obj.find("\"border\"");
            if (borderPos != std::string::npos) {
                size_t bStart = obj.find("{", borderPos);
                size_t bEnd = obj.find("}", bStart);
                std::string bObj = obj.substr(bStart, bEnd - bStart + 1);

                st.border.enable = GetJsonBool(bObj, "enable");
                st.border.size = GetJsonInt(bObj, "size");
                if (st.border.size == 0) st.border.size = 3;
            }

            g_config.status.push_back(st);
            objStart = objEnd;
        }
    }

    if (!g_config.status.empty()) {
        g_config.currentStatusId = g_config.status[0].id;
    }

    return true;
}

// AppBar functions
void RegisterAppBar(HWND hwnd, const RECT& rect, bool isTop) {
    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = hwnd;
    SHAppBarMessage(ABM_NEW, &abd);

    abd.uEdge = isTop ? ABE_TOP : ABE_BOTTOM;
    abd.rc = rect;
    SHAppBarMessage(ABM_SETPOS, &abd);
}

void RemoveAppBar(HWND hwnd) {
    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = hwnd;
    SHAppBarMessage(ABM_REMOVE, &abd);
}

// Window procedures
LRESULT CALLBACK BannerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        ClassificationStatus status = GetCurrentStatus();
        COLORREF bgColor = HexToRGB(status.color);

        // Fill background
        HBRUSH brush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        // Determine if this is top or bottom banner
        bool isTop = (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
        BannerConfig& banner = isTop ? status.topBanner : status.bottomBanner;

        if (banner.enable) {
            // Setup text drawing
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 0, 0));

            HFONT hFont = CreateFontW(
                status.textOptions.size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                status.textOptions.font.c_str()
            );
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            // Draw center text
            DrawTextW(hdc, banner.centerText.c_str(), -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Draw left text
            std::wstring leftText = ExpandTemplate(banner.leftText);
            RECT leftRect = rect;
            leftRect.left += 10;
            DrawTextW(hdc, leftText.c_str(), -1, &leftRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Draw right text
            std::wstring rightText = ExpandTemplate(banner.rightText);
            RECT rightRect = rect;
            rightRect.right -= 10;
            DrawTextW(hdc, rightText.c_str(), -1, &rightRect,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_UPDATE_DISPLAY:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_DESTROY:
        RemoveAppBar(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK BorderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        ClassificationStatus status = GetCurrentStatus();
        COLORREF bgColor = HexToRGB(status.color);
        HBRUSH brush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_UPDATE_DISPLAY:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Create overlay window
HWND CreateOverlayWindow(LPCWSTR className, WNDPROC wndProc, int x, int y, int width, int height, bool isAppBar = false, bool isTop = true) {
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        className, L"", WS_POPUP,
        x, y, width, height,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (hwnd) {
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, isTop ? 0 : 1);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        if (isAppBar) {
            RECT rect = { x, y, x + width, y + height };
            RegisterAppBar(hwnd, rect, isTop);
        }
    }

    return hwnd;
}

// Monitor enumeration
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    static int monitorIndex = 0;

    // Check if we should show on this monitor
    if (g_config.display >= 0 && monitorIndex != g_config.display) {
        monitorIndex++;
        return TRUE;
    }

    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &mi);

    int x = mi.rcMonitor.left;
    int y = mi.rcMonitor.top;
    int width = mi.rcMonitor.right - mi.rcMonitor.left;
    int height = mi.rcMonitor.bottom - mi.rcMonitor.top;

    ClassificationStatus status = GetCurrentStatus();

    MonitorWindows mw = {};
    mw.monitorRect = mi.rcMonitor;

    // Create top banner if enabled
    if (status.topBanner.enable) {
        mw.bannerTop = CreateOverlayWindow(
            L"ClassificationBanner", BannerWndProc,
            x, y, width, status.topBanner.size, true, true
        );
        SetWindowPos(mw.bannerTop, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }

    // Create bottom banner if enabled
    if (status.bottomBanner.enable) {
        mw.bannerBottom = CreateOverlayWindow(
            L"ClassificationBanner", BannerWndProc,
            x, y + height - status.bottomBanner.size, width, status.bottomBanner.size, true, false
        );
        SetWindowPos(mw.bannerBottom, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }

    // Create borders if enabled
    if (status.border.enable) {
        int topOffset = status.topBanner.enable ? status.topBanner.size : 0;
        int bottomOffset = status.bottomBanner.enable ? status.bottomBanner.size : 0;
        int borderHeight = height - topOffset - bottomOffset;

        mw.borderTop = CreateOverlayWindow(
            L"ClassificationBorder", BorderWndProc,
            x, y + topOffset, width, status.border.size
        );

        mw.borderBottom = CreateOverlayWindow(
            L"ClassificationBorder", BorderWndProc,
            x, y + height - bottomOffset - status.border.size, width, status.border.size
        );

        mw.borderLeft = CreateOverlayWindow(
            L"ClassificationBorder", BorderWndProc,
            x, y + topOffset, status.border.size, borderHeight
        );

        mw.borderRight = CreateOverlayWindow(
            L"ClassificationBorder", BorderWndProc,
            x + width - status.border.size, y + topOffset, status.border.size, borderHeight
        );

        SetWindowPos(mw.borderTop, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(mw.borderBottom, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(mw.borderLeft, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(mw.borderRight, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    g_monitorWindows.push_back(mw);
    monitorIndex++;

    return TRUE;
}

// Update all displays
void UpdateAllDisplays() {
    for (const auto& mw : g_monitorWindows) {
        if (mw.bannerTop) SendMessage(mw.bannerTop, WM_UPDATE_DISPLAY, 0, 0);
        if (mw.bannerBottom) SendMessage(mw.bannerBottom, WM_UPDATE_DISPLAY, 0, 0);
        if (mw.borderTop) SendMessage(mw.borderTop, WM_UPDATE_DISPLAY, 0, 0);
        if (mw.borderBottom) SendMessage(mw.borderBottom, WM_UPDATE_DISPLAY, 0, 0);
        if (mw.borderLeft) SendMessage(mw.borderLeft, WM_UPDATE_DISPLAY, 0, 0);
        if (mw.borderRight) SendMessage(mw.borderRight, WM_UPDATE_DISPLAY, 0, 0);
    }
}

// TCP client thread for netsync
void TCPClientThread() {
    if (!g_config.netsync.enable) return;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    while (g_running) {
        g_tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (g_tcpSocket == INVALID_SOCKET) {
            Sleep(5000);
            continue;
        }

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(g_config.netsync.port);
        InetPtonW(AF_INET, g_config.netsync.ip.c_str(), &serverAddr.sin_addr);

        if (connect(g_tcpSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(g_tcpSocket);
            Sleep(5000);
            continue;
        }

        // Connected - listen for commands
        char buffer[4096];
        while (g_running) {
            int received = recv(g_tcpSocket, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;

            buffer[received] = '\0';
            std::string cmd(buffer);

            // Parse command
            if (cmd.find("SET_STATUS:") == 0) {
                std::string statusId = cmd.substr(11);
                // Remove any trailing newlines or whitespace
                statusId.erase(statusId.find_last_not_of(" \n\r\t") + 1);

                {
                    std::lock_guard<std::mutex> lock(g_configMutex);
                    g_config.currentStatusId = UTF8ToWide(statusId);
                }

                // Signal main thread to recreate windows
                PostMessage(g_mainWindow, WM_RECREATE_WINDOWS, 0, 0);

                std::string response = "OK\n";
                send(g_tcpSocket, response.c_str(), response.length(), 0);
            }
            else if (cmd.find("GET_STATUS") == 0) {
                std::lock_guard<std::mutex> lock(g_configMutex);
                std::string response = "CURRENT_STATUS:" + WideToUTF8(g_config.currentStatusId) + "\n";
                send(g_tcpSocket, response.c_str(), response.length(), 0);
            }
            else if (cmd.find("RELOAD_CONFIG") == 0) {
                LoadConfig();
                PostMessage(g_mainWindow, WM_RECREATE_WINDOWS, 0, 0);
                std::string response = "CONFIG_RELOADED\n";
                send(g_tcpSocket, response.c_str(), response.length(), 0);
            }
        }

        closesocket(g_tcpSocket);
        Sleep(5000); // Wait before reconnecting
    }

    WSACleanup();
}

// UDP server thread (legacy support)
void UDPServerThread() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    g_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_udpSocket == INVALID_SOCKET) return;

    u_long mode = 1;
    ioctlsocket(g_udpSocket, FIONBIO, &mode);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(4312);

    if (bind(g_udpSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(g_udpSocket);
        return;
    }

    char buffer[4096];
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    while (g_running) {
        int received = recvfrom(g_udpSocket, buffer, sizeof(buffer) - 1, 0,
            (sockaddr*)&clientAddr, &clientAddrLen);

        if (received > 0) {
            buffer[received] = '\0';
            std::string cmd(buffer);

            if (cmd.find("SET_STATUS:") == 0) {
                std::string statusId = cmd.substr(11);
                {
                    std::lock_guard<std::mutex> lock(g_configMutex);
                    g_config.currentStatusId = UTF8ToWide(statusId);
                }
                PostMessage(g_mainWindow, WM_RECREATE_WINDOWS, 0, 0);

                std::string response = "OK";
                sendto(g_udpSocket, response.c_str(), response.length(), 0,
                    (sockaddr*)&clientAddr, clientAddrLen);
            }
            else if (cmd == "GET_STATUS") {
                std::lock_guard<std::mutex> lock(g_configMutex);
                std::string response = "Current status: " + WideToUTF8(g_config.currentStatusId);
                sendto(g_udpSocket, response.c_str(), response.length(), 0,
                    (sockaddr*)&clientAddr, clientAddrLen);
            }
            else if (cmd == "RELOAD_CONFIG") {
                LoadConfig();
                PostMessage(g_mainWindow, WM_RECREATE_WINDOWS, 0, 0);
                std::string response = "Config reloaded";
                sendto(g_udpSocket, response.c_str(), response.length(), 0,
                    (sockaddr*)&clientAddr, clientAddrLen);
            }
        }

        Sleep(10);
    }

    closesocket(g_udpSocket);
    WSACleanup();
}

// Update thread for periodic refresh
void UpdateDisplayThread() {
    while (g_running) {
        ClassificationStatus status = GetCurrentStatus();
        Sleep(status.updateInterval);
        UpdateAllDisplays();
    }
}

// System tray
void AddTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Classification Banner");

    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

void ShowTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();

    // Add status options
    for (size_t i = 0; i < g_config.status.size(); i++) {
        std::string text = WideToUTF8(g_config.status[i].topBanner.centerText);
        AppendMenuA(hMenu, MF_STRING, ID_TRAY_STATUS_BASE + i, text.c_str());

        if (g_config.status[i].id == g_config.currentStatusId) {
            CheckMenuItem(hMenu, ID_TRAY_STATUS_BASE + i, MF_CHECKED);
        }
    }

    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// Cleanup
void CleanupWindows() {
    for (auto& mw : g_monitorWindows) {
        if (mw.bannerTop) DestroyWindow(mw.bannerTop);
        if (mw.bannerBottom) DestroyWindow(mw.bannerBottom);
        if (mw.borderTop) DestroyWindow(mw.borderTop);
        if (mw.borderBottom) DestroyWindow(mw.borderBottom);
        if (mw.borderLeft) DestroyWindow(mw.borderLeft);
        if (mw.borderRight) DestroyWindow(mw.borderRight);
    }
    g_monitorWindows.clear();
}

// Hidden window for tray icon
LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        AddTrayIcon(hwnd);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            PostQuitMessage(0);
            CleanupWindows();
            g_running = false;
        }
        else if (LOWORD(wParam) >= ID_TRAY_STATUS_BASE) {
            int index = LOWORD(wParam) - ID_TRAY_STATUS_BASE;

            if (index < (int)g_config.status.size()) {
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_config.currentStatusId = g_config.status[index].id;
                EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
            }
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Register window classes
bool RegisterWindowClasses() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = BannerWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ClassificationBanner";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassExW(&wc)) return false;

    wc.lpfnWndProc = BorderWndProc;
    wc.lpszClassName = L"ClassificationBorder";
    if (!RegisterClassExW(&wc)) return false;

    wc.lpfnWndProc = HiddenWndProc;
    wc.lpszClassName = L"ClassificationHidden";
    if (!RegisterClassExW(&wc)) return false;

    return true;
}

// Recreate all windows (must be called from main thread)
void RecreateWindows() {
    CleanupWindows();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LoadConfig();

    if (!RegisterWindowClasses()) {
        MessageBoxW(NULL, L"Failed to register window classes", L"Error", MB_ICONERROR);
        return 1;
    }

    // Create hidden window for tray
    g_mainWindow = CreateWindowExW(0, L"ClassificationHidden", L"", WS_OVERLAPPEDWINDOW,
        0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    // Create windows for all monitors
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);

    // Start UDP server thread (legacy)
    g_udpThread = std::thread(UDPServerThread);

    // Start TCP client thread if netsync enabled
    if (g_config.netsync.enable) {
        g_tcpThread = std::thread(TCPClientThread);
    }

    // Start update thread
    g_updateThread = std::thread(UpdateDisplayThread);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

		if (msg.message == WM_RECREATE_WINDOWS) {
			RecreateWindows();
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		if (msg.message == WM_COMMAND) {
			if (LOWORD(msg.wParam) == ID_TRAY_EXIT) {
				break;
			}
		}

		if (!g_running) {
			break;
		}
    }

    // Cleanup
    g_running = false;
    if (g_udpThread.joinable()) g_udpThread.join();
    if (g_tcpThread.joinable()) g_tcpThread.join();
    if (g_updateThread.joinable()) g_updateThread.join();
    CleanupWindows();

    return (int)msg.wParam;
}