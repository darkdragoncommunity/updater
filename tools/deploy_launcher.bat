@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Configurable values
set "BUILD_DIR=out\build\x64-Release"
set "DIST_DIR=dist\deploy-launcher"
set "STAGE_DIR=dist\deploy-stage"
set "RCLONE_REMOTE=your-r2-remote"
set "RCLONE_DEST=your-r2-remote:your-bucket/launcher"
set "BASE_URL=https://your-patch-server.com"
set "RCLONE_EXE=rclone"
set "CMAKE_EXE=cmake"
set "NINJA_EXE=ninja"
set "VCVARS_BAT="
set "VERSION_FILE=VERSION.txt"
set "TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
set "DO_BUILD=0"
set "EXPECTED_LAUNCHER_VERSION="

if /I "%~1"=="--build" set "DO_BUILD=1"

set "ROOT_DIR=%~dp0.."
pushd "%ROOT_DIR%" >nul

if not exist "%BUILD_DIR%\launcher.exe" (
    if "%DO_BUILD%"=="1" (
        echo Build output not found yet. Will configure and build first.
    ) else (
        echo launcher.exe not found in %BUILD_DIR%
        echo Build Release in Visual Studio first, or run:
        echo   tools\deploy_launcher.bat --build
        popd >nul
        exit /b 1
    )
)

where rclone >nul 2>nul
if errorlevel 1 (
    set "RCLONE_EXE=%LOCALAPPDATA%\Microsoft\WinGet\Packages\Rclone.Rclone_Microsoft.Winget.Source_8wekyb3d8bbwe\rclone-v1.73.2-windows-amd64\rclone.exe"
    if not exist "!RCLONE_EXE!" (
        echo rclone was not found in PATH.
        echo Also did not find WinGet install at:
        echo   !RCLONE_EXE!
        popd >nul
        exit /b 1
    )
)

where cmake >nul 2>nul
if errorlevel 1 (
    if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
        set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
    ) else if exist "%ProgramFiles(x86)%\CMake\bin\cmake.exe" (
        set "CMAKE_EXE=%ProgramFiles(x86)%\CMake\bin\cmake.exe"
    ) else (
        echo cmake was not found in PATH.
        echo Also did not find a standard install under Program Files.
        popd >nul
        exit /b 1
    )
)

where ninja >nul 2>nul
if errorlevel 1 (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" (
        set "NINJA_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    ) else (
        echo ninja was not found in PATH.
        echo Also did not find the Visual Studio bundled Ninja.
        popd >nul
        exit /b 1
    )
)

where cl >nul 2>nul
if errorlevel 1 (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS_BAT=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo cl.exe was not found in PATH.
        echo Also did not find vcvars64.bat for Visual Studio 2019 Community.
        popd >nul
        exit /b 1
    )
)

if not exist "%VERSION_FILE%" (
    > "%VERSION_FILE%" echo 1
)

if exist "launcher_settings.json" (
    echo Generating settings.bin from launcher_settings.json...
    python tools\encrypt_settings.py --config launcher_settings.json
    if errorlevel 1 (
        echo Failed to generate settings.bin
        popd >nul
        exit /b 1
    )
)

if "%DO_BUILD%"=="1" (
    for /f "usebackq delims=" %%V in (`powershell -NoProfile -Command "$v = 1; if (Test-Path '.\%VERSION_FILE%') { $raw = (Get-Content '.\%VERSION_FILE%' -Raw).Trim(); if ($raw -match '^[0-9]+$') { $v = [int]$raw + 1 } }; Set-Content '.\%VERSION_FILE%' $v -NoNewline; Write-Output $v"`) do set "BUILD_NUMBER=%%V"
    if not defined BUILD_NUMBER set "BUILD_NUMBER=1"
    set "EXPECTED_LAUNCHER_VERSION=1.0.!BUILD_NUMBER!.0"
    echo Build number: !BUILD_NUMBER!
    echo Expected launcher version: !EXPECTED_LAUNCHER_VERSION!

    if defined VCVARS_BAT (
        echo Loading Visual Studio build environment...
        call "%VCVARS_BAT%" >nul
        if errorlevel 1 (
            echo Failed to load Visual Studio build environment.
            popd >nul
            exit /b 1
        )
    )

    echo Reconfiguring launcher version...
    "%CMAKE_EXE%" -S . -B "build-deploy\x64-Release" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%" -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"
    if errorlevel 1 (
        echo CMake configure failed.
        popd >nul
        exit /b 1
    )

    echo Building Release...
    "%CMAKE_EXE%" --build "build-deploy\x64-Release" --config Release
    if errorlevel 1 (
        echo Build failed.
        popd >nul
        exit /b 1
    )

    set "BUILD_DIR=build-deploy\x64-Release"
) else (
    echo Using existing Release build from %BUILD_DIR%
)

