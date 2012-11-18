
#include <Windows.h>

static const BYTE s_7byteNop[] = { 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00 };

LPCTSTR DoWork(void);
LPCTSTR PatchLocation(HANDLE hProcess, LPVOID lpcvLocation, LPCVOID lpvData, DWORD dwSize);
DWORD HandleError(LPCTSTR errorMessage);

extern "C" int CDECL main(void)
{
    LPCTSTR lpErrorMessage;

    lpErrorMessage = DoWork();

    DWORD dwResult = 0;

    if (lpErrorMessage)
        dwResult = HandleError(lpErrorMessage);

    ExitProcess(dwResult);
    return dwResult;
}

LPCTSTR DoWork(void)
{
    HWND hDesktopWindow = GetDesktopWindow();

    // Get screen resolution
    RECT rcWindowRect;
    BOOL bResult = GetWindowRect(hDesktopWindow, &rcWindowRect);
    if (!bResult)
        return TEXT("GetWindowRect failed!");

    DWORD dwScreenWidth = rcWindowRect.right;
    DWORD dwScreenHeight = rcWindowRect.bottom;

    // Start speed.exe
    int result = (int)ShellExecute(hDesktopWindow, TEXT("open"), TEXT("speed.exe"), NULL, NULL, SW_SHOWNORMAL);
    if (result < 32)
        return TEXT("ShellExecute failed!");

    DWORD const dwMaxSleepTime = 3000;
    DWORD const dwMaxRetries = 10;
    DWORD dwRetry = 0;

    HWND hWindow;
    do
    {
        // Sleep some time to allow speed.exe create the window
        if (dwRetry)
            Sleep(dwMaxSleepTime / dwMaxRetries);

        // Find the window
        hWindow = FindWindow(TEXT("GameFrame"), NULL);
    }
    while (!hWindow && ++dwRetry < dwMaxRetries);

    if (!hWindow)
        return TEXT("FindWindow failed!");

    DWORD dwProcessId = 0;

    // Retrieve the process id of speed.exe
    GetWindowThreadProcessId(hWindow, &dwProcessId);

    if (!dwProcessId)
        return TEXT("GetWindowThreadProcessId failed!");

    DWORD dwDesiredAccess =
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_READ | PROCESS_VM_WRITE |
        PROCESS_VM_OPERATION;

    // Open speed.exe process
    HANDLE hProcess = OpenProcess(dwDesiredAccess, FALSE, dwProcessId);
    if (!hProcess)
        return TEXT("OpenProcess failed!");

    LPVOID lpPatchLocation;
    LPCVOID lpPatchData;
    DWORD dwPatchSize;
    LPCTSTR lpPatchResult;

    // Patch the switch jump
    lpPatchLocation = (LPVOID)0x6C27DE;
    lpPatchData = s_7byteNop;
    dwPatchSize = sizeof(s_7byteNop);
    lpPatchResult = PatchLocation(hProcess, lpPatchLocation, lpPatchData, dwPatchSize);
    if (lpPatchResult)
        return lpPatchResult;

    // Patch width
    lpPatchLocation = (LPVOID)0x6C27EF;
    lpPatchData = &dwScreenWidth;
    dwPatchSize = sizeof(dwScreenWidth);
    lpPatchResult = PatchLocation(hProcess, lpPatchLocation, lpPatchData, dwPatchSize);
    if (lpPatchResult)
        return lpPatchResult;

    // Patch height
    lpPatchLocation = (LPVOID)0x6C27F5;
    lpPatchData = &dwScreenHeight;
    dwPatchSize = sizeof(dwScreenHeight);
    lpPatchResult = PatchLocation(hProcess, lpPatchLocation, lpPatchData, dwPatchSize);
    if (lpPatchResult)
        return lpPatchResult;

    DWORD dwClientWidth;
    do
    {
        // Patch loaded flag
        BYTE bFlag = 1;
        lpPatchLocation = (LPVOID)0x982C39;
        lpPatchData = &bFlag;
        dwPatchSize = sizeof(bFlag);
        lpPatchResult = PatchLocation(hProcess, lpPatchLocation, lpPatchData, dwPatchSize);
        if (lpPatchResult)
            return lpPatchResult;

        lpPatchLocation = (LPVOID)0x982BE4;
        bResult = ReadProcessMemory(hProcess, lpPatchLocation, &dwClientWidth, sizeof(DWORD), NULL);
        if (!bResult)
            break;

        Sleep(500);
    }
    while (dwClientWidth != dwScreenWidth);

    CloseHandle(hProcess);

    // All done, hooray
    return NULL;
}

LPCTSTR PatchLocation(HANDLE hProcess, LPVOID lpLocation, LPCVOID lpData, DWORD dwSize)
{
    BOOL bResult;

    DWORD dwProtect = PAGE_EXECUTE_READWRITE;
    bResult = VirtualProtectEx(hProcess, lpLocation, dwSize, dwProtect, &dwProtect);
    if (!bResult)
        return TEXT("VirtualProtectEx (1) failed!");

    bResult = WriteProcessMemory(hProcess, lpLocation, lpData, dwSize, NULL);
    if (!bResult)
        return TEXT("WriteProcessMemory");

    bResult = VirtualProtectEx(hProcess, lpLocation, dwSize, dwProtect, &dwProtect);
    if (!bResult)
        return TEXT("VirtualProtectEx (2) failed!");

    return NULL;
}

DWORD HandleError(LPCTSTR errorMessage)
{
    DWORD error = GetLastError();

    TCHAR message[512];

    wsprintf(message, TEXT("Error Code: %d\nError Message: %s"), error, errorMessage);

    MessageBox(GetDesktopWindow(), message, TEXT("Error"), MB_OK | MB_ICONERROR);

    return error;
}
