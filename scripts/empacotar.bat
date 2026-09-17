@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --preset msvc-release || exit /b 1
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build --preset msvc-release || exit /b 1
if exist dist rmdir /s /q dist
mkdir dist
copy build-release\src\app\HydroEdit2.exe dist\ >nul
"C:\Qt\6.11.2\msvc2022_64\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw dist\HydroEdit2.exe || exit /b 1
echo Pacote em dist\
