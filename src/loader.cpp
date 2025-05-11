#include "pch.h"

#include "loader.h"

CarbonInjector::CarbonInjector(HMODULE hModule) : m_hModule(hModule) {
    auto exe_dir_opt = GetCurrentExeDirectory();
    if (exe_dir_opt) {
        m_modsPath = fs::path(*exe_dir_opt).parent_path() / kModsDir;
    }
}

int CarbonInjector::run() {
    if (m_modsPath.empty()) {
        showTaskDialog(
            "Failed to retrieve EXE path",
            "Could not determine the executable directory. The mod loader needs to know where it's running from to find your mods.",
            DialogIcon::Error
        );
        FreeLibraryAndExitThread(m_hModule, 1);
        return 1;
    }

    if (!ensureModsDirectory()) {
        FreeLibraryAndExitThread(m_hModule, 1);
        return 1;
    }

    int mods_loaded = loadMods();

    FreeLibraryAndExitThread(m_hModule, 0);
    return 0;
}

int CarbonInjector::loadMods() {
    std::error_code ec;
    bool any_mods_found = false;
    int mods_loaded = 0;

    // Load loose DLLs
    for (const auto& entry : fs::directory_iterator(m_modsPath, ec)) {
        if (ec) {
            showTaskDialog(
                "Directory Access Failure",
                std::format("Could not read the contents of the DLLMods directory.\n\nError details:\n{}", ec.message()),
                DialogIcon::Error
            );
            break;
        }

        if (!fs::is_regular_file(entry, ec))
            continue;

        if (entry.path().extension() == ".dll") {
            any_mods_found = true;
            if (loadModDll(entry.path()))
                mods_loaded++;
        }
    }

    if (!ec) {
        for (const auto& entry : fs::directory_iterator(m_modsPath, ec)) {
            if (ec) {
                showTaskDialog(
                    "Directory Access Failure",
                    std::format("Could not read the contents of the DLLMods directory.\n\nError details:\n{}", ec.message()),
                    DialogIcon::Error
                );
                break;
            }

            if (!fs::is_directory(entry, ec))
                continue;

            any_mods_found = true;
            const auto dir_name = entry.path().filename().string();
            const auto dll_path = entry.path() / (dir_name + ".dll");

            if (!fs::exists(dll_path, ec)) {
                showTaskDialog(
                    "Missing DLL File",
                    std::format("📂 Mod folder: {}\n\n❌ Missing: {}.dll\n\nEach mod folder needs a DLL with the same name as the folder.", dir_name, dir_name),
                    DialogIcon::Warning
                );
                continue;
            }

            if (loadModDll(dll_path))
                mods_loaded++;
        }
    }

    if (!any_mods_found && !ec) {
        showTaskDialog(
            "No Mods Found",
            std::format("No mods were found in the DLLMods folder.\n\n✨ Getting Started ✨\n1. Place DLL files directly in:\n   {}\n   OR\n2. Create a subfolder in:\n   {}\n3. Name it after your mod (e.g., \"MyAwesomeMod\")\n4. Place a DLL with the same name inside it",
                m_modsPath.string(), m_modsPath.string()),
            DialogIcon::Info
        );
    }

    return mods_loaded;
}

bool CarbonInjector::ensureModsDirectory() {
    std::error_code ec;
    if (fs::is_directory(m_modsPath, ec))
        return true;

    showTaskDialog(
        "Creating Mods Directory",
        std::format("📁 Creating a new directory for your mods at:\n{}\n\nThis is where you'll place all your mod folders.", m_modsPath.string()),
        DialogIcon::Info
    );

    fs::create_directories(m_modsPath, ec);
    if (ec) {
        showTaskDialog(
            "Couldn't Create Mods Directory",
            std::format(
                "❌ Failed to create the DLLMods directory.\n\nError details:\n{}\n\nPossible solutions:\n• Make sure the game isn't in a protected location\n• Run the game as administrator\n• Check your antivirus isn't blocking file creation",
                ec.message()
            ),
            DialogIcon::Error
        );
        return false;
    }

    showTaskDialog(
        "Mods Directory Created!",
        "✅ The DLLMods directory has been created successfully!\n\nYou can now add your mods there.",
        DialogIcon::Info
    );

    return true;
}

