@echo off
setlocal EnableDelayedExpansion

cd /d %~dp0
set fRelease=%~dp0Release\
set compile=1
set commit=1
set launch=1

if %compile%==1 (
	call compileCode.bat Server mainServer
	REM call compileCode.bat Client mainClient
)

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
) else if %commit%==1 (
	cd /d %~dp0
	call git add compile*
	call git add Sources/*
	call git commit -m "Automatic commit"
	call git push
	cd /d %fRelease%/
	echo.
	pause
)

if %commit%==0 (
	pause
)


:: Launch
if %launch%==1 (
	start "Server" Server.exe
	REM timeout 1
	REM start "Client" Client.exe
)

exit