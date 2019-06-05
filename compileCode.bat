@echo off
setlocal EnableDelayedExpansion


::--------------------------------------------------------
::---------------- Script for build ----------------------
::--------------------------------------------------------

:: ----- Define inputs -----
set ExecutableName=%~1
set EntryMain="%~2"

set fSources=Sources
set fRelease=Release
set fObjects=Objects

set forceRebuild=0
set delExeOnFail=1
set launchExeOnSuccess=0

:: Set environnement and variables
cd /d %~dp0
set fSources=%~dp0!fSources!
set fRelease=%~dp0!fRelease!
set fObjects=%~dp0!fObjects!

:: ----- Define tools -----
:: -- Visual Studio 2013 - 64 bits
REM set COMPILER="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\amd64\cl.exe"
REM set LINKER="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\amd64\link.exe"
REM set EVT_SCRIPT="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\amd64\vcvars64.bat"

:: -- Visual Studio 2013 - 32 bits
REM set COMPILER="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\cl.exe"
REM set LINKER="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\link.exe"
REM set EVT_SCRIPT="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\vcvars32.bat"


:: -- Visual Studio 2017 - 64bits
set COMPILER="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.12.25827\bin\Hostx64\x64\cl.exe"
set LINKER="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.12.25827\bin\Hostx64\x64\link.exe"
set EVT_SCRIPT="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

:: -- Visual Studio 2017 - 32bits
REM set COMPILER="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.12.25827\bin\Hostx86\x86\cl.exe"
REM set LINKER="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.12.25827\bin\Hostx86\x86\link.exe"
REM set EVT_SCRIPT="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"


:: ----- Define dependecies -----
set INCLUDES_PATH_DEP=^
	D:\Dev\Opencv4\build_vc15_x64\install\include ^
	D:\Dev\LibJpeg\libjpeg-turbo-1.5.2 ^
	D:\Dev\h264\openh264\codec\api\ ^
	D:\Dev\h264\openh264\test\utils

set LIBRARIES_PATH_DEP=^
	D:\Dev\Opencv4\build_vc15_x64\install\x64\vc15\lib ^
	D:\Dev\LibJpeg\build_vc17x64\Release ^
	D:\Dev\h264\openh264\bin\x64\Release
 
set LIBRARIES_NAME_DEP=^
	ws2_32.lib ^
	opencv_core400.lib ^
	opencv_imgproc400.lib ^
	opencv_highgui400.lib ^
	opencv_imgcodecs400.lib ^
	opencv_dnn400.lib ^
	opencv_videoio400.lib ^
	turbojpeg.lib ^
	welsenc.lib ^
	welsdec.lib ^
	Strmiids.lib ^
	ole32.lib
 
set COMMAND_LAUNCH=

:: Dependencies externes
echo.
call :needFile D:\Dev\Opencv4\build_vc15_x64\install\x64\vc15\bin opencv_core400.dll !fRelease!
call :needFile D:\Dev\Opencv4\build_vc15_x64\install\x64\vc15\bin opencv_highgui400.dll !fRelease!
call :needFile D:\Dev\Opencv4\build_vc15_x64\install\x64\vc15\bin opencv_imgcodecs400.dll !fRelease!
call :needFile D:\Dev\Opencv4\build_vc15_x64\install\x64\vc15\bin opencv_imgproc400.dll !fRelease!
call :needFile D:\Dev\Opencv4\build_vc15_x64\install\x64\vc15\bin opencv_videoio400.dll !fRelease!
call :needFile D:\Dev\Opencv4\build_vc15_x64\install\x64\vc15\bin opencv_ffmpeg400_64.dll !fRelease!
call :needFile D:\Dev\LibJpeg\build_vc17x64\Release turbojpeg.dll !fRelease!
call :needFile D:\Dev\h264\openh264\bin\x64\Release welsdec.dll !fRelease!
call :needFile D:\Dev\h264\openh264\bin\x64\Release welsenc.dll !fRelease!
call :needFile D:\Dev\h264\openh264\bin\x64\Release welsvp.dll !fRelease!
echo.

::--------------------------------------------------------

if not exist %fSources% (
	echo %fSources% doesn't exist. Abort.
	echo. & pause & exit
)
call :createEvt

:: --- Track all files (relative path) ---
call :stringlength sizeParentPath fSources
set /a "sizeParentPath+=1"

:: cpp - store all files to be compiled - remove not good main[..].cpp
set "nbCppFiles=-1"
for /r %fSources% %%g in (*.c *.cpp) do (
	set absPath=%%g
	set relPath=!absPath:~%sizeParentPath%!
	
	echo !relPath!|find "main" >nul
	if errorlevel 1 (
		:: Add this .cpp because it's not a main
		set /a "nbCppFiles+=1"
		set _listCppFiles[!nbCppFiles!]=!relPath!
	) else (
		echo !%%g!|find !EntryMain! >nul
		if not  errorlevel 1 (
			:: Add this .cpp because it's the good main
			set /a "nbCppFiles+=1"
			set _listCppFiles[!nbCppFiles!]=!relPath!
		)
	)
)

