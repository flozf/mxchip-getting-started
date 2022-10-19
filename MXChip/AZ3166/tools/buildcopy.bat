@echo off

SET MXCHIP_DRIVE=D:

setlocal
cd /d %~dp0\..

cmake --build build

if errorlevel 1 goto ende

if not exist %MXCHIP_DRIVE%\details.txt goto notfound

echo on
copy build\app\mxchip_azure_iot.bin %MXCHIP_DRIVE%\
@echo off
goto ende

:notfound
echo mxchip not found

:ende
