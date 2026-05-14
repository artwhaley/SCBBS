@echo off
setlocal
set "ROOT=%~dp0"
set "_VSINSTALL="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" set "_VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\Community"
if not defined _VSINSTALL if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" set "_VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\Professional"
if not defined _VSINSTALL if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" set "_VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
if not defined _VSINSTALL if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" set "_VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
if not defined _VSINSTALL (
  echo Visual Studio 2022 not found in default install paths.
  exit /b 1
)
call "%_VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b 1
msbuild "%ROOT%SCMFD_Joystick_Root\SCMFD_Joystick_Root.vcxproj" %*
if errorlevel 1 exit /b %errorlevel%
msbuild "%ROOT%TestEnumeratorAndClient\TestEnumeratorAndClient.vcxproj" %*