:: hpp - store all and find the most recent
set "nbHppFiles=-1"
set "dtRecent.d=01/01/0001 01:01"
set "dtRecent.t=01:01:01"

for /r %fSources% %%g in (.*h *.hpp) do (
	set absPath=%%g
	set relPath=!absPath:~%sizeParentPath%!

	set /a "nbHppFiles+=1"
	set _listHppFiles[!nbHppFiles!]=!relPath!
	
	call :dateFile %fSources%\!relPath! dt
	call :compareDate "!dtRecent.d!" !dtRecent.t! "!dt.d!" !dt.t! result

	if !result! lss 0 (
		set "dtRecent.d=!dt.d!"
		set "dtRecent.t=!dt.t!"	
	)
)

:: -- Compile sources if needed --
set "updated=0"
set "failed=0"
for /L %%i in (0, 1, %nbCppFiles%) do (
	set fileName=!_listCppFiles[%%i]!
	
	set objectName=!fileName:\=_!
	set objectName=!objectName:.cpp=.obj!
	set objectName=!objectName:.c=.obj!

	:: Compile or not ?
	set "needed=1"
	
	if %forceRebuild% == 0 (
		if exist %fObjects%\!objectName! (	
			:: Compare date
			call :dateFile %fObjects%\!objectName! dtObj
			call :dateFile %fSources%\!fileName! dtFile
			call :compareDate "!dtObj.d!" !dtObj.t! "!dtFile.d!" !dtFile.t! resultFO
			
			if !resultFO! gtr 0 (
				call :compareDate "!dtObj.d!" !dtObj.t! "!dtRecent.d!" !dtRecent.t! resultRO
				if !resultRO! gtr 0 set "needed=0"
			)
		)
	)
	
	:: Compile
	if !needed! == 1 (
		if !updated! == 0 (
			echo  Compiling..
			set "updated=1"
		)
		
		if %delExeOnFail% == 1 (
			if exist %fObjects%\!objectName! del %fObjects%\!objectName!
		)
		
		::------------------ Compile ------------------------------
		call :compile %fSources%\!fileName! %fObjects%\!objectName!
		
		if not exist %fObjects%\!objectName! (
			echo.
			echo. --- Failed to compile %fSources%\!fileName!
			echo.
			set "failed=1"
		)
	)
)

:: -- Linking if any object has been created --
if not exist %fRelease%\%ExecutableName%.exe (
	if %failed% == 0 (
		set "updated=1"
	)
)
if %failed% == 0 (
	if %updated% == 1 (
		echo.
		echo  Linking..
		
		set objectsList=0
		for /r %fObjects% %%g in (*.obj) do (
			echo !%%g!|find "main" >nul
			if errorlevel 1 (
				:: Add this .obj because it's not a main
				if !objectsList! == 0 (set objectsList=%%g) else (set objectsList=!objectsList! %%g)
			) else (
				echo !%%g!|find !EntryMain! >nul
				if not  errorlevel 1 (
				:: Add this .obj because it's the good main
					if !objectsList! == 0 (set objectsList=%%g) else (set objectsList=!objectsList! %%g)
				)
			)
		)

		::--------------------- Link -----------------------------
		call :link "!objectsList!" %fRelease%\%ExecutableName%
	) else echo  Already up-to-date.
) else (
	if exist %fRelease%\%ExecutableName%.exe del %fRelease%\%ExecutableName%.exe
)
	

	
:: Launch on success
if exist %fRelease%\%ExecutableName%.exe (
	echo.
	
	::------------------ Execute -----------------------------
	if %launchExeOnSuccess% == 1 (
		echo.---
		echo.
		cd /d %fRelease%/
		%ExecutableName%.exe %COMMAND_LAUNCH%
	)
) else echo Failed build.

exit /b


::------------------------------------------------------------------------------------
::--------------------------------- Functions section ----------------------------
::------------------------------------------------------------------------------------

::----------------- Building ------------------------------
:compile <SourcePath> <DstObjectPath>	
	set CMD_INCLUDES_PATH_DEP=0
	if "%INCLUDES_PATH_DEP%"=="" (
		set CMD_INCLUDES_PATH_DEP=
	) else (
		(for %%a in (%INCLUDES_PATH_DEP%) do ( 
			if !CMD_INCLUDES_PATH_DEP! == 0 (set CMD_INCLUDES_PATH_DEP=/I %%a) else (set CMD_INCLUDES_PATH_DEP=!CMD_INCLUDES_PATH_DEP! /I %%a)
		))
	)
	
	%COMPILER% ^
		/c /EHa /W3 /MD /nologo /O2 /Ob2 ^
		%CMD_INCLUDES_PATH_DEP% ^
		%~1 /Fo%~2
exit /b

