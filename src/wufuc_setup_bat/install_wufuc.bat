@echo off
:: Copyright (C) 2017 zeffy

:: This program is free software: you can redistribute it and/or modify
:: it under the terms of the GNU General Public License as published by
:: the Free Software Foundation, either version 3 of the License, or
:: (at your option) any later version.

:: This program is distributed in the hope that it will be useful,
:: but WITHOUT ANY WARRANTY; without even the implied warranty of
:: MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
:: GNU General Public License for more details.

:: You should have received a copy of the GNU General Public License
:: along with this program.  If not, see <http://www.gnu.org/licenses/>.

set "wufuc_version=^(unknown version^)"
set /p wufuc_version=<"%~dp0version.txt"
title wufuc %wufuc_version% Setup

echo Copyright ^(C^) 2017 zeffy
echo This program comes with ABSOLUTELY NO WARRANTY.
echo This is free software, and you are welcome to redistribute it
echo under certain conditions; see COPYING.txt for details.
echo.

fltmc >nul 2>&1 || (
        echo This batch script requires administrator privileges. Right-click on
        echo the script and select "Run as administrator".
        goto :die
)

:loop_args
if [%1]==[] goto :check_requirements
if /I "%1"=="/UNINSTALL" set "UNINSTALL=1"
if /I "%1"=="/UNATTENDED" set "UNATTENDED=1"
if /I "%1"=="/NORESTART" set "NORESTART=1"
shift /1
goto :loop_args

:check_requirements
echo Checking system requirements...

set "systemfolder=%systemroot%\System32"

if /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
        goto :is_x64
) else (
        if /I "%PROCESSOR_ARCHITEW6432%"=="AMD64" goto :is_wow64
        if /I "%PROCESSOR_ARCHITECTURE%"=="x86" goto :is_x86
)
goto :unsupported

:is_x86
set "WINDOWS_ARCHITECTURE=x86"
set "wufuc_dll=wufuc32.dll"
goto :dll_exists

:is_wow64
set "systemfolder=%systemroot%\Sysnative"
:is_x64
set "WINDOWS_ARCHITECTURE=x64"
set "wufuc_dll=wufuc64.dll"

:dll_exists
set "wufuc_dll_fullpath=%~dp0%wufuc_dll%"
if exist "%wufuc_dll_fullpath%" goto :xml_exists

echo ERROR - Could not find %wufuc_dll_fullpath%!
echo.
echo This most likely means you tried to clone the repository.
echo Please download wufuc from here: https://github.com/zeffy/wufuc/releases/latest
echo.
echo If you are using an unstable AppVeyor build, it could also mean you
echo downloaded the wrong build of wufuc for your operating system. If this
echo is the case, you need to download the %WINDOWS_ARCHITECTURE% build instead.
echo.
echo AVG ^(and possibly other anti-virus^) users:
echo This error could also mean that your anti-virus deleted or quarantined %wufuc_dll%
echo in which case, you will need to make an exception and restore it.
goto :die

:xml_exists
set "wufuc_xml=%~dp0wufuc_ScheduledTask.xml"
if exist "%wufuc_xml%" goto :check_winver

echo ERROR - Could not find %wufuc_xml%!
echo.
echo This most likely means you didn't extract all the files from the archive.
echo.
echo Please extract all the files from wufuc_%wufuc_version%.zip to a permanent
echo location like C:\Program Files\wufuc and try again.
goto :die

:check_winver
ver | findstr " 6\.1\." >nul && (
        echo Detected supported operating system: Windows 7 %WINDOWS_ARCHITECTURE%
        goto :check_mode
)
ver | findstr " 6\.3\." >nul && (
        echo Detected supported operating system: Windows 8.1 %WINDOWS_ARCHITECTURE%
        goto :check_mode
)

:unsupported
echo ERROR - Detected that you are using an unsupported operating system.
echo.
echo This patch only works on the following versions of Windows:
echo.
echo   - Windows 7 ^(x64 / x86^) ^& Windows Server 2008 R2   [6.1.xxxx]
echo   - Windows 8.1 ^(x64 / x86^) ^& Windows Server 2012 R2 [6.3.xxxx]
echo.
goto :die

