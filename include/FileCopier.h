#pragma once
#include <string>
#include <vector>
#include <memory>
#include <windows.h>

// Progress callback function type
typedef void (*ProgressCallbackFunc)(int completed, int total, void* userData);

// Source file information
struct SourceInfo {
    std::wstring path;        // File path
    std::wstring status;      // Current status (e.g., "Ready", "Copying", etc.)
    long long speed;          // Measured speed in Kbps
};

// Thread parameter structure
struct CopyThreadParam {
    class FileCopier* pCopier;
};

class FileCopier {
public:
    FileCopier();
    ~FileCopier();

    // Add a source file
    void AddSource(const std::wstring& path);

    // Remove a source file
    void RemoveSource(size_t index);

    // Clear all sources
    void ClearSources();

    // Get list of sources
    const std::vector<SourceInfo>& GetSources() const;

    // Start copying files
    bool StartCopy(
        const std::wstring& destinationPath,
        ProgressCallbackFunc progressCallback,
        void* userData,
        int packetSize = 65536    // 64KB default
    );

    // Cancel the copy operation
    void Cancel();

    // Check if a copy is in progress
    bool IsOperationInProgress() const;

    // Friend function for thread procedure
    friend DWORD WINAPI CopyThreadProc(LPVOID lpParameter);

private:
    // Copy operation function
    void DoCopyOperation();

    // Copy a single packet from source to destination
    bool CopyPacket(
        const std::wstring& sourcePath,
        HANDLE hDestFile,
        LARGE_INTEGER offset,
        DWORD packetSize,
        int packetIndex,
        BYTE* buffer
    );

    // Member variables
    std::vector<SourceInfo> m_sources;
    std::wstring m_destinationPath;
    std::wstring m_destinationFilename;
    int m_packetSize;
    ProgressCallbackFunc m_progressCallback;
    void* m_userData;

    // Threading
    HANDLE m_thread;
    HANDLE m_cancelEvent;
    CopyThreadParam m_threadParam;
    bool m_operationInProgress;

    // Progress tracking
    int m_totalPackets;
    int m_completedPackets;
    CRITICAL_SECTION m_cs;  // For thread synchronization

    // Optimizations
    std::unique_ptr<BYTE[]> m_buffer;  // Reusable buffer for copying
    static const DWORD BUFFER_SIZE = 1024 * 1024;  // 1MB buffer
};

// Thread procedure (declared outside of class for Win32 API compatibility)
DWORD WINAPI CopyThreadProc(LPVOID lpParameter);