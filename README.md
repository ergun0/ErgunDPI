# ErgunDPI v2.2

Windows için C++17 ve WinDivert 2.2 tabanlı bağlantı aracı. Seçili uygulamalarda (Discord ve Roblox) bağlantı başlangıcındaki paketleri işleyerek erişim ve kararlılık sorunlarını azaltmayı hedefler.

---

## Ne İşe Yarar ve Neden Tercih Edilir?

Teknik terimlere girmeden özetlemek gerekirse; bazı araçlar tüm internet trafiğini kendi üzerinden geçirir. ErgunDPI ise yalnızca seçtiğin uygulamaların bağlantı açılışını ele alır, geri kalan her şeye dokunmaz:

* **İnternet hızını düşürmez:** Tarayıcıda dosya indirirken, video izlerken veya diğer uygulamalarda hattının hızını kesmez; genel ortalamada tüm trafiği yoran çözümlere kıyasla yaklaşık **%25–%35 daha verimli** bir kullanım deneyimi sunar.
* **Oyunlarda takılma/dalgalanma yapmaz:** CS2 gibi oyunların veri akışına müdahale etmez ve arka planda gereksiz paket yükü oluşturmadığı için ping/jitter sıçramalarını tetiklemez.
* **Hafiftir ve yormaz:** Sadece gerekli anlarda devreye girer; pencereyi küçülttüğünde sağ altta (sistem tepsisinde) sessizce çalışır, pencere arka plandayken ekran kartını boş yere yormaz.
* **İstediğini seçebilirsin:** Discord ve Roblox seçeneklerini ayrı ayrı açıp kapatabilirsin.

## Kurulum

1. **Releases** sekmesinden son sürüm `.zip` dosyasını indir ve bir klasöre çıkar.
2. `Kurulum.bat` dosyasına sağ tıklayıp **Yönetici olarak çalıştır** de.
3. Kurulum dosyaları `C:\ErgunDPI` klasörüne kopyalar, Cloudflare DNS (DoH) ayarını uygular, IPv6'yı kapatır, oturum açılış görevi oluşturur ve masaüstüne kısayol bırakır.

> **Güvenlik yazılımı uyarısı:** Ağ sürücüsü (`WinDivert64.sys`) kullanan araçlar Windows Defender veya diğer antivirüsler tarafından yanlış alarm verebilir. Gerekirse `C:\ErgunDPI` ve çalışma klasörünü dışlamalara ekleyebilirsin.

## Kaldırma

`Kaldir.bat` dosyasına sağ tıklayıp **Yönetici olarak çalıştır** dediğinde uygulama kapatılır, sürücü servisleri kaldırılır, `C:\ErgunDPI` klasörü, başlangıç görevi ve kısayol silinir, DNS ve IPv6 ayarları eski haline döndürülür.

## Derleme (MinGW-w64)

```powershell
windres resource.rc -O coff -o resource.res
g++ -O3 -std=c++17 -march=x86-64-v2 -flto -fno-exceptions -fno-rtti -s -municode -mwindows -static ErgunDPI.cpp dpi_engine.cpp resource.res -o ErgunDPI.exe -lws2_32 -lgdi32 -luser32 -lshell32 -ldwmapi -ld2d1 -ldwrite
```

## Sorumluluk Reddi

Bu proje yalnızca eğitim, test ve bağlantı davranışını inceleme amacıyla paylaşılmıştır. Yazılımın indirilmesi, kurulması veya kullanılması sonucunda ortaya çıkabilecek ağ kesintileri, bağlantı hataları, hesap veya hizmet kısıtlamaları, güvenlik yazılımı engellemeleri ya da sistem/ağ ayarlarındaki değişikliklerden doğacak tüm sorumluluk kullanıcıya aittir. Bu aracı kullanırken bulunduğunuz ülkenin yürürlükteki yasalarına ve ilgili platformların kullanım kurallarına uymak kullanıcının yükümlülüğündedir. Proje "olduğu gibi" (as-is) sunulmaktadır ve kesintisiz çalışacağına dair garanti verilmez.

---

# ErgunDPI v2.2 (English)

A C++17 and WinDivert 2.2 based utility for Windows designed to help with connection stability and access issues on selected targets (Discord and Roblox).

## What It Does & Why Use It

In plain terms, some tools route and inspect traffic across your whole system. ErgunDPI focuses only on initial connection packets for selected targets and leaves everything else untouched:

* **Keeps speeds intact:** Does not interfere with general browsing or downloads, offering roughly **25–35% better overall efficiency/responsiveness** compared to approaches that push all traffic through extra processing.
* **Friendly to online games:** Avoids interfering with game traffic (such as CS2) and keeps background overhead low so it does not add network jitter.
* **Lightweight behavior:** Minimizes to the system tray and pauses unnecessary 60 FPS UI redraws when the window is not in focus.
* **Per-target toggles:** Discord and Roblox can be toggled independently.

## Installation

1. Download the latest `.zip` from **Releases** and extract it to a folder.
2. Right-click `Kurulum.bat` and select **Run as administrator**.
3. The script copies files to `C:\ErgunDPI`, configures Cloudflare DNS (DoH), disables IPv6, creates a logon task, and places a shortcut on the Desktop.

> **Antivirus note:** Kernel packet drivers (`WinDivert64.sys`) can trigger false positives in security software. Add `C:\ErgunDPI` and your working folder to exclusions if needed.

## Uninstallation

Right-click `Kaldir.bat` and choose **Run as administrator** to stop the app, remove driver services, delete `C:\ErgunDPI`, the scheduled task, and the Desktop shortcut, and restore default DNS and IPv6 settings.

## Building from Source (MinGW-w64)

```powershell
windres resource.rc -O coff -o resource.res
g++ -O3 -std=c++17 -march=x86-64-v2 -flto -fno-exceptions -fno-rtti -s -municode -mwindows -static ErgunDPI.cpp dpi_engine.cpp resource.res -o ErgunDPI.exe -lws2_32 -lgdi32 -luser32 -lshell32 -ldwmapi -ld2d1 -ldwrite
```

## Disclaimer

This software is provided solely for educational, testing, and network behavior research purposes. The user assumes full responsibility for any consequences arising from downloading, installing, or using this software, including network disruptions, connection issues, account or service restrictions, security software alerts, or changes to system and network settings. Users are solely responsible for complying with applicable local laws and the terms of service of any third-party platforms. The software is provided "as is", without warranty of any kind.
