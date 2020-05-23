@echo off

REM Note(Zen): Create the output directory
if not exist Generated mkdir Generated

REM Note(Zen): Copy the data (such as images etc.) to output folder
start /b /wait "" "xcopy" .\data .\generated\data\ /y /s /e /q
set files= 
for %%i in (*.zsl) do ( call set "files=%%files%% %%i" )
sitegen.exe --title Zenarii --author Zenarii --html --header header.html --footer footer.html %files%

REM Note(Zen): Copy HTML/CSS to Generated directory, then copy all files to my sites directory for upload
robocopy ../sites Generated *.html > nul
robocopy ../sites Generated *.css > nul
robocopy Generated W:\site /S >nul
pause