#include "../include/FileCopier.h"
#include <shlwapi.h>
#include <algorithm>
#include <strsafe.h>
#pragma comment(lib, "shlwapi.lib")

// Thread procedure
DWORD WINAPI CopyThreadProc(LPVOID lpParameter)
{
    CopyThreadParam* pParam = static_cast<CopyThreadParam*>(lpParameter);
    if (pParam && pParam->pCopier)
    {
        pParam->pCopier->DoCopyOperation();
    }
    return 0;
}

FileCopier::FileCopier()
    : m_packetSize(65536),
    m_thread(NULL),
    m_operationInProgress(false),
    m_totalPackets(0),
    m_completedPackets(0)
{
    // Create cancel event (manual reset)
    m_cancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Initialize critical section for thread safety
    InitializeCriticalSection(&m_cs);

    // Allocate reusable buffer
    m_buffer = std::make_unique<BYTE[]>(BUFFER_SIZE);
}

FileCopier::~FileCopier()
{
    // Cancel any ongoing operation
    Cancel();

    // Wait for thread to exit
    if (m_thread)
    {
        WaitForSingleObject(m_thread, INFINITE);
        CloseHandle(m_thread);
        m_thread = NULL;
    }

    // Close event handle
    if (m_cancelEvent)
    {
        CloseHandle(m_cancelEvent);
        m_cancelEvent = NULL;
    }

    // Delete critical section
    DeleteCriticalSection(&m_cs);
}

void FileCopier::AddSource(const std::wstring& path)
{
    // Don't modify sources during an operation
    if (m_operationInProgress)
        return;

    // Check if source already exists
    for (const auto& source : m_sources)
    {
        if (_wcsicmp(source.path.c_str(), path.c_str()) == 0)
            return;  // Source already exists
    }

    // Add the source
    SourceInfo info;
    info.path = path;
    info.status = L"Ready";
    info.speed = 0;

    m_sources.push_back(info);
}

void FileCopier::RemoveSource(size_t index)
{
    // Don't modify sources during an operation
    if (m_operationInProgress)
        return;

    if (index < m_sources.size())
    {
        m_sources.erase(m_sources.begin() + index);
    }
}

void FileCopier::ClearSources()
{
    // Don't modify sources during an operation
    if (m_operationInProgress)
        return;

    m_sources.clear();
}

const std::vector<SourceInfo>& FileCopier::GetSources() const
{
    return m_sources;
}

bool FileCopier::StartCopy(
    const std::wstring& destinationPath,
    ProgressCallbackFunc progressCallback,
    void* userData,
    int packetSize)
{
    // Check if already in progress
    if (m_operationInProgress)
        return false;

    // Check if we have sources
    if (m_sources.empty())
        return false;

    // Get the first source file information to determine size
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesEx(m_sources[0].path.c_str(), GetFileExInfoStandard, &fileInfo))
        return false;

    // Extract filename from the first source
    const wchar_t* fileName = PathFindFileName(m_sources[0].path.c_str());
    if (!fileName || !*fileName)
        return false;

    // Set destination file path
    m_destinationPath = destinationPath;

    // Ensure the destination path ends with a backslash
    if (!m_destinationPath.empty() && m_destinationPath.back() != L'\\')
        m_destinationPath += L'\\';

    // Append the filename to get the full destination path
    m_destinationFilename = m_destinationPath + fileName;

    // Store parameters
    m_packetSize = packetSize;
    m_progressCallback = progressCallback;
    m_userData = userData;

    // Calculate total number of packets
    LARGE_INTEGER fileSize;
    fileSize.HighPart = fileInfo.nFileSizeHigh;
    fileSize.LowPart = fileInfo.nFileSizeLow;

    m_totalPackets = static_cast<int>((fileSize.QuadPart + m_packetSize - 1) / m_packetSize);
    m_completedPackets = 0;

    // Reset cancel event
    ResetEvent(m_cancelEvent);

    // Set operation as in progress
    m_operationInProgress = true;

    // Create worker thread
    m_threadParam.pCopier = this;
    m_thread = CreateThread(
        NULL,                           // Default security attributes
        0,                              // Default stack size
        CopyThreadProc,                 // Thread function
        &m_threadParam,                 // Parameter to thread function
        0,                              // Default creation flags
        NULL);                          // Receive thread identifier

    if (!m_thread)
    {
        m_operationInProgress = false;
        return false;
    }

    return true;
}

void FileCopier::Cancel()
{
    if (m_operationInProgress && m_thread && m_cancelEvent)
    {
        // Signal the cancel event
        SetEvent(m_cancelEvent);

        // Wait for thread to exit (with timeout)
        if (WaitForSingleObject(m_thread, 5000) == WAIT_TIMEOUT)
        {
            // Force terminate the thread if it doesn't exit gracefully
            TerminateThread(m_thread, 1);
        }

        // Clean up
        CloseHandle(m_thread);
        m_thread = NULL;
        m_operationInProgress = false;
    }
}

bool FileCopier::IsOperationInProgress() const
{
    return m_operationInProgress;
}

