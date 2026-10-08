@echo off
rem Opens a command prompt with the project-local Git, Git LFS and Node.js on PATH.
set "PATH=%~dp0..\.tools\mingit\cmd;%~dp0..\.tools\mingit\mingw64\bin;%~dp0..\.tools\node;%PATH%"
cd /d "%~dp0.."
echo seige2222 dev shell: git, git-lfs and node from .tools are on PATH.
cmd /k
