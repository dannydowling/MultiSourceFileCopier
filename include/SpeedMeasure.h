#pragma once
#include <string>
#include <vector>
#include <memory>
#include <windows.h>

class SpeedMeasure {
public:
    SpeedMeasure();
    ~SpeedMeasure();

    // Measure speed of a single source file
    // Returns speed in Kbps, or -1 on error
    long long MeasureSourceSpeed(const std::wstring& sourcePath);

    // Measure speeds of a list of sources and sort by speed (descending)
    // Returns true if successful, false otherwise
    // The input vector is modified to contain paths sorted by speed
    bool MeasureAndSortSources(std::vector<std::wstring>& sources);

private:
    // Read a sample of data to measure speed
    // Returns speed in Kbps, or -1 on error
    long long MeasureFileSpeed(HANDLE hFile);

    // Sample buffer size for measurement (64KB is a good starting point)
    static const DWORD SAMPLE_SIZE = 64 * 1024;

    // Reusable buffer to avoid repeated allocations
    std::unique_ptr<BYTE[]> m_buffer;
};