for /f "usebackq delims=" %%V in (`powershell -NoProfile -Command "(Get-Item '.\%BUILD_DIR%\launcher.exe').VersionInfo.FileVersion"`) do set "LAUNCHER_VERSION=%%V"
if not defined LAUNCHER_VERSION set "LAUNCHER_VERSION=0.0.0"
echo Built launcher version: %LAUNCHER_VERSION%

if defined EXPECTED_LAUNCHER_VERSION (
    if /I not "%LAUNCHER_VERSION%"=="%EXPECTED_LAUNCHER_VERSION%" (
        echo Version mismatch. Expected %EXPECTED_LAUNCHER_VERSION% but built EXE reports %LAUNCHER_VERSION%.
        echo Aborting deploy to prevent self-update loops.
        popd >nul
        exit /b 1
    )
)

echo Packaging launcher...
powershell -ExecutionPolicy Bypass -File ".\tools\package_release.ps1" -BuildDir "%BUILD_DIR%" -OutputDir "%DIST_DIR%" -Zip
if errorlevel 1 (
    echo Packaging failed.
    popd >nul
    exit /b 1
)

if exist "%STAGE_DIR%" rd /s /q "%STAGE_DIR%"
mkdir "%STAGE_DIR%\launcher" >nul 2>nul
mkdir "%STAGE_DIR%\launcher\helper" >nul 2>nul

echo Building launcher.zip for self-update...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$zipPath = Join-Path '%ROOT_DIR%' '%STAGE_DIR%\\launcher\\launcher.zip'; $sourceDir = Join-Path '%ROOT_DIR%' '%DIST_DIR%'; $tempDir = Join-Path '%STAGE_DIR%' 'launcher_zip_src'; Remove-Item -Force -Recurse $tempDir -ErrorAction SilentlyContinue; Copy-Item $sourceDir $tempDir -Recurse -Force; Remove-Item -Force (Join-Path $tempDir 'helper\\autoupdate.exe') -ErrorAction SilentlyContinue; if (Test-Path $zipPath) { Remove-Item -Force $zipPath }; Compress-Archive -Path (Join-Path $tempDir '*') -DestinationPath $zipPath -Force; Remove-Item -Force -Recurse $tempDir"
if errorlevel 1 (
    echo Failed to build launcher.zip
    popd >nul
    exit /b 1
)

copy /y "%DIST_DIR%\launcher.exe" "%STAGE_DIR%\launcher\launcher.exe" >nul
if exist "%DIST_DIR%\helper\autoupdate.exe" (
    copy /y "%DIST_DIR%\helper\autoupdate.exe" "%STAGE_DIR%\launcher\helper\autoupdate.exe" >nul
)

(
echo {
echo   "version": "%LAUNCHER_VERSION%",
echo   "launcher_url": "%BASE_URL%/launcher/launcher.zip"
echo }
) > "%STAGE_DIR%\launcher\version.json"

rem Code signing placeholder. Uncomment and adapt when the certificate is ready.
rem signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a "%STAGE_DIR%\launcher\launcher.exe"
rem if exist "%STAGE_DIR%\launcher\helper\autoupdate.exe" signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a "%STAGE_DIR%\launcher\helper\autoupdate.exe"

set "RCLONE_FLAGS=--s3-no-check-bucket --stats-one-line --stats=2s --retries 1 -vv"

echo Uploading launcher files with rclone...
"%RCLONE_EXE%" copy "%STAGE_DIR%\launcher\launcher.exe" "%RCLONE_DEST%" %RCLONE_FLAGS%
if errorlevel 1 (
    echo Failed to upload launcher.exe
    popd >nul
    exit /b 1
)

"%RCLONE_EXE%" copy "%STAGE_DIR%\launcher\launcher.zip" "%RCLONE_DEST%" %RCLONE_FLAGS%
if errorlevel 1 (
    echo Failed to upload launcher.zip
    popd >nul
    exit /b 1
)

"%RCLONE_EXE%" copy "%STAGE_DIR%\launcher\version.json" "%RCLONE_DEST%" %RCLONE_FLAGS%
if errorlevel 1 (
    echo Failed to upload version.json
    popd >nul
    exit /b 1
)

if exist "%STAGE_DIR%\launcher\helper\autoupdate.exe" (
    "%RCLONE_EXE%" copy "%STAGE_DIR%\launcher\helper\autoupdate.exe" "%RCLONE_DEST%/helper" %RCLONE_FLAGS%
    if errorlevel 1 (
        echo Failed to upload helper\autoupdate.exe
        popd >nul
        exit /b 1
    )
)

echo Deploy complete.
echo Version: %LAUNCHER_VERSION%
echo Remote: %RCLONE_DEST%
echo URL root: %BASE_URL%/launcher

popd >nul
exit /b 0
