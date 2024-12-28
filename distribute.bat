@echo off

cmake -B dist\win32\Output\build-fx -G "Visual Studio 17 2022" -DCMAKE_GENERATOR_PLATFORM=x64 -DPLUGIN_TYPE=Effect -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake -B dist\win32\Output\build-inst -G "Visual Studio 17 2022" -DCMAKE_GENERATOR_PLATFORM=x64 -DPLUGIN_TYPE=Instrument -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl

cmake --build dist\win32\Output\build-fx --config Release
cmake --build dist\win32\Output\build-inst --config Release

REM Check if Inno Setup's ISCC.exe is installed and in PATH
if not exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" (
    echo Inno Setup 6 is not installed or not in PATH.
    exit /b 1
)

setlocal

set "version="
set /p version=<VERSION
echo The version is: %version%

REM Compile the Inno Setup script
"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /dAppVersion="%version%" /dOutputBaseFilename="Automate-%version%-win32-setup" "dist\win32\installer.iss"

endlocal


