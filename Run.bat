@echo off
title AUREA
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Extras\run.ps1"
if errorlevel 1 pause
