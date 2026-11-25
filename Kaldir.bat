@echo off
chcp 65001 >nul
title ErgunDPI Kaldir

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Yonetici olarak calistirman lazim.
    pause
    exit /b 1
)

echo ErgunDPI dosyalari, baslangic gorevi, kisayol ve DNS/IPv6 ayarlari sifirlanacak.
set /p secim="Kaldirilsin mi? (E/H): "
if /I not "%secim%"=="E" exit /b 0

taskkill /f /im ErgunDPI.exe >nul 2>&1
sc stop WinDivert >nul 2>&1
sc delete WinDivert >nul 2>&1
sc stop WinDivert1.4 >nul 2>&1
sc delete WinDivert1.4 >nul 2>&1
timeout /t 1 /nobreak >nul

if exist "C:\ErgunDPI" rd /s /q "C:\ErgunDPI"
schtasks /delete /tn "ErgunDPI" /f >nul 2>&1

powershell -NoProfile -Command "$lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'ErgunDPI.lnk'; if (Test-Path $lnk) { Remove-Item $lnk -Force }"

netsh dns delete encryption server=1.1.1.1 >nul 2>&1
netsh dns delete encryption server=1.0.0.1 >nul 2>&1
powershell -NoProfile -Command "Get-NetAdapter | Where-Object Status -eq 'Up' | ForEach-Object { Set-DnsClientServerAddress -InterfaceIndex $_.ifIndex -ResetServerAddresses -ErrorAction SilentlyContinue; Enable-NetAdapterBinding -Name $_.Name -ComponentID ms_tcpip6 -Confirm:$false -ErrorAction SilentlyContinue }"
ipconfig /flushdns >nul

echo Kaldirma bitti, tum ayarlar eski haline dondu.
pause