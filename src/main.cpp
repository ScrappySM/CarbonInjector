#include "pch.h"

#include "loader.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        if (HANDLE hThread = CreateThread(nullptr, 0, CarbonInjector::MainThread, hModule, 0, nullptr)) {
            CloseHandle(hThread);
        }
    }

    return TRUE;
}