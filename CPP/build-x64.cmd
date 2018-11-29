@echo on

set ROOT=%cd%\7zip
if not defined OUTDIR set OUTDIR=%ROOT%\bin64
mkdir %OUTDIR%

set OPTS=CPU=AMD64 MY_STATIC_LINK=1 NEW_COMPILER=1
set LFLAGS=/SUBSYSTEM:WINDOWS,"5.02"

cd %ROOT%\Bundles\Format7z
nmake %OPTS%
copy AMD64\7za.dll %OUTDIR%\7za.dll

cd %ROOT%\Bundles\Format7zF
nmake %OPTS%
copy AMD64\7z.dll %OUTDIR%\7z.dll

cd %ROOT%\UI\FileManager
nmake %OPTS%
copy AMD64\7zFM.exe %OUTDIR%\7zFM.exe

cd %ROOT%\UI\GUI
nmake %OPTS%
copy AMD64\7zG.exe %OUTDIR%\7zG.exe

cd %ROOT%\Bundles\Codec_flzma2
nmake %OPTS%
copy AMD64\flzma2.dll %OUTDIR%\flzma2-x64.dll

cd %ROOT%\..\..\C\Util\7zipInstall
nmake %OPTS%
copy AMD64\7zipInstall.exe %OUTDIR%\Install-x64.exe

cd %ROOT%\..\..\C\Util\7zipUninstall
nmake %OPTS%
copy AMD64\7zipUninstall.exe %OUTDIR%\Uninstall.exe

set LFLAGS=/SUBSYSTEM:CONSOLE,"5.02"
cd %ROOT%\UI\Console
nmake %OPTS%
copy AMD64\7z.exe %OUTDIR%\7z.exe

cd %ROOT%\Bundles\Alone
nmake %OPTS%
copy AMD64\7za.exe %OUTDIR%\7za.exe

:ende
cd %ROOT%\..
