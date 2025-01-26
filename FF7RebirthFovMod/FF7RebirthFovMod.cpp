#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <vector>

std::atomic<bool> running;
// Default key bindings (Keypad +, Keypad -, Keypad *, Keypad /)
const DWORD DEFAULT_INCREASE_KEY = VK_ADD;  
const DWORD DEFAULT_DECREASE_KEY = VK_SUBTRACT;  
const DWORD DEFAULT_SET60_KEY = VK_MULTIPLY;     
const DWORD DEFAULT_RESET_KEY = VK_DIVIDE;     

struct HotkeyConfig
{
    DWORD increaseKey;
    DWORD decreaseKey;
    DWORD set60Key;
    DWORD resetKey;
	float initialFov;
};

BOOL WINAPI ConsoleHandler( DWORD signal )
{
    if (signal == CTRL_CLOSE_EVENT)
    {
        running = false;
        return TRUE;
    }
    return FALSE;
}

bool IsProcessRunning( HANDLE hProcess )
{
    DWORD exitCode;
    return GetExitCodeProcess( hProcess, &exitCode ) && exitCode == STILL_ACTIVE;
}

void CreateDefaultConfig()
{
    std::ofstream file( "FF7RebirthFovMod.ini" );
    if (file.is_open())
    {
        file << "[Settings]\n";
        file << "; Example key codes (hex values):\n";
		file << ";  Keypad + = 0x6B, Keypad - = 0x6D, Keypad * = 0x6A, Keypad / = 0x6F\n";
        file << ";  PageUp=0x21, PageDown=0x22, Home=0x24, End=0x23\n";
        file << ";  Arrow keys: Left=0x25, Up=0x26, Right=0x27, Down=0x28\n";
        file << ";  Function keys: F1-F12=0x70-0x7B\n";
        file << "; Full list: https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes\n";
        file << "IncreaseKey=0x6B\n";
        file << "DecreaseKey=0x6D\n";
        file << "Set60Key=0x6A\n";
        file << "ResetKey=0x6F\n";
        file << "; InitialFov: The initial FOV value set when the program starts and when reset. Default is 0.\n";
        file << "; The game uses 0 to indicate that camera should use their default fov values.\n";
		file << "; e.g., 60 for open world sections, 65 for Cloud's Nibelheim section in Chapter 1.\n";
        file << "InitialFov=0\n"; // Added InitialFov entry
        file.close();
	}
    else
    {
        std::cerr << "Failed to create default config file FF7RebirthFovMod.ini!" << std::endl;
    }
}

HotkeyConfig LoadConfig()
{
    HotkeyConfig config = {
        DEFAULT_INCREASE_KEY,
        DEFAULT_DECREASE_KEY,
        DEFAULT_SET60_KEY,
        DEFAULT_RESET_KEY,
        0.0f // Default initialFov
    };

    const char* iniFilename = "FF7RebirthFovMod.ini";

    if (GetFileAttributesA( iniFilename ) == INVALID_FILE_ATTRIBUTES)
    {
        std::cout << "\nConfig file not found in current directory." << std::endl;
        std::cout << "Creating default config file FF7RebirthFovMod.ini in current directory." << std::endl;
        CreateDefaultConfig();
        return config;
    }
    else
    {
        std::cout << "\nConfig file found in current directory. Loading config from FF7RebirthFovMod.ini" << std::endl;
    }

    std::ifstream file( iniFilename );
    if (!file.is_open()) return config;

    std::string line;
    std::map<std::string, DWORD*> keyMap = {
        {"increasekey", &config.increaseKey},
        {"decreasekey", &config.decreaseKey},
        {"set60key", &config.set60Key},
        {"resetkey", &config.resetKey}
    };

    while (std::getline( file, line ))
    {
        size_t commentPos = line.find( ';' );
        if (commentPos != std::string::npos) line = line.substr( 0, commentPos );

        size_t equalsPos = line.find( '=' );
        if (equalsPos == std::string::npos) continue;

        std::string key = line.substr( 0, equalsPos );
        std::string value = line.substr( equalsPos + 1 );

        key.erase( remove_if( key.begin(), key.end(), isspace ), key.end() );
        value.erase( remove_if( value.begin(), value.end(), isspace ), value.end() );
        std::transform( key.begin(), key.end(), key.begin(), ::tolower );

        if (key.empty() || value.empty()) continue;

        // Parse key bindings (hex)
        try
        {
            DWORD parsedValue = std::stoul( value, nullptr, 16 );
            auto it = keyMap.find( key );
            if (it != keyMap.end())
            {
                *(it->second) = parsedValue;
            }
        }
        catch (...)
        {
            // Invalid value, keep default
        }

        // Parse InitialFov (float)
        if (key == "initialfov")
        {
            try
            {
                float fovValue = std::stof( value );
                config.initialFov = fovValue;
            }
            catch (...)
            {
                // Invalid value, keep default
            }
        }
    }

    return config;
}

