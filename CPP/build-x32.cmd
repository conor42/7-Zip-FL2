@echo on

set ROOT=%cd%\7zip
if not defined OUTDIR set OUTDIR=%ROOT%\bin32
mkdir %OUTDIR%

set OPTS=MY_STATIC_LINK=1 NEW_COMPILER=1
set LFLAGS=/SUBSYSTEM:WINDOWS,"5.01"

cd %ROOT%\Bundles\Format7z
nmake %OPTS%
copy x86\7za.dll %OUTDIR%\7za.dll

cd %ROOT%\Bundles\Format7zF
nmake %OPTS%
copy x86\7z.dll %OUTDIR%\7z.dll

cd %ROOT%\UI\FileManager
nmake %OPTS%
copy x86\7zFM.exe %OUTDIR%\7zFM.exe

cd %ROOT%\UI\GUI
nmake %OPTS%
copy x86\7zG.exe %OUTDIR%\7zG.exe

cd %ROOT%\Bundles\Codec_flzma2
nmake %OPTS%
copy x86\flzma2.dll %OUTDIR%\flzma2-x32.dll

cd %ROOT%\..\..\C\Util\7zipInstall
nmake %OPTS%
copy x86\7zipInstall.exe %OUTDIR%\Install-x32.exe

cd %ROOT%\..\..\C\Util\7zipUninstall
nmake %OPTS%
copy x86\7zipUninstall.exe %OUTDIR%\Uninstall.exe

set LFLAGS=/SUBSYSTEM:CONSOLE,"5.01"
cd %ROOT%\UI\Console
nmake %OPTS%
copy x86\7z.exe %OUTDIR%\7z.exe

cd %ROOT%\Bundles\Alone
nmake %OPTS%
copy x86\7za.exe %OUTDIR%\7za.exe

:ende
cd %ROOT%\..
