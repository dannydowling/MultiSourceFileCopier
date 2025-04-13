#include "../include/FileCopier.h"
#include <C:/temp/boost_1_88_0/boost/thread.hpp>
#include <C:/temp/boost_1_88_0/boost/thread/mutex.hpp>
#include <C:/temp/boost_1_88_0/boost/thread/condition_variable.hpp>
#include <C:/temp/boost_1_88_0/boost/chrono.hpp>
#include <shlwapi.h>
#include <algorithm>
#include <strsafe.h>
#include <queue>
#include <vector>
#include <memory>

#pragma comment(lib, "shlwapi.lib")



// Thread procedure implementation
DWORD WINAPI CopyThreadProc(LPVOID lpParameter)
{
    CopyThreadParam* pParam = static_cast<CopyThreadParam*>(lpParameter);
    if (pParam && pParam->pCopier)
    {
        pParam->pCopier->DoCopyOperation();
    }
    return 0;
}

// Constructor
FileCopier::FileCopier()
    : m_packetSize(65536),
    m_thread(NULL),
    m_operationInProgress(false),
    m_totalPackets(0),
    m_completedPackets(0),
    m_progressCallback(nullptr),
    m_userData(nullptr)
{
    // Create cancel event (manual reset)
    m_cancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Initialize critical section for thread safety
    InitializeCriticalSection(&m_cs);

    // Allocate reusable buffer
    m_buffer = std::make_unique<BYTE[]>(BUFFER_SIZE);
}

// Destructor
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

// Add a source file
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

// Remove a source file
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

// Clear all sources
void FileCopier::ClearSources()
{
    // Don't modify sources during an operation
    if (m_operationInProgress)
        return;

    m_sources.clear();
}

// Get list of sources
const std::vector<SourceInfo>& FileCopier::GetSources() const
{
    return m_sources;
}

// Start copying files
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

    // Set destination path
    m_destinationPath = destinationPath;

    // Ensure the destination path ends with a backslash
    if (!m_destinationPath.empty() && m_destinationPath.back() != L'\\')
        m_destinationPath += L'\\';

    // Store parameters
    m_packetSize = packetSize;
    m_progressCallback = progressCallback;
    m_userData = userData;

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


// Cancel the copy operation
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
#pragma warning(suppress: 6258) // Intentional force termination after timeout
            TerminateThread(m_thread, 1);
        }

        // Clean up
        CloseHandle(m_thread);
        m_thread = NULL;
        m_operationInProgress = false;
    }
}

// Check if a copy is in progress
bool FileCopier::IsOperationInProgress() const
{
    return m_operationInProgress;
}

// Copy a single packet from source to destination
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

// Add a source file with additional information
void FileCopier::AddSourceWithInfo(const SourceInfo& info)
{
    // Don't modify sources during an operation
    if (m_operationInProgress)
        return;

    // Check if source already exists
    for (const auto& source : m_sources)
    {
        if (_wcsicmp(source.path.c_str(), info.path.c_str()) == 0)
            return;  // Source already exists
    }

    // Add the source with provided info
    m_sources.push_back(info);
}

// Recursively add files from a directory
int FileCopier::AddSourceDirectory(const std::wstring& directoryPath, bool recursive)
{
    int filesAdded = 0;
    std::wstring searchPath = directoryPath;

    // Ensure path ends with backslash
    if (!searchPath.empty() && searchPath.back() != L'\\')
        searchPath += L'\\';

    // Search for all files in the directory
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile((searchPath + L"*").c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            // Skip . and .. directories
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
                continue;

            std::wstring fullPath = searchPath + findData.cFileName;

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // Recursively process subdirectories if recursive flag is set
                if (recursive)
                    filesAdded += AddSourceDirectory(fullPath, recursive);
            }
            else
            {
                // It's a file, add it to our sources
                AddSource(fullPath);
                filesAdded++;
            }
        } while (FindNextFile(hFind, &findData));

        FindClose(hFind);
    }

    return filesAdded;
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

    // Process each source file
    int totalFilesCount = static_cast<int>(m_sources.size());
    int completedFilesCount = 0;
    bool allSuccess = true;

    for (size_t sourceIndex = 0; sourceIndex < m_sources.size(); sourceIndex++)
    {
        const std::wstring& sourcePath = m_sources[sourceIndex].path;

        // Extract filename from the current source
        const wchar_t* fileName = PathFindFileName(sourcePath.c_str());
        if (!fileName || !*fileName)
            continue;

        // Create destination file path for this source
        std::wstring destinationFilename = m_destinationPath + fileName;

        // Get file size for this source
        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        LARGE_INTEGER fileSize = { 0 };
        if (!GetFileAttributesEx(sourcePath.c_str(), GetFileExInfoStandard, &fileInfo))
            continue;

        fileSize.HighPart = fileInfo.nFileSizeHigh;
        fileSize.LowPart = fileInfo.nFileSizeLow;

        // Create the destination file
        HANDLE hDestFile = CreateFile(
            destinationFilename.c_str(),
            GENERIC_WRITE,
            0,  // No sharing
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);

        if (hDestFile == INVALID_HANDLE_VALUE)
            continue;

        // Pre-allocate the destination file for better performance
        LARGE_INTEGER distPos = { 0 };
        SetFilePointerEx(hDestFile, fileSize, NULL, FILE_BEGIN);
        SetEndOfFile(hDestFile);
        SetFilePointerEx(hDestFile, distPos, NULL, FILE_BEGIN);

        // Calculate number of packets for this file
        int filePackets = static_cast<int>((fileSize.QuadPart + m_packetSize - 1) / m_packetSize);

        // Update total packets for progress
        m_totalPackets = filePackets;
        m_completedPackets = 0;

        // Copy the file in packets
        bool fileSuccess = true;
        for (int i = 0; i < filePackets; i++)
        {
            // Check for cancel
            if (WaitForSingleObject(m_cancelEvent, 0) == WAIT_OBJECT_0)
            {
                fileSuccess = false;
                allSuccess = false;
                break;
            }

            LARGE_INTEGER offset;
            offset.QuadPart = static_cast<LONGLONG>(i) * static_cast<LONGLONG>(m_packetSize);

            // Calculate actual packet size (last packet might be smaller)
            DWORD actualPacketSize = m_packetSize;
            if (i == filePackets - 1 && fileSize.QuadPart > 0)
            {
                LONGLONG remaining = fileSize.QuadPart - offset.QuadPart;
                if (remaining < actualPacketSize)
                    actualPacketSize = static_cast<DWORD>(remaining);
            }

            // Copy this packet
            if (!CopyPacket(sourcePath, hDestFile, offset, actualPacketSize, i, m_buffer.get()))
            {
                fileSuccess = false;
                allSuccess = false;
                break;
            }

            // Update progress
            EnterCriticalSection(&m_cs);
            m_completedPackets++;
            int completed = m_completedPackets;
            LeaveCriticalSection(&m_cs);

            // Report progress per file
            if (m_progressCallback)
            {
                m_progressCallback(completed, filePackets, m_userData);
            }
        }

        // Close the destination file
        CloseHandle(hDestFile);

        if (!fileSuccess)
            break;

        // Update completed files count
        completedFilesCount++;
    }

    // Operation completed
    EnterCriticalSection(&m_cs);
    m_operationInProgress = false;
    LeaveCriticalSection(&m_cs);
}