std::string GetKeyName( DWORD vkCode )
{
    // List of keys that require the extended key flag
    const std::vector<DWORD> extendedKeys = {
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
        VK_PRIOR, VK_NEXT, // Page Up, Page Down
        VK_HOME, VK_END,
        VK_INSERT, VK_DELETE,
        VK_NUMLOCK, VK_RCONTROL, VK_RMENU, // Right Alt
        VK_RWIN, VK_LWIN,
        VK_DIVIDE // Numpad /
    };

    UINT scanCode = MapVirtualKeyA( vkCode, MAPVK_VK_TO_VSC );

    // Set extended bit if needed
    bool isExtended = std::find( extendedKeys.begin(),
        extendedKeys.end(),
        vkCode ) != extendedKeys.end();

    // Create lParam format for GetKeyNameText
    LPARAM lParam = (LPARAM)(scanCode << 16);
    if (isExtended)
    {
        lParam |= 0x01000000; // Set extended bit
    }

    char keyName[256];
    if (GetKeyNameTextA( (LONG)lParam, keyName, sizeof( keyName ) ) != 0)
    {
        return keyName;
    }
    return "Unknown";
}

uintptr_t GetModuleBaseAddress( DWORD pid, const wchar_t* modName )
{
    uintptr_t baseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid );
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32 modEntry;
        modEntry.dwSize = sizeof( modEntry );
        if (Module32First( hSnap, &modEntry ))
        {
            do
            {
                if (_wcsicmp( modEntry.szModule, modName ) == 0)
                {
                    baseAddr = (uintptr_t)modEntry.modBaseAddr;
                    break;
                }
            } while (Module32Next( hSnap, &modEntry ));
        }
        CloseHandle( hSnap );
    }
    return baseAddr;
}

uintptr_t ResolvePointerChainCombatCam( HANDLE hProcess, uintptr_t baseAddr )
{
    uintptr_t addr = baseAddr + 0x090A71E8; // Initial offset
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    addr += 0x50;
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    addr += 0x438;

    return addr;
}

uintptr_t ResolvePointerChainCityCam( HANDLE hProcess, uintptr_t baseAddr )
{
    uintptr_t addr = baseAddr + 0x092A0388; // Initial offset
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    // Follow the pointer chain: ->B8->40->470
    addr += 0xB8;
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    addr += 0x40;
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    addr += 0x340;

    return addr;
}

uintptr_t ResolvePointerChainOpenWorldCam( HANDLE hProcess, uintptr_t baseAddr )
{
    uintptr_t addr = baseAddr + 0x092A0388; // Initial offset
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    // Follow the pointer chain: ->B8->40->470
    addr += 0xB8;
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;
    
    addr += 0x40;
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;
    
    addr += 0x470;

    return addr;
}

uintptr_t ResolvePointerChainFOVCvar( HANDLE hProcess, uintptr_t baseAddr )
{
    uintptr_t addr = baseAddr + 0x09020218; // Initial offset
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    addr += 0x5F0;
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    addr += 0xE8;
    if (!ReadProcessMemory( hProcess, (LPCVOID)addr, &addr, sizeof( addr ), NULL )) return 0;

    addr += 0x244;

    return addr;
}

