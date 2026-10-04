#include "pipe_server.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace Halo3AP {

ModState g_modState;

static std::mutex g_logMtx;

void Log(const char* format, ...) {
    std::lock_guard<std::mutex> lock(g_logMtx);
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeStr[64];
    snprintf(timeStr, sizeof(timeStr), "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    // Write to a log file next to the game executable or in AppData
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath)) {
        std::string logFile = std::string(tempPath) + "Halo3AP_mod.log";
        std::ofstream ofs(logFile, std::ios::app);
        if (ofs.is_open()) {
            ofs << timeStr << buffer << "\n";
            ofs.flush();
        }
    }
    OutputDebugStringA((std::string(timeStr) + buffer).c_str());
}

// Helper to extract string array from JSON
static std::vector<std::string> ExtractJsonStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return result;

    size_t openBracket = json.find('[', pos);
    size_t closeBracket = json.find(']', openBracket);
    if (openBracket == std::string::npos || closeBracket == std::string::npos) return result;

    std::string arrayContent = json.substr(openBracket + 1, closeBracket - openBracket - 1);
    size_t start = 0;
    while ((start = arrayContent.find('"', start)) != std::string::npos) {
        size_t end = arrayContent.find('"', start + 1);
        if (end == std::string::npos) break;
        std::string item = arrayContent.substr(start + 1, end - start - 1);
        if (!item.empty()) {
            result.push_back(item);
        }
        start = end + 1;
    }
    return result;
}

// Simple JSON state parser
void ParseStateJson(const std::string& json) {
    std::lock_guard<std::mutex> lock(g_modState.mtx);

    // 1. Unlocked Missions
    auto missions = ExtractJsonStringArray(json, "unlocked_missions");
    if (!missions.empty() || json.find("\"unlocked_missions\"") != std::string::npos) {
        std::set<std::string> newMissions(missions.begin(), missions.end());
        if (newMissions != g_modState.unlockedMissions) {
            g_modState.unlockedMissions = std::move(newMissions);
            Log("[Halo3AP Mod] Updated unlocked missions count: %zu", g_modState.unlockedMissions.size());
        }
    }

    // 2. Unlocked Weapons
    auto weapons = ExtractJsonStringArray(json, "unlocked_weapons");
    if (!weapons.empty() || json.find("\"unlocked_weapons\"") != std::string::npos) {
        std::set<std::string> newWeapons(weapons.begin(), weapons.end());
        if (newWeapons != g_modState.unlockedWeapons) {
            std::string weaponList = "";
            for (const auto& w : weapons) {
                if (!weaponList.empty()) weaponList += ", ";
                weaponList += w;
            }
            g_modState.unlockedWeapons = std::move(newWeapons);
            Log("[Halo3AP Mod] Updated unlocked weapons count: %zu [%s]", g_modState.unlockedWeapons.size(), weaponList.c_str());
        }
    }

    // 3. Weapon License Mode
    size_t modePos = json.find("\"weapon_license_mode\"");
    if (modePos != std::string::npos) {
        size_t colon = json.find(':', modePos);
        if (colon != std::string::npos) {
            int newMode = std::atoi(json.c_str() + colon + 1);
            if (newMode != g_modState.weaponLicenseMode) {
                g_modState.weaponLicenseMode = newMode;
                Log("[Halo3AP Mod] Weapon license mode: %d", g_modState.weaponLicenseMode);
            }
        }
    }

    // 4. Starting Weapon
    size_t startPos = json.find("\"starting_weapon\"");
    if (startPos != std::string::npos) {
        size_t firstQuote = json.find('"', startPos + 17);
        if (firstQuote != std::string::npos) {
            size_t secondQuote = json.find('"', firstQuote + 1);
            if (secondQuote != std::string::npos) {
                std::string newStarter = json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                if (newStarter != g_modState.startingWeapon) {
                    g_modState.startingWeapon = std::move(newStarter);
                    Log("[Halo3AP Mod] Starting weapon: %s", g_modState.startingWeapon.c_str());
                }
            }
        }
    }
}


bool IsWeaponUnlocked(const std::string& weaponName) {
    // Heavy support weapons and turrets unlocked by default
    if (weaponName == "missile_pod" || weaponName == "machinegun_turret" || weaponName == "plasma_cannon") {
        return true;
    }
    std::lock_guard<std::mutex> lock(g_modState.mtx);
    if (g_modState.weaponLicenseMode == 0) return true;

    // If client hasn't connected or pushed weapons yet, permit default starting weapon
    if (g_modState.unlockedWeapons.empty()) {
        return weaponName == g_modState.startingWeapon;
    }

    return g_modState.unlockedWeapons.find(weaponName) != g_modState.unlockedWeapons.end();
}



struct MissionMapping {
    const char* scenario;
    const char* friendly;
};

static const MissionMapping G_MISSIONS[] = {
    {"010_jungle", "Sierra 117"},
    {"020_base", "Crow's Nest"},
    {"030_outskirts", "Tsavo Highway"},
    {"030_docks", "Tsavo Highway"},
    {"040_voi", "The Storm"},
    {"050_floodvoi", "Floodgate"},
    {"070_waste", "The Ark"},
    {"060_ark", "The Ark"},
    {"100_citadel", "The Covenant"},
    {"070_halo", "The Covenant"},
    {"110_hc", "Cortana"},
    {"080_cortana", "Cortana"},
    {"120_halo", "Halo"},
};

