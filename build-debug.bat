@echo off
setlocal
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0"
msbuild PulseBar.sln /t:PulseBar /p:Configuration=Debug /p:Platform=x64 /m /v:m /nologo
if errorlevel 1 exit /b %errorlevel%
python -X utf8 -I -B "%~dp0tools\stage-languages.py" --build-output "Bin/x64/Debug"
exit /b %errorlevel%
