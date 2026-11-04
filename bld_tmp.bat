@echo off
setlocal
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
msbuild PulseBar.sln /t:PulseBar /p:Configuration=Release /p:Platform=x64 /m /v:m /nologo
echo MSBUILD_EXIT=%errorlevel%
if errorlevel 1 exit /b %errorlevel%
python -X utf8 -I -B "%~dp0tools\sync-distribution.py" --build-output "%~dp0Bin\x64\Release" --write
exit /b %errorlevel%
