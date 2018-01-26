@echo on

set ROOT=%cd%\7zip
if not defined OUTDIR set OUTDIR=%ROOT%\bin32
mkdir %OUTDIR%

set OPTS=MY_STATIC_LINK=1 NEW_COMPILER=1
set LFLAGS=/SUBSYSTEM:WINDOWS,"5.01"

cd %ROOT%\Bundles\Format7z
nmake %OPTS%
copy O\7za.dll %OUTDIR%\7z.dll

cd %ROOT%\UI\FileManager
nmake %OPTS%
copy O\7zFM.exe %OUTDIR%\7zFM.exe

cd %ROOT%\UI\GUI
nmake %OPTS%
copy O\7zG.exe %OUTDIR%\7zG.exe

set LFLAGS=/SUBSYSTEM:CONSOLE,"5.01"
cd %ROOT%\UI\Console
nmake %OPTS%
copy O\7z.exe %OUTDIR%\7z.exe

cd %ROOT%\Bundles\Alone
nmake %OPTS%
copy O\7za.exe %OUTDIR%\7za.exe

:ende
cd %ROOT%\..