bool CarbonInjector::loadModDll(const fs::path& dll_path) {
    HMODULE loaded_dll = LoadLibraryA(dll_path.string().c_str());
    if (!loaded_dll) {
        DWORD error = GetLastError();
        showTaskDialog(
            "Mod Loading Failed",
            std::format("❌ Failed to load mod:\n{}\n\nError details:\n{}\n\nPossible solutions:\n• The DLL might be corrupted\n• It might have missing dependencies\n• It might not be compatible with this game version",
                dll_path.filename().string(),
                FormatWindowsError(error)),
            DialogIcon::Error
        );
        return false;
    }
    return true;
}

void CarbonInjector::showTaskDialog(std::wstring_view main_message, std::wstring_view content, DialogIcon icon) const {
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX) };
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    TASKDIALOGCONFIG config{};
    TASKDIALOG_BUTTON buttons[] = { {IDOK, L"Got it!"} };

    config.cbSize = sizeof(TASKDIALOGCONFIG);
    config.hwndParent = nullptr;
    config.hInstance = GetModuleHandle(nullptr);
    config.dwFlags = TDF_USE_COMMAND_LINKS | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION | TDF_ENABLE_HYPERLINKS;

    config.pszWindowTitle = L"Carbon Injector";
    config.pszMainInstruction = main_message.data();
    config.pszContent = content.data();

    switch (icon) {
        case DialogIcon::Error:
            config.pszMainIcon = TD_ERROR_ICON;
            break;
        case DialogIcon::Warning:
            config.pszMainIcon = TD_WARNING_ICON;
            break;
        case DialogIcon::Info:
        default:
            config.pszMainIcon = TD_INFORMATION_ICON;
            break;
    }

    config.pszFooter = L"<a href=\"https://github.com/ScrappySM/CarbonLauncher\">Carbon Launcher v2</a>";
    config.pszFooterIcon = TD_INFORMATION_ICON;

    config.cButtons = std::size(buttons);
    config.pButtons = buttons;
    config.nDefaultButton = IDOK;

    TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
}

void CarbonInjector::showTaskDialog(std::string_view main_message, std::string_view content, DialogIcon icon) const {
    int wMainLen = MultiByteToWideChar(CP_UTF8, 0, main_message.data(),
        static_cast<int>(main_message.size()), nullptr, 0);
    int wContentLen = MultiByteToWideChar(CP_UTF8, 0, content.data(),
        static_cast<int>(content.size()), nullptr, 0);

    std::wstring w_main(wMainLen, 0);
    std::wstring w_content(wContentLen, 0);

    MultiByteToWideChar(CP_UTF8, 0, main_message.data(),
        static_cast<int>(main_message.size()), &w_main[0], wMainLen);
    MultiByteToWideChar(CP_UTF8, 0, content.data(),
        static_cast<int>(content.size()), &w_content[0], wContentLen);

    showTaskDialog(w_main, w_content, icon);
}

std::optional<std::string> CarbonInjector::GetCurrentExeDirectory() {
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);

    if (len == 0 || len == MAX_PATH) {
        return std::nullopt;
    }

    return fs::path(buffer).parent_path().string();
}

std::string CarbonInjector::FormatWindowsError(DWORD error_code) {
    char* message_buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

    if (size == 0 || !message_buffer) {
        return std::format("Unknown error code: {}", error_code);
    }

    std::string message(message_buffer, size);
    LocalFree(message_buffer);

    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' '))
        message.pop_back();

    return message;
}

DWORD WINAPI CarbonInjector::MainThread(LPVOID lp_param) {
    HMODULE hModule = reinterpret_cast<HMODULE>(lp_param);
    CarbonInjector loader(hModule);
    return loader.run();
}
