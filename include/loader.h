#pragma once

#include "pch.h"

namespace fs = std::filesystem;

enum class DialogIcon {
    Info,
    Error,
    Warning
};

class CarbonInjector {
public:
    CarbonInjector(HMODULE hModule);

    static DWORD WINAPI MainThread(LPVOID lp_param);

private:
    HMODULE m_hModule;
    fs::path m_modsPath;

    bool ensureModsDirectory();
    bool loadModDll(const fs::path& dll_path);
    int loadMods();

    void showTaskDialog(std::wstring_view main_message, std::wstring_view content, DialogIcon icon) const;
    void showTaskDialog(std::string_view main_message, std::string_view content, DialogIcon icon) const;

    static std::optional<std::string> GetCurrentExeDirectory();
    static std::string FormatWindowsError(DWORD error_code);

    int run();

    static constexpr const char* kModsDir = "DLLMods";
};
