@echo off
setlocal
cd /d "%~dp0"
cocoa_mcp_server.exe --project CocoaProject.json %*
