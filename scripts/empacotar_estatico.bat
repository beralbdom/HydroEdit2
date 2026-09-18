@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --preset msvc-static || exit /b 1
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build --preset msvc-static || exit /b 1
if not exist dist mkdir dist
copy /y build-static\src\app\HydroEdit2.exe dist\HydroEdit2-estatico.exe >nul || exit /b 1
echo Executavel unico em dist\HydroEdit2-estatico.exe
