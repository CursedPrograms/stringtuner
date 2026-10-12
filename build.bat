@echo off
REM Builds stringtuner (standalone exe + VST3) in Release and puts them in the dist folder.
REM Needs CMake and Visual Studio 2022 with "Desktop development with C++".
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul || (echo CMake not found - install it from https://cmake.org and tick "add to PATH". & goto :fail)

if not exist build\CMakeCache.txt (
    echo Configuring - the first time this downloads JUCE, give it a few minutes...
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :fail
)

echo Building...
cmake --build build --config Release --parallel --target stringtuner_Standalone stringtuner_VST3 || goto :fail

if exist dist rmdir /s /q dist
mkdir dist
copy /y "build\stringtuner_artefacts\Release\Standalone\stringtuner.exe" dist\ >nul || goto :fail
xcopy /e /i /q /y "build\stringtuner_artefacts\Release\VST3\stringtuner.vst3" "dist\stringtuner.vst3" >nul || goto :fail

echo.
echo Done:
echo   %cd%\dist\stringtuner.exe       - standalone app
echo   %cd%\dist\stringtuner.vst3      - VST3: copy to C:\Program Files\Common Files\VST3
echo.
echo (If the build failed to overwrite the exe, close stringtuner and run this again.)
pause
exit /b 0

:fail
echo.
echo Build failed - see the messages above.
pause
exit /b 1
