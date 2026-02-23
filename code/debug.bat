@echo off

pushd %~dp0..\data
cdb %~dp0..\build\win32_handmade.exe
popd
