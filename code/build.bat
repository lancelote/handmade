@echo off

set SOURCE_DIR=%~dp0
set BUILD_DIR=%SOURCE_DIR%..\build

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
pushd %BUILD_DIR%
cl -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 /nologo /FC /Zi %SOURCE_DIR%win32_handmade.c user32.lib Gdi32.lib
popd
