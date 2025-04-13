#include "../include/SpeedMeasure.h"
#include <algorithm>
#include <map>

SpeedMeasure::SpeedMeasure()
{
    // Allocate the measurement buffer
    m_buffer = std::make_unique<BYTE[]>(SAMPLE_SIZE);
}

SpeedMeasure::~SpeedMeasure()
{
    // Buffer will be automatically cleaned up by unique_ptr
}

// Measure speed of a single source file
long long SpeedMeasure::MeasureSourceSpeed(const std::wstring& sourcePath)
{
    // Open the file
    HANDLE hFile = CreateFile(
        sourcePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return -1;  // Error opening file
    }

    // Measure the file speed
    long long speed = MeasureFileSpeed(hFile);

    // Close the file
    CloseHandle(hFile);

    return speed;
}

// Measure speeds of a list of sources and sort by speed
bool SpeedMeasure::MeasureAndSortSources(std::vector<std::wstring>& sources)
{
    if (sources.empty())
    {
        return false;
    }

    // Map of source paths to their speeds
    std::map<std::wstring, long long> sourceSpeedMap;

    // Measure each source
    for (const auto& path : sources)
    {
        long long speed = MeasureSourceSpeed(path);
        sourceSpeedMap[path] = speed;
    }

    // Sort the sources by speed (descending)
    std::sort(sources.begin(), sources.end(),
        [&sourceSpeedMap](const std::wstring& a, const std::wstring& b) {
            return sourceSpeedMap[a] > sourceSpeedMap[b];
        });

    return true;
}

// Read a sample of data to measure speed
long long SpeedMeasure::MeasureFileSpeed(HANDLE hFile)
{
    if (!m_buffer)
    {
        return -1;  // Buffer not available
    }

    // Try multiple measurements and take the average for more accuracy
    const int NUM_MEASUREMENTS = 3;
    long long totalSpeed = 0;
    int validMeasurements = 0;

    for (int i = 0; i < NUM_MEASUREMENTS; i++)
    {
        // Choose a different position in the file for each measurement
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize))
        {
            return -1;
        }

        // Skip this measurement if file is too small
        if (fileSize.QuadPart < SAMPLE_SIZE)
        {
            if (i == 0)
            {
                // If first measurement, try to read what we can
                SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            }
            else
            {
                // Otherwise skip this measurement
                continue;
            }
        }
        else
        {
            // Find a suitable random position, avoiding the very beginning and end
            // Use a simple algorithm to distribute the positions across the file
            LONGLONG position = 0;
            if (fileSize.QuadPart > SAMPLE_SIZE * 3)
            {
                position = (fileSize.QuadPart / 4) * (i + 1);
                position = min(position, fileSize.QuadPart - SAMPLE_SIZE * 2);
            }

            // Set the file pointer to the calculated position
            LARGE_INTEGER distanceToMove;
            distanceToMove.QuadPart = position;
            if (!SetFilePointerEx(hFile, distanceToMove, NULL, FILE_BEGIN))
            {
                return -1;
            }
        }

        // Get starting time with high precision
        LARGE_INTEGER frequency, startTime, endTime;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&startTime);

        // Read a sample from the file
        DWORD bytesRead = 0;
        BOOL result = ReadFile(hFile, m_buffer.get(), SAMPLE_SIZE, &bytesRead, NULL);

        // Get ending time
        QueryPerformanceCounter(&endTime);

        // Calculate speed
        if (result && bytesRead > 0)
        {
            // Calculate elapsed time in seconds
            double elapsedSeconds = static_cast<double>(endTime.QuadPart - startTime.QuadPart) /
                static_cast<double>(frequency.QuadPart);

            if (elapsedSeconds > 0.0)
            {
                // Calculate speed in Kbps (kilobits per second)
                // 8 bits per byte, 1000 bits per Kbit
                long long speedKbps = static_cast<long long>((bytesRead * 8) / (elapsedSeconds * 1000));
                totalSpeed += speedKbps;
                validMeasurements++;
            }
        }
    }

    // Calculate average speed
    if (validMeasurements > 0)
    {
        return totalSpeed / validMeasurements;
    }

    return -1;  // No valid measurements
}