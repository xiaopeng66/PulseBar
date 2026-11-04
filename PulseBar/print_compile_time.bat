REM Write the current date and time to compile_time.txt
del /F /Q compile_time.txt
echo %date:~0,10% >> compile_time.txt
echo %time:~0,8% >> compile_time.txt