void HotkeyListener( HANDLE hProcess, uintptr_t fovAddrFOVCvar, std::atomic<bool>& running,
    const HotkeyConfig& config )
{
    const float step = 5.0f;
    float desiredFov = config.initialFov; // Start with initialFov from config
    bool keyStates[4] = { false };

    while (running)
    {
        bool currentKeys[] = {
            GetAsyncKeyState( config.increaseKey ) & 0x8000,
            GetAsyncKeyState( config.decreaseKey ) & 0x8000,
            GetAsyncKeyState( config.set60Key ) & 0x8000,
            GetAsyncKeyState( config.resetKey ) & 0x8000
        };

        if (currentKeys[0] && !keyStates[0])
        {
            desiredFov += step;
            std::cout << "FOV increased to: " << desiredFov << std::endl;
        }
        if (currentKeys[1] && !keyStates[1])
        {
            desiredFov -= step;
            std::cout << "FOV decreased to: " << desiredFov << std::endl;
        }
        if (currentKeys[2] && !keyStates[2])
        {
            desiredFov = 60.0f;
            std::cout << "FOV set to 60" << std::endl;
        }
        if (currentKeys[3] && !keyStates[3])
        {
            desiredFov = config.initialFov; // Reset to initialFov
            std::cout << "FOV reset to initial value: " << config.initialFov << std::endl;
        }

        memcpy( keyStates, currentKeys, sizeof( keyStates ) );
        WriteProcessMemory( hProcess, (LPVOID)fovAddrFOVCvar, &desiredFov, sizeof( desiredFov ), NULL );
        Sleep( 50 );
    }
}

