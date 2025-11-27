# Build Instructions

## Prerequisites

### For User-Space Application:
- Windows 10/11 (x64)
- Visual Studio 2019 or later (with C++ Desktop Development workload)
- CMake 3.15 or later
- Windows SDK

### For Kernel Driver:
- Windows 10/11 SDK
- Windows Driver Kit (WDK) 10
- Visual Studio 2019 or later
- Test signing enabled (for development)

## Building the User-Space Application

### Using CMake:
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Using Visual Studio:
1. Open Visual Studio
2. File -> Open -> CMake -> Select CMakeLists.txt
3. Build -> Build All

### Manual Build (Visual Studio):
1. Create new Visual Studio C++ project
2. Add all .cpp files from `src/` directory
3. Add include directories: `src/`, `driver/`
4. Link against: `psapi.lib`
5. Set C++ standard to C++17
6. Build in Release mode

## Building the Kernel Driver

### Prerequisites:
1. Install Windows Driver Kit (WDK) 10
2. Enable test signing:
   ```cmd
   bcdedit /set testsigning on
   ```
   (Requires administrator privileges and reboot)

### Build Steps:
1. Open "x64 Native Tools Command Prompt for VS 2019" (or later)
2. Navigate to driver directory:
   ```cmd
   cd driver
   ```
3. Build the driver:
   ```cmd
   build -cZ
   ```

### Alternative: Using Visual Studio:
1. Open Visual Studio
2. File -> New -> Project -> Visual C++ -> Windows Driver -> Kernel Mode Driver, Empty (KMDF)
3. Copy `kernel_driver.c` and `kernel_driver.h` to the project
4. Update project settings:
   - Target Platform: Windows 10
   - Configuration Type: Driver
   - Driver Type: WDM
5. Build the project

## Driver Installation

### Test Certificate Signing:
1. Generate test certificate:
   ```cmd
   makecert -r -pe -ss PrivateCertStore -n "CN=GameMemoryReader Test" GameMemoryReader.cer
   ```
2. Sign the driver:
   ```cmd
   signtool sign /a /v /s PrivateCertStore /n "GameMemoryReader Test" /t http://timestamp.digicert.com GameMemoryReader.sys
   ```

### Loading the Driver:
1. Copy `GameMemoryReader.sys` to `C:\Windows\System32\drivers\`
2. Create service:
   ```cmd
   sc create GameMemoryReader type= kernel binPath= C:\Windows\System32\drivers\GameMemoryReader.sys
   ```
3. Start the service:
   ```cmd
   sc start GameMemoryReader
   ```

### Unloading the Driver:
```cmd
sc stop GameMemoryReader
sc delete GameMemoryReader
```

## Troubleshooting

### Driver Won't Load:
- Ensure test signing is enabled: `bcdedit /set testsigning on`
- Check driver is signed (even with test certificate)
- Verify driver file is in correct location
- Check Event Viewer for driver load errors

### Application Can't Connect to Driver:
- Verify driver service is running: `sc query GameMemoryReader`
- Check driver device name matches in code: `\\\\.\\GameMemoryReader`
- Ensure running as administrator

### Build Errors:
- Verify WDK is properly installed
- Check Visual Studio version compatibility
- Ensure all include paths are correct
- Verify Windows SDK version matches WDK version

## Development Notes

- The driver uses WDF (Windows Driver Framework) for better stability
- Test signing is required for development - production use requires proper code signing certificate
- Driver communication uses IOCTL codes for security
- Memory operations use kernel-mode APIs for direct access

## Security Warning

**IMPORTANT**: This software is for educational and research purposes only. Using kernel-level drivers to modify game memory may:
- Violate game terms of service
- Result in account bans
- Have legal implications
- Compromise system security

Use at your own risk and only in authorized testing environments.
