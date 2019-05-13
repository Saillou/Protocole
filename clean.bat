@echo off
setlocal EnableDelayedExpansion

::--------------------------------------------------------
::--------------- Script for cleaning ----------------------
::--------------------------------------------------------

:: ----- Define inputs -----
set fRelease=Release
set fObjects=Objects

:: Set environnement and variables
cd /d %~dp0
set fRelease=%~dp0!fRelease!
set fObjects=%~dp0!fObjects!

echo.

call :clean !fRelease!
call :clean !fObjects!

:: End - wait key
echo. & pause & exit


::------------------------------------------------------------------------------------
::--------------------------------- Functions section ----------------------------
::------------------------------------------------------------------------------------

::----------------- Delete folder content ------------------------------
:clean <SourcePath>	
	if exist %~1 (
		cd /d %~1
		for /F "delims=" %%i in ('dir /b') do (
			if exist "%%i\*" (
				rmdir "%%i" /s /q 
			) else del "%%i" /s /q
		)
	)
exit /b