int main()
{
    DWORD pid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

    // Find process ID
    PROCESSENTRY32 procEntry;
    procEntry.dwSize = sizeof( procEntry );
    bool found = false;

    if (Process32First( hSnap, &procEntry ))
    {
        do
        {
            if (_wcsicmp( procEntry.szExeFile, L"ff7rebirth_.exe" ) == 0)
            {
                pid = procEntry.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32Next( hSnap, &procEntry ));
    }
    CloseHandle( hSnap );

    if (!found)
    {
        std::cerr << "ff7rebirth_.exe not found!" << std::endl;
        std::cerr << "Exiting in 5 seconds..." << std::endl;
        Sleep( 5000 );
        return 1;
    }
    std::cout << "Found ff7rebirth_.exe with PID: " << pid << std::endl;

    // Get process handle with additional access flags
    HANDLE hProcess = OpenProcess( PROCESS_VM_READ | PROCESS_VM_WRITE |
        PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
        FALSE, pid );
    if (!hProcess)
    {
        std::cerr << "Failed to open ff7rebirth_.exe! Error: " << GetLastError() << std::endl;
        std::cerr << "Exiting in 5 seconds..." << std::endl;
        Sleep( 5000 );
        return 1;
    }
    std::cout << "Successfully opened ff7rebirth_.exe game process." << std::endl;

    // Get base address
    uintptr_t moduleBase = GetModuleBaseAddress( pid, L"ff7rebirth_.exe" );
    if (!moduleBase)
    {
        std::cerr << "Failed to find module base address of ff7rebirth_.exe!" << std::endl;
        std::cerr << "Exiting in 5 seconds..." << std::endl;
        Sleep( 5000 );
        CloseHandle( hProcess );
        return 1;
    }
    std::cout << "Found module base address: 0x" << std::hex << moduleBase << std::dec << std::endl;

    // Resolve pointer chain
    //uintptr_t fovAddrOpenWorldCam = ResolvePointerChainOpenWorldCam( hProcess, moduleBase );
    //uintptr_t fovAddrCityCam = ResolvePointerChainCityCam( hProcess, moduleBase );
    //uintptr_t fovAddrCombatCam = ResolvePointerChainCombatCam( hProcess, moduleBase );
    uintptr_t fovAddrFOVCvar = ResolvePointerChainFOVCvar( hProcess, moduleBase );
    if (!fovAddrFOVCvar)
    {
        std::cerr << "Failed to resolve pointer chain!" << std::endl;
        std::cerr << "Exiting in 5 seconds..." << std::endl;
        Sleep( 5000 );
        CloseHandle( hProcess );
        return 1;
    }
    //std::cout << "Resolved FOV address for Open World (?) Cam: 0x" << std::hex << fovAddrOpenWorldCam << std::dec << std::endl;
    //std::cout << "Resolved FOV address for City Cam (?): 0x" << std::hex << fovAddrCityCam << std::dec << std::endl;
    //std::cout << "Resolved FOV address for Combat Cam (?): 0x" << std::hex << fovAddrCombatCam << std::dec << std::endl;
    std::cout << "Resolved FOV address for FOV Cvar: 0x" << std::hex << fovAddrFOVCvar << std::dec << std::endl;
    
    //float currentOpenWorldFov;
    //float currentCityFov;
    //float currentCombatFov;
    float currentFOVCvar;
    HotkeyConfig config;
    if (
  //      ReadProcessMemory( hProcess, (LPCVOID)(fovAddrOpenWorldCam), &currentOpenWorldFov, sizeof( currentOpenWorldFov ), NULL ) &&
        //ReadProcessMemory( hProcess, (LPCVOID)(fovAddrCityCam), &currentCityFov, sizeof( currentCityFov ), NULL )&&
        //ReadProcessMemory( hProcess, (LPCVOID)(fovAddrCombatCam), &currentCombatFov, sizeof( currentCombatFov ), NULL ) &&
        ReadProcessMemory( hProcess, (LPCVOID)(fovAddrFOVCvar), &currentFOVCvar, sizeof( currentFOVCvar ), NULL )
        )
    {
        //std::cout << "Initial Open World (?) FOV: " << currentOpenWorldFov << std::endl;
        //std::cout << "Initial City (?) FOV: " << currentCityFov << std::endl;
        //std::cout << "Initial Combat (?) FOV: " << currentCombatFov << std::endl;
        std::cout << "Initial FOV Cvar: " << currentFOVCvar << std::endl;
        config = LoadConfig();
		if (config.initialFov != 0.0f)
		{
            std::cout << "Reading Config and setting Initial FOV set to " << config.initialFov << std::endl;
		}
        std::cout << "\n=== FOV Control Active ===" << std::endl;
        std::cout << "Monitoring game status automatically." << std::endl;
        // Display active key bindings
        std::cout << "Active Key Bindings:\n"
            << "Increase FOV: " << GetKeyName( config.increaseKey ) << " (0x"
            << std::hex << config.increaseKey << ")\n"
            << "Decrease FOV: " << GetKeyName( config.decreaseKey ) << " (0x"
            << config.decreaseKey << ")\n"
            << "Set to 60:    " << GetKeyName( config.set60Key ) << " (0x"
            << config.set60Key << ")\n"
            << "Reset FOV to 0 (Default):    " << GetKeyName( config.resetKey ) << " (0x"
            << config.resetKey << ")\n" << std::dec;
        std::cout << "The game uses 0 to indicate that the default FOV should be used." << std::endl;
        std::cout << "Some sections use preset FOVs, which will be preserved if the FOV CVar is set to 0. " << std::endl;
        std::cout << "One such example is Cloud's Nibelheim section in Chapter 1 where 65 is used." << std::endl;

        //std::cout << "ESC      : Exit program" << std::endl;
        std::cout << "\nInput listening started..." << std::endl;
        std::cout << "You can safely close this console window at any time to stop\n";
    }
    else
    {
        std::cerr << "Failed to read initial FOV! Could not read process memory?" << std::endl;
        std::cerr << "Exiting in 5 seconds..." << std::endl;
        CloseHandle( hProcess );
        Sleep( 5000 );
        return 1;
    }

    // Set console close handler
    SetConsoleCtrlHandler( ConsoleHandler, TRUE );

    // Start hotkey listener
    std::atomic<bool> running( true );
    std::thread hotkeyThread( HotkeyListener, hProcess, fovAddrFOVCvar, std::ref( running ) , config);

    // Main monitoring loop
    int safetyCounter = 0;
    while (running)
    {
        // Check game status every 1 second (10 * 100ms)
        if (++safetyCounter >= 10)
        {
            if (!IsProcessRunning( hProcess ))
            {
                std::cout << "\nGame process terminated. Closing controller..." << std::endl;
                running = false;
                break;
            }
            safetyCounter = 0;
        }

        //// Check for ESC key
        //if (GetAsyncKeyState( VK_ESCAPE ) & 0x8000)
        //{
        //    running = false;
        //}

        Sleep( 100 );
    }

    // Cleanup
    running = false;
    hotkeyThread.join();

    if (hProcess)
    {
        CloseHandle( hProcess );
        std::cout << "Cleanly released game resources." << std::endl;
    }

    std::cout << "\nController shutdown complete. Safe to close." << std::endl;
    std::cerr << "Exiting in 5 seconds..." << std::endl;
    Sleep( 5000 );

    return 0;
}