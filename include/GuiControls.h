#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "../include/FileCopier.h"
#include "../include/SpeedMeasure.h"

// Control IDs
#define ID_SOURCE_LISTVIEW     1001
#define ID_DESTINATION_EDIT    1002
#define ID_BROWSE_BUTTON       1003
#define ID_ADD_SOURCE_BUTTON   1004
#define ID_ADD_FOLDER_BUTTON   1012
#define ID_REMOVE_BUTTON       1005
#define ID_MEASURE_BUTTON      1006
#define ID_START_BUTTON        1007
#define ID_CANCEL_BUTTON       1008
#define ID_PROGRESS_BAR        1009
#define ID_STATUS_BAR          1010
#define ID_PACKET_SIZE_COMBO   1011

// Window class name
#define WINDOW_CLASS_NAME L"MultiSourceFileCopierClass"

// Message for updating the progress bar (to avoid cross-thread UI updates)
#define WM_UPDATE_PROGRESS (WM_USER + 1)
#define WM_COPY_COMPLETE   (WM_USER + 2)

// Main application window class
class MainWindow {
public:
    MainWindow(HINSTANCE hInstance);
    ~MainWindow();

    // Initialize the window
    bool Initialize(int nCmdShow);

    // Run message loop
    int Run();

private:
    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // UI functions
    void CreateControls(HWND hwnd);
    void ResizeControls(HWND hwnd);
    void UpdateSourceList();
    void UpdateStatusText(const wchar_t* text);
    void SetProgress(int percent);
    void EnableCopyControls(bool enable);

    // Action handlers
    void OnAddSource();
    void OnAddFolder();
    void OnRemoveSource();
    void OnBrowseDestination();
    void OnMeasureSpeeds();
    void OnStartCopy();
    void OnCancelOperation();
    void OnCopyComplete(bool success);

    // Progress callback
    static void ProgressCallback(int completed, int total, void* userData);

    // Member variables
    HWND m_hwnd;                // Main window handle
    HWND m_sourceListView;      // List view control for sources
    HWND m_destinationEdit;     // Edit control for destination
    HWND m_progressBar;         // Progress bar
    HWND m_statusBar;           // Status bar
    HWND m_packetSizeCombo;     // Packet size combo box
    HINSTANCE m_hInstance;      // Application instance

    FileCopier m_fileCopier;    // File copier instance
    SpeedMeasure m_speedMeasure; // Speed measurement

    // Helper methods
    int GetSelectedPacketSize();
};