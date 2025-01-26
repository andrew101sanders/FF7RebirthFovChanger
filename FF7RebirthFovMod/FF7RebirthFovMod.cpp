#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <atomic>
#include <thread>

std::atomic<bool> running;

BOOL WINAPI ConsoleHandler( DWORD signal )
{
    if (signal == CTRL_CLOSE_EVENT)
    {
        // Let the main thread know to exit gracefully
        running = false;
        return TRUE;
    }
    return FALSE;
}

bool IsProcessRunning( HANDLE hProcess )
{
    DWORD exitCode;
    if (!GetExitCodeProcess( hProcess, &exitCode ))
    {
        return false; // Process is definitely gone if we can't get status
    }
    return (exitCode == STILL_ACTIVE);
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

void HotkeyListener( HANDLE hProcess, uintptr_t fovAddrOpenWorld, uintptr_t fovAddrCity, uintptr_t fovAddrCombat, std::atomic<bool>& running )
{
    const float step = 5.0f;
    const float defaultFov = 60.0f;
    float desiredFov = defaultFov;
    bool plusPressed = false;
    bool minusPressed = false;
    bool asteriskPressed = false;

    while (running)
    {
        // Check for keys
        bool currentPlus = GetAsyncKeyState( VK_ADD ) & 0x8000;
        bool currentMinus = GetAsyncKeyState( VK_SUBTRACT ) & 0x8000;
        bool currentAsterisk = GetAsyncKeyState( VK_MULTIPLY ) & 0x8000;

        // Handle Numpad+ (increase FOV)
        if (currentPlus && !plusPressed)
        {
            desiredFov += step;
            std::cout << "FOV increased to: " << desiredFov << std::endl;
        }

        // Handle Numpad- (decrease FOV)
        if (currentMinus && !minusPressed)
        {
            desiredFov -= step;
            std::cout << "FOV decreased to: " << desiredFov << std::endl;
        }

        // Handle Numpad* (reset to default)
        if (currentAsterisk && !asteriskPressed)
        {
            desiredFov = defaultFov;
            std::cout << "FOV reset to default: " << desiredFov << std::endl;
        }

        // Continuously write the desired FOV to all addresses
        WriteProcessMemory( hProcess, (LPVOID)fovAddrOpenWorld, &desiredFov, sizeof( desiredFov ), NULL );
        WriteProcessMemory( hProcess, (LPVOID)fovAddrCity, &desiredFov, sizeof( desiredFov ), NULL );
        WriteProcessMemory( hProcess, (LPVOID)fovAddrCombat, &desiredFov, sizeof( desiredFov ), NULL );

        // Update key states for edge detection
        plusPressed = currentPlus;
        minusPressed = currentMinus;
        asteriskPressed = currentAsterisk;

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
	uintptr_t fovAddrOpenWorldCam = ResolvePointerChainOpenWorldCam( hProcess, moduleBase );
	uintptr_t fovAddrCityCam = ResolvePointerChainCityCam( hProcess, moduleBase );
	uintptr_t fovAddrCombatCam = ResolvePointerChainCombatCam( hProcess, moduleBase );
    if (!fovAddrOpenWorldCam || !fovAddrCityCam || !fovAddrCombatCam)
    {
        std::cerr << "Failed to resolve pointer chain!" << std::endl;
        std::cerr << "Exiting in 5 seconds..." << std::endl;
        Sleep( 5000 );
        CloseHandle( hProcess );
        return 1;
    }
	std::cout << "Resolved FOV address for Open World (?) Cam: 0x" << std::hex << fovAddrOpenWorldCam << std::dec << std::endl;
	std::cout << "Resolved FOV address for City Cam (?): 0x" << std::hex << fovAddrCityCam << std::dec << std::endl;
	std::cout << "Resolved FOV address for Combat Cam (?): 0x" << std::hex << fovAddrCombatCam << std::dec << std::endl;

    float currentOpenWorldFov;
    float currentCityFov;
    float currentCombatFov;
    if (ReadProcessMemory( hProcess, (LPCVOID)(fovAddrOpenWorldCam), &currentOpenWorldFov, sizeof( currentOpenWorldFov ), NULL ) &&
		ReadProcessMemory( hProcess, (LPCVOID)(fovAddrCityCam), &currentCityFov, sizeof( currentCityFov ), NULL )&&
		ReadProcessMemory( hProcess, (LPCVOID)(fovAddrCombatCam), &currentCombatFov, sizeof( currentCombatFov ), NULL ))
    {
        std::cout << "Initial Open World (?) FOV: " << currentOpenWorldFov << std::endl;
		std::cout << "Initial City (?) FOV: " << currentCityFov << std::endl;
		std::cout << "Initial Combat (?) FOV: " << currentCombatFov << std::endl;
        std::cout << "\n=== FOV Control Active ===" << std::endl;
        std::cout << "Monitoring game status automatically." << std::endl;
        std::cout << "Numpad + : Increase FOV by 5" << std::endl;
        std::cout << "Numpad - : Decrease FOV by 5" << std::endl;
        std::cout << "Numpad * : Reset to 60" << std::endl;
        //std::cout << "ESC      : Exit program" << std::endl;
        std::cout << "\nInput listening started..." << std::endl;
        std::cout << "You can safely close this console window at any time to stop\n";
	}
	else
	{
		std::cerr << "Failed to read initial FOV!" << std::endl;
		std::cerr << "Exiting in 5 seconds..." << std::endl;
		CloseHandle( hProcess );
		Sleep( 5000 );
		return 1;
	}

    // Set console close handler
    SetConsoleCtrlHandler( ConsoleHandler, TRUE );

    // Start hotkey listener
    std::atomic<bool> running( true );
    std::thread hotkeyThread( HotkeyListener, hProcess, fovAddrOpenWorldCam, fovAddrCityCam, fovAddrCombatCam, std::ref( running ) );

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