:check_mode
set "wufuc_task=wufuc.{72EEE38B-9997-42BD-85D3-2DD96DA17307}"
set "space= "

if "%UNINSTALL%"=="1" goto :confirm_uninstall

:: BEGIN INSTALL MODE /////////////////////////////////////////////////////////
:confirm_install
if "%UNATTENDED%"=="1" goto :install
echo.
echo wufuc disables the "Unsupported Hardware" message in Windows Update,
echo and allows you to continue installing updates on Windows 7 and 8.1
echo systems with Intel Kaby Lake, AMD Ryzen, or other unsupported processors.
echo.
echo Please be absolutely sure you really need wufuc before proceeding.
echo.
set /p CONTINUE_INSTALL=Enter 'Y' if you want to install wufuc %wufuc_version%:%space%
if /I "%CONTINUE_INSTALL%"=="Y" goto :install
goto :cancel

:install
call :uninstall
net start Schedule
schtasks /Create /XML "%wufuc_xml%" /TN "%wufuc_task%" /F
schtasks /Change /TN "%wufuc_task%" /TR "'%systemroot%\System32\rundll32.exe' """%wufuc_dll_fullpath%""",RUNDLL32_Start"
schtasks /Change /TN "%wufuc_task%" /ENABLE
net stop wuauserv
schtasks /Run /TN "%wufuc_task%"

timeout /nobreak /t 3 >nul
net start wuauserv
echo.
echo You may need to restart your PC to finish installing wufuc.
goto :confirm_restart
:: END INSTALL MODE ///////////////////////////////////////////////////////////

:: BEGIN UNINSTALL MODE ///////////////////////////////////////////////////////
:confirm_uninstall
if "%UNATTENDED%"=="1" goto :uninstall_stub
echo.
set /p CONTINUE_UNINSTALL=Enter 'Y' if you want to uninstall wufuc:%space%
if /I "%CONTINUE_UNINSTALL%"=="Y" goto :uninstall_stub
goto :cancel

:uninstall_stub
call :uninstall
echo You may need to restart your PC to finish uninstalling wufuc.
goto :confirm_restart

:uninstall
        :: restore wuaueng.dll if it was modified by 0.1-0.5
        sfc /SCANFILE="%systemfolder%\wuaueng.dll"

        :: remove traces of wufuc 0.6-0.7, 0.9.999+
        schtasks /Query /TN "%wufuc_task%" >nul 2>&1 && (
        schtasks /Delete /TN "%wufuc_task%" /F )
        rundll32 "%wufuc_dll_fullpath%",RUNDLL32_Unload

        :: remove traces of wufuc 0.8.x
        set "regkey=HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\svchost.exe"
        set "wufuc_dll_target=%systemfolder%\%wufuc_dll%"
        reg query "%regkey%" >nul 2>&1 || goto :delete_target
        reg delete "%regkey%" /f || goto :skip_delete
:delete_target
        set "del_ext=.del-%random%"
        if exist "%wufuc_dll_target%" (
                ren "%wufuc_dll_target%" "%wufuc_dll%%del_ext%" && (
                rundll32 "%wufuc_dll_fullpath%",RUNDLL32_DeleteFile "%wufuc_dll_target%%del_ext%" )
        )
:skip_delete
        exit /b
:: END UNINSTALL MODE /////////////////////////////////////////////////////////

:confirm_restart
if "%NORESTART%"=="1" goto :die
if "%UNATTENDED%"=="1" goto :restart
echo.
set /p CONTINUE_RESTART=Enter 'Y' if you would like to restart now:%space%
if /I "%CONTINUE_RESTART%"=="Y" goto :restart
goto :die
:restart
shutdown /r /t 0
goto :die

:die
echo.
if "%UNATTENDED%"=="1" (
        timeout /t 5 /nobreak
) else (
        echo Press any key to exit...
        pause >nul
)
exit /b

:cancel
echo.
echo Canceled by user, press any key to exit...
pause >nul
exit /b
