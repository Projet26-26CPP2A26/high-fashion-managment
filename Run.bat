@echo off
title AUREA - Gestion des matieres
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Extras\run.ps1" %*
if errorlevel 1 pause