bool IsMissionUnlocked(const std::string& scenario) {
    std::lock_guard<std::mutex> lock(g_modState.mtx);
    // If no client is connected yet or missions haven't been pushed, allow default Sierra 117
    if (g_modState.unlockedMissions.empty()) {
        return (scenario.find("010_jungle") != std::string::npos || scenario.find("Sierra 117") != std::string::npos);
    }

    // Find corresponding friendly name
    std::string friendlyName = "";
    for (const auto& item : G_MISSIONS) {
        if (scenario.find(item.scenario) != std::string::npos) {
            friendlyName = item.friendly;
            break;
        }
    }

    for (const auto& m : g_modState.unlockedMissions) {
        if (scenario.find(m) != std::string::npos) {
            return true;
        }
        if (!friendlyName.empty() && m.find(friendlyName) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static std::string EscapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        if (c == '\\') {
            out += "\\\\";
        } else if (c == '"') {
            out += "\\\"";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out += c;
        }
    }
    return out;
}

static std::string ProcessClientMessage(const std::string& buffer) {
    ParseStateJson(buffer);

    // Build response including any pending events
    std::string ack;
    {
        std::lock_guard<std::mutex> lock(g_modState.mtx);
        if (g_modState.pendingEvents.empty()) {
            ack = "{\"status\":\"ok\"}\n";
        } else {
            // Drain pending events into the response
            ack = "{\"status\":\"ok\",\"events\":[";
            bool first = true;
            for (const auto& ev : g_modState.pendingEvents) {
                if (!first) ack += ",";
                ack += "\"" + EscapeJsonString(ev) + "\"";
                first = false;
            }
            ack += "]}\n";
            g_modState.pendingEvents.clear();
        }
    }
    return ack;
}

DWORD WINAPI PipeServerThread(LPVOID lpParam) {
    const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\Halo3Archipelago";
    Log("[Halo3AP Mod] Starting Named Pipe IPC Server on %ls", PIPE_NAME);

    while (true) {
        HANDLE hPipe = CreateNamedPipeW(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            4096,
            4096,
            0,
            nullptr
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Log("[Halo3AP Mod] Failed to create Named Pipe! Error: %lu", GetLastError());
            Sleep(2000);
            continue;
        }

        Log("[Halo3AP Mod] Named Pipe listening for Halo3Client.py connection...");
        BOOL connected = ConnectNamedPipe(hPipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            Log("[Halo3AP Mod] Client connected to Named Pipe!");
            {
                std::lock_guard<std::mutex> lock(g_modState.mtx);
                g_modState.clientConnected = true;
            }

            // Send handshake
            std::string greeting = "{\"event\":\"mod_connected\",\"version\":\"1.0.0\"}\n";
            DWORD written = 0;
            WriteFile(hPipe, greeting.c_str(), (DWORD)greeting.length(), &written, nullptr);

            char buffer[4096];
            while (true) {
                DWORD bytesRead = 0;
                BOOL success = ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
                if (!success || bytesRead == 0) {
                    break;
                }
                buffer[bytesRead] = '\0';
                std::string ack = ProcessClientMessage(buffer);
                WriteFile(hPipe, ack.c_str(), (DWORD)ack.length(), &written, nullptr);
            }

            Log("[Halo3AP Mod] Client disconnected from Named Pipe.");
            {
                std::lock_guard<std::mutex> lock(g_modState.mtx);
                g_modState.clientConnected = false;
            }
        }

        CloseHandle(hPipe);
        Sleep(500);
    }
    return 0;
}

DWORD WINAPI TcpServerThread(LPVOID lpParam) {
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        Log("[Halo3AP Mod] WSAStartup failed: %d", res);
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        Log("[Halo3AP Mod] TCP socket() failed: %d", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(29500);

    if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        Log("[Halo3AP Mod] TCP bind() failed: %d", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        Log("[Halo3AP Mod] TCP listen() failed: %d", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    Log("[Halo3AP Mod] TCP IPC Server listening on 127.0.0.1:29500 (Proton/Linux compatible)...");

    while (true) {
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock == INVALID_SOCKET) {
            Sleep(500);
            continue;
        }

        Log("[Halo3AP Mod] Client connected to TCP IPC Server!");
        {
            std::lock_guard<std::mutex> lock(g_modState.mtx);
            g_modState.clientConnected = true;
        }

        // Send handshake
        std::string greeting = "{\"event\":\"mod_connected\",\"version\":\"1.0.0\"}\n";
        send(clientSock, greeting.c_str(), (int)greeting.length(), 0);

        std::string lineBuffer;
        char buffer[4096];
        while (true) {
            int bytesRead = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead <= 0) {
                break;
            }
            buffer[bytesRead] = '\0';
            lineBuffer += buffer;

            size_t newlinePos;
            while ((newlinePos = lineBuffer.find('\n')) != std::string::npos) {
                std::string line = lineBuffer.substr(0, newlinePos);
                lineBuffer.erase(0, newlinePos + 1);

                if (line.empty()) continue;
                std::string ack = ProcessClientMessage(line);
                send(clientSock, ack.c_str(), (int)ack.length(), 0);
            }
        }

        Log("[Halo3AP Mod] Client disconnected from TCP IPC.");
        {
            std::lock_guard<std::mutex> lock(g_modState.mtx);
            g_modState.clientConnected = false;
        }
        closesocket(clientSock);
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}

} // namespace Halo3AP
