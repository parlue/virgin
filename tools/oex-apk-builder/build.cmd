@echo off
setlocal
where py >nul 2>nul
if errorlevel 1 goto python
py -3 "%~dp0build.py" %*
exit /b %errorlevel%
:python
python "%~dp0build.py" %*
exit /b %errorlevel%
