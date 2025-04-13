#include "../include/GuiControls.h"
#include <windowsx.h>
#include <shlobj.h>
#include <strsafe.h>
#include <map>

// Constructor
MainWindow::MainWindow(HINSTANCE hInstance)
    : m_hInstance(hInstance),
    m_hwnd(nullptr),
    m_sourceListView(nullptr),
    m_destinationEdit(nullptr),
    m_progressBar(nullptr),
    m_statusBar(nullptr),
    m_packetSizeCombo(nullptr)
{
}

// Destructor
MainWindow::~MainWindow()
{
    // Cleanup if needed
}

// Initialize the main window
bool MainWindow::Initialize(int nCmdShow)
{
    // Register the window class
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = m_hInstance;
    wcex.hIcon = LoadIcon(m_hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(nullptr, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Calculate centered window position
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 800;
    int windowHeight = 700;
    int windowX = (screenWidth - windowWidth) / 2;
    int windowY = (screenHeight - windowHeight) / 2;

    // Create the main window
    m_hwnd = CreateWindow(
        WINDOW_CLASS_NAME,
        L"Multi-Source File Copy Utility",
        WS_OVERLAPPEDWINDOW,
        windowX, windowY, windowWidth, windowHeight,
        nullptr, nullptr, m_hInstance, this);  // Pass 'this' pointer for use in WindowProc

    if (!m_hwnd)
    {
        MessageBox(nullptr, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Create all the UI controls
    CreateControls(m_hwnd);

    // Show and update the window
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    return true;
}

// Run the message loop
int MainWindow::Run()
{
    MSG msg = { 0 };
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Static window procedure
LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* pThis = nullptr;

    // On window creation, store the MainWindow instance pointer
    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (MainWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        // Retrieve the MainWindow instance pointer
        pThis = (MainWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    // Call the member function to handle the message
    if (pThis)
    {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    else
    {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// Member window procedure implementation
LRESULT MainWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_ADD_SOURCE_BUTTON:
            OnAddSource();
            break;

        case ID_ADD_FOLDER_BUTTON:
            OnAddFolder();
            break;

        case ID_REMOVE_BUTTON:
            OnRemoveSource();
            break;

        case ID_BROWSE_BUTTON:
            OnBrowseDestination();
            break;

        case ID_MEASURE_BUTTON:
            OnMeasureSpeeds();
            break;

        case ID_START_BUTTON:
            OnStartCopy();
            break;

        case ID_CANCEL_BUTTON:
            OnCancelOperation();
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }
    break;

    case WM_SIZE:
        // Resize all controls when the window is resized
        ResizeControls(hwnd);
        break;

    case WM_UPDATE_PROGRESS:
        // Update progress from worker thread
        SetProgress((int)wParam);
        break;

    case WM_COPY_COMPLETE:
        // Copy operation completed
        OnCopyComplete((bool)wParam);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Create all the UI controls
void MainWindow::CreateControls(HWND hwnd)
{
    // Create group box for sources
    CreateWindow(
        L"BUTTON", L"Source Files",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 10, 760, 320,
        hwnd, nullptr, m_hInstance, nullptr);

    // Create source list view with columns
    m_sourceListView = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        20, 30, 740, 250,
        hwnd, (HMENU)ID_SOURCE_LISTVIEW, m_hInstance, nullptr);

    // Set extended list view styles
    ListView_SetExtendedListViewStyle(m_sourceListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Add columns to the list view
    LVCOLUMN lvc = { 0 };
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    // Path column
    lvc.iSubItem = 0;
    lvc.cx = 440;
    lvc.pszText = L"Source Path";
    ListView_InsertColumn(m_sourceListView, 0, &lvc);

    // Status column
    lvc.iSubItem = 1;
    lvc.cx = 100;
    lvc.pszText = L"Status";
    ListView_InsertColumn(m_sourceListView, 1, &lvc);

    // Speed column
    lvc.iSubItem = 2;
    lvc.cx = 120;
    lvc.pszText = L"Speed (Mbps)";
    ListView_InsertColumn(m_sourceListView, 2, &lvc);

    // Create buttons for source actions
    CreateWindow(
        L"BUTTON", L"Add File",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 290, 120, 30,
        hwnd, (HMENU)ID_ADD_SOURCE_BUTTON, m_hInstance, nullptr);

    CreateWindow(
        L"BUTTON", L"Add Folder",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 290, 120, 30,
        hwnd, (HMENU)ID_ADD_FOLDER_BUTTON, m_hInstance, nullptr);

    CreateWindow(
        L"BUTTON", L"Remove",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        280, 290, 120, 30,
        hwnd, (HMENU)ID_REMOVE_BUTTON, m_hInstance, nullptr);

    CreateWindow(
        L"BUTTON", L"Measure Speeds",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        280, 290, 140, 30,
        hwnd, (HMENU)ID_MEASURE_BUTTON, m_hInstance, nullptr);

    // Create group box for destination
    CreateWindow(
        L"BUTTON", L"Destination",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 340, 760, 60,
        hwnd, nullptr, m_hInstance, nullptr);

    // Create destination controls
    CreateWindow(
        L"STATIC", L"Folder:",
        WS_CHILD | WS_VISIBLE,
        20, 365, 50, 20,
        hwnd, nullptr, m_hInstance, nullptr);

    m_destinationEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
        80, 365, 580, 25,
        hwnd, (HMENU)ID_DESTINATION_EDIT, m_hInstance, nullptr);

    CreateWindow(
        L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 365, 90, 25,
        hwnd, (HMENU)ID_BROWSE_BUTTON, m_hInstance, nullptr);

    // Create group box for settings
    CreateWindow(
        L"BUTTON", L"Settings",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 410, 760, 60,
        hwnd, nullptr, m_hInstance, nullptr);

    // Create packet size dropdown
    CreateWindow(
        L"STATIC", L"Packet Size:",
        WS_CHILD | WS_VISIBLE,
        20, 435, 80, 20,
        hwnd, nullptr, m_hInstance, nullptr);

    m_packetSizeCombo = CreateWindow(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        110, 435, 150, 200,
        hwnd, (HMENU)ID_PACKET_SIZE_COMBO, m_hInstance, nullptr);

    // Add packet size options
    ComboBox_AddString(m_packetSizeCombo, L"16 KB");
    ComboBox_AddString(m_packetSizeCombo, L"32 KB");
    ComboBox_AddString(m_packetSizeCombo, L"64 KB");
    ComboBox_AddString(m_packetSizeCombo, L"128 KB");
    ComboBox_AddString(m_packetSizeCombo, L"256 KB");
    ComboBox_AddString(m_packetSizeCombo, L"512 KB");
    ComboBox_AddString(m_packetSizeCombo, L"1 MB");
    ComboBox_SetCurSel(m_packetSizeCombo, 2);  // Default to 64 KB

    // Create progress bar group
    CreateWindow(
        L"BUTTON", L"Progress",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 480, 760, 40,
        hwnd, nullptr, m_hInstance, nullptr);

    // Create progress bar
    m_progressBar = CreateWindowEx(
        0, PROGRESS_CLASS, L"",
        WS_CHILD | WS_VISIBLE,
        20, 500, 740, 30,
        hwnd, (HMENU)ID_PROGRESS_BAR, m_hInstance, nullptr);

    // Initialize progress bar range (0-100)
    SendMessage(m_progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    // Create action buttons
    CreateWindow(
        L"BUTTON", L"Start Copy",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        280, 530, 120, 30,
        hwnd, (HMENU)ID_START_BUTTON, m_hInstance, nullptr);

    HWND hCancelButton = CreateWindow(
        L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        410, 530, 120, 30,
        hwnd, (HMENU)ID_CANCEL_BUTTON, m_hInstance, nullptr);

    // Status bar at the bottom
    m_statusBar = CreateWindowEx(
        0, STATUSCLASSNAME, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd, (HMENU)ID_STATUS_BAR, m_hInstance, nullptr);

    // Set initial status text
    UpdateStatusText(L"Ready");
}

// Resize controls when the window is resized
void MainWindow::ResizeControls(HWND hwnd)
{
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    // Resize and reposition groups
    SetWindowPos(GetDlgItem(hwnd, 0), nullptr, 10, 10, width - 20, 320, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, 0), nullptr, 10, 340, width - 20, 60, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, 0), nullptr, 10, 410, width - 20, 60, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, 0), nullptr, 10, 480, width - 20, 40, SWP_NOZORDER);

    // Resize list view
    SetWindowPos(m_sourceListView, nullptr, 20, 30, width - 40, 250, SWP_NOZORDER);

    // Update destination edit control width
    SetWindowPos(m_destinationEdit, nullptr, 80, 365, width - 200, 25, SWP_NOZORDER);

    // Update browse button position
    SetWindowPos(GetDlgItem(hwnd, ID_BROWSE_BUTTON), nullptr, width - 110, 365, 90, 25, SWP_NOZORDER);

    // Update progress bar width
    SetWindowPos(m_progressBar, nullptr, 20, 500, width - 40, 15, SWP_NOZORDER);

    // Update action buttons position
    SetWindowPos(GetDlgItem(hwnd, ID_START_BUTTON), nullptr, (width / 2) - 130, 530, 120, 30, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, ID_CANCEL_BUTTON), nullptr, (width / 2) + 10, 530, 120, 30, SWP_NOZORDER);

    // Position status bar
    SendMessage(m_statusBar, WM_SIZE, 0, 0);
}

// Update source list view
void MainWindow::UpdateSourceList()
{
    // Clear the list view
    ListView_DeleteAllItems(m_sourceListView);

    // Get sources from file copier
    const std::vector<SourceInfo>& sources = m_fileCopier.GetSources();

    // Add each source to the list view
    for (size_t i = 0; i < sources.size(); i++)
    {
        const SourceInfo& source = sources[i];

        // Add item with path
        LVITEM lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)source.path.c_str();

        int index = ListView_InsertItem(m_sourceListView, &lvi);

        // Set status text
        ListView_SetItemText(m_sourceListView, index, 1, (LPWSTR)source.status.c_str());

        // Set speed value (as text)
        if (source.speed > 0)
        {
            WCHAR speedText[32];
            StringCchPrintf(speedText, 32, L"%.2f", source.speed / 1000.0); // Convert to Mbps
            ListView_SetItemText(m_sourceListView, index, 2, speedText);
        }
        else
        {
            ListView_SetItemText(m_sourceListView, index, 2, L"-");
        }
    }
}

// Update status text
void MainWindow::UpdateStatusText(const wchar_t* text)
{
    SendMessage(m_statusBar, SB_SETTEXT, 0, (LPARAM)text);
}

// Set progress bar value
void MainWindow::SetProgress(int percent)
{
    SendMessage(m_progressBar, PBM_SETPOS, (WPARAM)percent, 0);
}

// Enable/disable copy controls
void MainWindow::EnableCopyControls(bool enable)
{
    EnableWindow(GetDlgItem(m_hwnd, ID_ADD_SOURCE_BUTTON), enable);
    EnableWindow(GetDlgItem(m_hwnd, ID_REMOVE_BUTTON), enable);
    EnableWindow(GetDlgItem(m_hwnd, ID_MEASURE_BUTTON), enable);
    EnableWindow(GetDlgItem(m_hwnd, ID_START_BUTTON), enable);
    EnableWindow(GetDlgItem(m_hwnd, ID_BROWSE_BUTTON), enable);
    EnableWindow(m_packetSizeCombo, enable);
    EnableWindow(GetDlgItem(m_hwnd, ID_CANCEL_BUTTON), !enable);
}

// Get selected packet size in bytes
int MainWindow::GetSelectedPacketSize()
{
    int index = ComboBox_GetCurSel(m_packetSizeCombo);
    if (index == CB_ERR)
        return 64 * 1024; // Default to 64 KB

    // Calculate packet size based on selection
    // 16KB * 2^index
    return 16 * 1024 * (1 << index);
}

// Add source button click handler
void MainWindow::OnAddSource()
{
    OPENFILENAME ofn = { 0 };
    WCHAR szFile[MAX_PATH * 100] = { 0 }; // Buffer for multiple files

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (GetOpenFileName(&ofn))
    {
        // Check if single or multiple files selected
        if (ofn.nFileOffset < lstrlen(szFile))
        {
            // Single file selected
            m_fileCopier.AddSource(szFile);
        }
        else
        {
            // Multiple files selected
            WCHAR directory[MAX_PATH] = { 0 };
            StringCchCopy(directory, MAX_PATH, szFile);

            WCHAR* p = szFile + ofn.nFileOffset;
            while (*p)
            {
                WCHAR fullPath[MAX_PATH];
                StringCchPrintf(fullPath, MAX_PATH, L"%s\\%s", directory, p);

                m_fileCopier.AddSource(fullPath);

                // Move to next file name
                p += lstrlen(p) + 1;
            }
        }

        UpdateSourceList();
        UpdateStatusText(L"Source files added");
    }
}

// Add folder button click handler
void MainWindow::OnAddFolder()
{
    BROWSEINFO bi = { 0 };
    bi.hwndOwner = m_hwnd;
    bi.lpszTitle = L"Select source folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl)
    {
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path))
        {
            UpdateStatusText(L"Adding files from folder...");

            // Add a message processing step to keep UI responsive
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            // Add all files from the directory recursively
            int filesAdded = m_fileCopier.AddSourceDirectory(path);

            // Update the list view
            UpdateSourceList();

            // Update status
            WCHAR statusText[128];
            StringCchPrintf(statusText, 128, L"Added %d files from folder", filesAdded);
            UpdateStatusText(statusText);
        }

        CoTaskMemFree(pidl);
    }
}

// Remove source button click handler
void MainWindow::OnRemoveSource()
{
    // Get selected item
    int selectedIndex = ListView_GetNextItem(m_sourceListView, -1, LVNI_SELECTED);
    if (selectedIndex >= 0)
    {
        m_fileCopier.RemoveSource(selectedIndex);
        UpdateSourceList();
        UpdateStatusText(L"Source file removed");
    }
}

// Browse destination button click handler
void MainWindow::OnBrowseDestination()
{
    BROWSEINFO bi = { 0 };
    bi.hwndOwner = m_hwnd;
    bi.lpszTitle = L"Select destination folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl)
    {
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path))
        {
            SetWindowText(m_destinationEdit, path);
            UpdateStatusText(L"Destination folder selected");
        }

        CoTaskMemFree(pidl);
    }
}