:link <AllSourcesPath> <DstExecutablePath>
	set CMD_LIBRARIES_PATH_DEP=0
	if "%LIBRARIES_PATH_DEP%"=="" (
		set CMD_LIBRARIES_PATH_DEP=
	) else (
		(for %%a in (%LIBRARIES_PATH_DEP%) do ( 
			if !CMD_LIBRARIES_PATH_DEP! == 0 (set CMD_LIBRARIES_PATH_DEP=/LIBPATH:%%a) else (set CMD_LIBRARIES_PATH_DEP=!CMD_LIBRARIES_PATH_DEP! /LIBPATH:%%a)
		))
	)
	
	set CMD_LIBRARIES_NAME_DEP=0
	if "%LIBRARIES_NAME_DEP%"=="" (
		set CMD_LIBRARIES_NAME_DEP=
	) else (
		(for %%a in (%LIBRARIES_NAME_DEP%) do ( 
			if !CMD_LIBRARIES_NAME_DEP! == 0 (set CMD_LIBRARIES_NAME_DEP=%%a) else (set CMD_LIBRARIES_NAME_DEP=!CMD_LIBRARIES_NAME_DEP! %%a)
		))
	)

	%LINKER% ^
		/SUBSYSTEM:CONSOLE /ENTRY:"mainCRTStartup" ^
		/MACHINE:X64 /INCREMENTAL:NO /NOLOGO /ERRORREPORT:PROMPT ^
		%CMD_LIBRARIES_PATH_DEP% ^
		%CMD_LIBRARIES_NAME_DEP% ^
		%~1 /out:%~2.exe
exit /b


::--------------------------------------------------------
:createEvt
:: Arborescence
if not exist %fObjects% mkdir %fObjects%
if not exist %fRelease% mkdir %fRelease%	

:: MSVC variables
call %EVT_SCRIPT%
echo. -------
echo.
exit /b


:needFile <Source> <FileName> <Where>
if not exist "%~3\%~2" (
	copy "%~1\%~2" "%~3"
)
exit /b

:dateFile <Path> <DateTime>
setlocal
set completePath=%~1

for %%a in (%completePath%) do (
	set nameFile=%%~na%%~xa
	set pathFile=%%~dpa
	set dateFile=%%~ta
)

for /f "delims=" %%i in ('"forfiles /p %pathFile% /m %nameFile% /c "cmd /c echo @ftime" "') do (
	set timeFile=%%i
)

(
	endlocal
	set "%~2.d=%dateFile%"
	set "%~2.t=%timeFile%"
	exit /b
)

:compareDateFile <Path1> <Path2> <Result>
setlocal 
call :dateFile %~1 dt1
call :dateFile %~2 dt2
call :compareDate "!dt1.d!" !dt1.t! "!dt2.d!" !dt2.t! result
(
	endlocal
	set "%~3=%result%"
	exit /b	
)

:compareDate <Date1> <Time1> <Date2> <Time2> <Result>
set dt1.d=%~1
set dt1.t=%~2

set dt2.d=%~3
set dt2.t=%~4

:: Split dates
for /f "tokens=1 delims= " %%i in ("%dt1.d%") do (
	for /f "tokens=1,2,3 delims=/" %%j in ("%%i") do (
		set a1[2]=%%j & set a1[1]=%%k & set a1[0]=%%l
	)
)
for /f "tokens=1 delims= " %%i in ("%dt2.d%") do (
	for /f "tokens=1,2,3 delims=/" %%j in ("%%i") do (
		set a2[2]=%%j & set a2[1]=%%k & set a2[0]=%%l
	)
)


:: Split times
for /f "tokens=1,2,3 delims=:" %%j in ("%dt1.t%") do (
	set a1[3]=%%j & set a1[4]=%%k & set a1[5]=%%l
)
for /f "tokens=1,2,3 delims=:" %%j in ("%dt2.t%") do (
	set a2[3]=%%j & set a2[4]=%%k & set a2[5]=%%l
)


:: Compare
set "res=0"
for /l %%i in (0, 1, 5) do (
	:: Need to remove leading 0
	set /a "a1i=1!a1[%%i]!-(11!a1[%%i]!-1!a1[%%i]!)/10"
	set /a "a2i=1!a2[%%i]!-(11!a2[%%i]!-1!a2[%%i]!)/10"
	
	if !res! == 0 (
		set /a "a12=!a1i!-!a2i!"
		if !a12! neq 0 set "res=!a12!"
	)
)
(
	endlocal
	
	if %res% lss 0 (
		set "%~5=-1"
	) else if %res% gtr 0 (
		set "%~5=1"
	) else (
		set "%~5=0"
	)
	
	exit /b	
)

:stringlength <ResultVar> <StringVar>
(
	setlocal
	set "s=!%~2!#"
	set "len=0"
	for %%P in (4096 2048 1024 512 256 128 64 32 16 8 4 2 1) do (
		if "!s:~%%P,1!" NEQ "" ( 
			set /a "len+=%%P"
			set "s=!s:~%%P!"
		)
	)
)
(
	endlocal
	set "%~1=%len%"
	exit /b
)