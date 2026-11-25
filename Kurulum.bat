@echo off
chcp 65001 >nul
title ErgunDPI Kurulum

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Yonetici olarak calistirman lazim.
    pause
    exit /b 1
)

if not exist "%~dp0ErgunDPI.exe" (
    echo ErgunDPI.exe eksik.
    pause
    exit /b 1
)

if not exist "%~dp0WinDivert.dll" if not exist "%~dp0WinDivert64.dll" (
    echo WinDivert.dll eksik.
    pause
    exit /b 1
)

if not exist "%~dp0WinDivert64.sys" (
    echo WinDivert64.sys eksik.
    pause
    exit /b 1
)

echo C:\ErgunDPI klasorune kurulacak, Cloudflare DNS (DoH) acilip IPv6 kapatilacak.
set /p secim="Devam edilsin mi? (E/H): "
if /I not "%secim%"=="E" exit /b 0

taskkill /f /im ErgunDPI.exe >nul 2>&1
sc stop WinDivert >nul 2>&1
sc stop WinDivert1.4 >nul 2>&1
timeout /t 1 /nobreak >nul

if not exist "C:\ErgunDPI" mkdir "C:\ErgunDPI"
copy /Y "%~dp0ErgunDPI.exe" "C:\ErgunDPI\ErgunDPI.exe" >nul
if exist "%~dp0WinDivert.dll" copy /Y "%~dp0WinDivert.dll" "C:\ErgunDPI\WinDivert.dll" >nul
if exist "%~dp0WinDivert64.dll" copy /Y "%~dp0WinDivert64.dll" "C:\ErgunDPI\WinDivert64.dll" >nul
copy /Y "%~dp0WinDivert64.sys" "C:\ErgunDPI\WinDivert64.sys" >nul

powershell -NoProfile -Command "Get-NetAdapter | Where-Object Status -eq 'Up' | ForEach-Object { Set-DnsClientServerAddress -InterfaceIndex $_.ifIndex -ServerAddresses ('1.1.1.1','1.0.0.1'); Disable-NetAdapterBinding -Name $_.Name -ComponentID ms_tcpip6 -Confirm:$false }"
netsh dns add encryption server=1.1.1.1 dohtemplate=https://cloudflare-dns.com/dns-query autoupgrade=yes udpfallback=yes >nul 2>&1
netsh dns add encryption server=1.0.0.1 dohtemplate=https://cloudflare-dns.com/dns-query autoupgrade=yes udpfallback=yes >nul 2>&1
ipconfig /flushdns >nul

schtasks /create /tn "ErgunDPI" /tr "\"C:\ErgunDPI\ErgunDPI.exe\"" /sc onlogon /rl highest /f >nul
powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'ErgunDPI.lnk'));$s.TargetPath = 'schtasks.exe'; $s.Arguments = '/run /tn \"ErgunDPI\"'; $s.IconLocation = 'C:\ErgunDPI\ErgunDPI.exe,0'; $s.WindowStyle = 7; $s.Save()"

echo Kurulum tamam, baslatiliyor...
schtasks /run /tn "ErgunDPI" >nul
timeout /t 2 /nobreak >nul