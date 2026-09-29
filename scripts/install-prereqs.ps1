# Installs Git, CMake, and Visual Studio 2022 Community with the C++ desktop workload.
# vcpkg is cloned afterwards (see README). Run from an elevated PowerShell if winget asks.

$ErrorActionPreference = "Stop"

Write-Host "Installing Git..."
winget install --id Git.Git -e --source winget --accept-package-agreements --accept-source-agreements

Write-Host "Installing CMake..."
winget install --id Kitware.CMake -e --source winget --accept-package-agreements --accept-source-agreements

Write-Host "Installing Visual Studio 2022 Community (Native Desktop). This is large and takes a while..."
winget install --id Microsoft.VisualStudio.2022.Community -e --source winget --accept-package-agreements --accept-source-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"

Write-Host "Done. Close and reopen the terminal so PATH updates apply."