void FileCopier::DoCopyOperation()
{
    // Create the destination directory if it doesn't exist
    if (!CreateDirectory(m_destinationPath.c_str(), NULL))
    {
        // Check if the error was because the directory already exists
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            // Directory creation failed and it doesn't exist
            EnterCriticalSection(&m_cs);
            m_operationInProgress = false;
            LeaveCriticalSection(&m_cs);
            return;
        }
    }

    // Create the destination file
    HANDLE hDestFile = CreateFile(
        m_destinationFilename.c_str(),
        GENERIC_WRITE,
        0,  // No sharing
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);

    if (hDestFile == INVALID_HANDLE_VALUE)
    {
        EnterCriticalSection(&m_cs);
        m_operationInProgress = false;
        LeaveCriticalSection(&m_cs);
        return;
    }

    // Get file size from first source
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesEx(m_sources[0].path.c_str(), GetFileExInfoStandard, &fileInfo))
    {
        LARGE_INTEGER fileSize;
        fileSize.HighPart = fileInfo.nFileSizeHigh;
        fileSize.LowPart = fileInfo.nFileSizeLow;

        // Pre-allocate the destination file for better performance
        LARGE_INTEGER distPos = { 0 };
        SetFilePointerEx(hDestFile, fileSize, NULL, FILE_BEGIN);
        SetEndOfFile(hDestFile);
        SetFilePointerEx(hDestFile, distPos, NULL, FILE_BEGIN);
    }

    // Process each packet
    for (int i = 0; i < m_totalPackets; i++)
    {
        // Check for cancel event
        if (WaitForSingleObject(m_cancelEvent, 0) == WAIT_OBJECT_0)
            break;

        // Calculate offset for this packet
        LARGE_INTEGER offset;
        offset.QuadPart = static_cast<LONGLONG>(i) * static_cast<LONGLONG>(m_packetSize);

        // Calculate actual packet size (last packet might be smaller)
        DWORD actualPacketSize = m_packetSize;
        if (i == m_totalPackets - 1)
        {
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            if (GetFileAttributesEx(m_sources[0].path.c_str(), GetFileExInfoStandard, &fileInfo))
            {
                LARGE_INTEGER fileSize;
                fileSize.HighPart = fileInfo.nFileSizeHigh;
                fileSize.LowPart = fileInfo.nFileSizeLow;

                LONGLONG remaining = fileSize.QuadPart - offset.QuadPart;
                if (remaining < actualPacketSize)
                    actualPacketSize = static_cast<DWORD>(remaining);
            }
        }

        // Try to copy from each source
        bool packetCopied = false;
        for (const auto& source : m_sources)
        {
            if (CopyPacket(source.path, hDestFile, offset, actualPacketSize, i, m_buffer.get()))
            {
                packetCopied = true;
                break;
            }

            // Check for cancel event
            if (WaitForSingleObject(m_cancelEvent, 0) == WAIT_OBJECT_0)
                break;
        }

        if (packetCopied)
        {
            // Increment completed packets counter
            EnterCriticalSection(&m_cs);
            m_completedPackets++;
            int completed = m_completedPackets;
            LeaveCriticalSection(&m_cs);

            // Report progress
            if (m_progressCallback)
            {
                m_progressCallback(completed, m_totalPackets, m_userData);
            }
        }
    }

    // Close the destination file
    CloseHandle(hDestFile);

    // Operation completed
    EnterCriticalSection(&m_cs);
    m_operationInProgress = false;
    LeaveCriticalSection(&m_cs);
}

bool FileCopier::CopyPacket(
    const std::wstring& sourcePath,
    HANDLE hDestFile,
    LARGE_INTEGER offset,
    DWORD packetSize,
    int packetIndex,
    BYTE* buffer)
{
    // Open the source file
    HANDLE hSrcFile = CreateFile(
        sourcePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);

    if (hSrcFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    // Seek to the correct position in the source file
    if (SetFilePointerEx(hSrcFile, offset, NULL, FILE_BEGIN) == FALSE)
    {
        CloseHandle(hSrcFile);
        return false;
    }

    // Seek to the correct position in the destination file
    if (SetFilePointerEx(hDestFile, offset, NULL, FILE_BEGIN) == FALSE)
    {
        CloseHandle(hSrcFile);
        return false;
    }

    // Read from source and write to destination in chunks for better memory management
    bool success = true;
    DWORD totalBytesRead = 0;

    while (totalBytesRead < packetSize)
    {
        // Check for cancel event
        if (WaitForSingleObject(m_cancelEvent, 0) == WAIT_OBJECT_0)
        {
            success = false;
            break;
        }

        // Calculate chunk size (using our fixed buffer)
        DWORD chunkSize = min(BUFFER_SIZE, packetSize - totalBytesRead);

        // Read from source
        DWORD bytesRead = 0;
        if (!ReadFile(hSrcFile, buffer, chunkSize, &bytesRead, NULL) || bytesRead == 0)
        {
            success = false;
            break;
        }

        // Write to destination
        DWORD bytesWritten = 0;
        if (!WriteFile(hDestFile, buffer, bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead)
        {
            success = false;
            break;
        }

        totalBytesRead += bytesRead;

        // If we read less than requested, we've reached the end of the file
        if (bytesRead < chunkSize)
            break;
    }

    // Close the source file
    CloseHandle(hSrcFile);

    return success;
}