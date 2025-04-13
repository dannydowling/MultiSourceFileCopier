#include <windows.h>
#include <commctrl.h>
#include "../include/GuiControls.h"

// Link with necessary libraries
#pragma comment(lib, "comdlg32.lib")  // For GetOpenFileName
#pragma comment(lib, "shell32.lib")   // For SHBrowseForFolder and SHGetPathFromIDList

// Initialize Common Controls
void InitializeCommonControls()
{
    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES |
        ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);
}

// Entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Initialize COM for shell functions
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
    {
        MessageBox(NULL, L"COM initialization failed!", L"Error", MB_ICONERROR);
        return 1;
    }

    // Initialize common controls
    InitializeCommonControls();

    // Create and initialize the main window
    MainWindow mainWindow(hInstance);
    if (!mainWindow.Initialize(nCmdShow))
    {
        CoUninitialize();
        return 1;
    }

    // Run the message loop
    int result = mainWindow.Run();

    // Clean up COM
    CoUninitialize();

    return result;
}