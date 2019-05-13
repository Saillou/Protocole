@echo off
setlocal EnableDelayedExpansion

cd /d %~dp0
set fRelease=%~dp0Release\

call compileCode.bat Server mainServer
call compileCode.bat Client mainClient

:: Launch on success
cd /d %fRelease%/
set bug=0


:: Test existence
if not exist Server.exe (
	echo Server build has failed.
	set /a "bug=1"
)
if not exist Client.exe (
	echo Client build has failed.
	set /a "bug=1"
)
if !bug!==1 (
	echo. & pause & exit
)


:: Launch
start "Server" Server.exe
timeout 1
start "Client" Client.exe
REM pause
exit