# Multi-Source File Copier

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A Windows utility application for efficiently copying files from multiple sources by intelligently selecting the fastest source in real-time.

## Overview

Multi-Source File Copier is designed to solve a common problem: when you have multiple copies of the same file across different storage devices (local disks, network drives, USB drives), which one should you use for the fastest copy? This application measures the speed of each source and dynamically selects the fastest one to minimize copy time.

## Key Features

- **Multiple Source Support**: Add multiple copies of the same file from different locations
- **Speed Measurement**: Automatically measures and displays the read speed of each source file
- **Source Prioritization**: Sorts sources by speed for optimal copying
- **Multi-threaded Copying**: Uses optimized packet-based copying for maximum performance
- **Dynamic Source Switching**: Automatically falls back to alternative sources if the primary source becomes unresponsive
- **Adjustable Packet Size**: Fine-tune performance with configurable packet sizes
- **Progress Tracking**: Real-time progress display and status updates

## Requirements

- Windows 10 or newer
- Microsoft Visual C++ Redistributable (2019 or newer)
- Boost Library (version 1.88.0 or compatible)

## Installation

### From Release

1. Download the latest release from the [Releases page](https://github.com/yourusername/MultiSourceFileCopier/releases)
2. Extract the ZIP file to your desired location
3. Run `MultiSourceFileCopier.exe`

### Building from Source

1. Clone the repository
2. Open the solution in Visual Studio 2022 or newer
3. Install the required NuGet packages
4. Build the solution in Release mode
5. The executable will be in the `bin/Release` directory

## Usage

### Basic Operation

1. Launch the application
2. Click "Add Source" to add multiple source files (identical copies from different locations)
3. Click "Measure Speeds" to analyze the performance of each source
4. Select a destination folder using "Browse"
5. Choose a packet size (default 64KB works well for most cases)
6. Click "Start Copy" to begin the operation

### Adding a Folder

1. Click "Add Folder" to add all files from a directory
2. The application will recursively scan the directory and add all files

### Optimizing Performance

- For large files on fast media (SSDs), larger packet sizes (256KB-1MB) often perform better
- For network or slow media, smaller packet sizes (16KB-64KB) may be more reliable
- The "Measure Speeds" function can help identify slow or congested sources

## How It Works

Multi-Source File Copier uses a packet-based approach to copy files:

1. The file is divided into multiple packets (chunks)
2. Each packet can be copied from any available source
3. Sources are ranked by measured read speed
4. The application dynamically monitors performance during copying
5. If a source becomes slow or unresponsive, the application automatically switches to a faster alternative
6. This approach optimizes overall throughput by always using the fastest available source for each packet

## Troubleshooting

- **Copy operation fails**: Ensure you have read permissions for all source files and write permission for the destination
- **Slow copy speeds**: Try different packet sizes or check if any background processes are consuming disk or network bandwidth
- **Application crashes**: Make sure you have the required Microsoft Visual C++ Redistributable installed

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- The Boost library for multi-threading support
- Windows API for file system operations
- Taylor Swift