// Measure speeds button click handler
void MainWindow::OnMeasureSpeeds()
{
    const std::vector<SourceInfo>& sources = m_fileCopier.GetSources();
    if (sources.empty())
    {
        MessageBox(m_hwnd, L"Please add at least one source file.", L"No Sources", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Disable controls during measurement
    EnableCopyControls(false);
    UpdateStatusText(L"Measuring source speeds...");

    // Create a list of paths to measure
    std::vector<std::wstring> paths;
    std::map<std::wstring, long long> speedMap;

    for (const auto& source : sources)
    {
        paths.push_back(source.path);
    }

    // Measure and sort the sources
    if (m_speedMeasure.MeasureAndSortSources(paths))
    {
        // Get the speeds for each path
        for (const auto& path : paths)
        {
            long long speed = m_speedMeasure.MeasureSourceSpeed(path);
            speedMap[path] = speed;
        }

        // Clear existing sources and add them back in sorted order with speeds
        m_fileCopier.ClearSources();
        for (const auto& path : paths)
        {
            // Add source with speed information
            SourceInfo info;
            info.path = path;
            info.status = L"Ready";
            info.speed = speedMap[path];

            // Add directly to the FileCopier's sources
            m_fileCopier.AddSourceWithInfo(info);
        }

        UpdateSourceList();
        UpdateStatusText(L"Sources measured and sorted by speed");
    }
    else
    {
        UpdateStatusText(L"Error measuring source speeds");
    }

    // Re-enable controls
    EnableCopyControls(true);
}

// Start copy button click handler
void MainWindow::OnStartCopy()
{
    const std::vector<SourceInfo>& sources = m_fileCopier.GetSources();
    if (sources.empty())
    {
        MessageBox(m_hwnd, L"Please add at least one source file.", L"No Sources", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Get destination folder
    WCHAR destinationPath[MAX_PATH];
    GetWindowText(m_destinationEdit, destinationPath, MAX_PATH);

    if (lstrlen(destinationPath) == 0)
    {
        MessageBox(m_hwnd, L"Please select a destination folder.", L"No Destination", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Disable controls during copy
    EnableCopyControls(false);

    // Reset progress bar
    SetProgress(0);
    UpdateStatusText(L"Starting copy operation...");

    // Get packet size from combo box
    int packetSize = GetSelectedPacketSize();

    // Start the copy operation
    if (!m_fileCopier.StartCopy(destinationPath, ProgressCallback, this, packetSize))
    {
        MessageBox(m_hwnd, L"Failed to start copy operation.", L"Error", MB_OK | MB_ICONERROR);
        // Re-enable controls
        EnableCopyControls(true);
    }
}

// Cancel button click handler
void MainWindow::OnCancelOperation()
{
    m_fileCopier.Cancel();
    UpdateStatusText(L"Cancelling operation...");
}

// Copy complete handler
void MainWindow::OnCopyComplete(bool success)
{
    if (success)
    {
        UpdateStatusText(L"Copy operation completed successfully.");
    }
    else
    {
        UpdateStatusText(L"Copy operation cancelled or failed.");
    }

    // Re-enable controls
    EnableCopyControls(true);
}

// Progress callback function (static)
void MainWindow::ProgressCallback(int completed, int total, void* userData)
{
    MainWindow* pThis = static_cast<MainWindow*>(userData);
    if (pThis)
    {
        // Calculate percentage
        int percent = (total > 0) ? (completed * 100) / total : 0;

        // Update UI (thread-safe using messages)
        PostMessage(pThis->m_hwnd, WM_UPDATE_PROGRESS, (WPARAM)percent, 0);

        // Update status text
        WCHAR statusText[128];
        StringCchPrintf(statusText, 128, L"Copying: %d of %d packets (%d%%)", completed, total, percent);
        SendMessage(pThis->m_statusBar, SB_SETTEXT, 0, (LPARAM)statusText);

        // If completed, send completion message
        if (completed >= total)
        {
            PostMessage(pThis->m_hwnd, WM_COPY_COMPLETE, (WPARAM)true, 0);
        }
    }
}