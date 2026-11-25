# ErgunDPI v2.2

Windows için C++17 ve WinDivert 2.2 ile geliştirilmiş, Discord ve Roblox odaklı bağlantı aracı.

---

## Neden GoodbyeDPI Yerine ErgunDPI?

Teknik ayrıntılara girmeden özetlemek gerekirse; bazı araçlar tüm sistem trafiğini işlerken ErgunDPI yalnızca seçili uygulamaların (Discord ve Roblox) ilk bağlantı paketlerine odaklanır:

* **Genel ortalamada ~%25–%35 daha yüksek verimlilik/akıcılık:** Tüm internet trafiğini kullanıcı alanından geçirmeden yalnızca hedef bağlantılarda işlem yaptığı için tarayıcı hızını, indirmeleri ve genel sistem akıcılığını korur.
* **Oyunlarda daha kararlı deneyim:** CS2 gibi oyunların UDP paket akışına dokunmaz ve arka planda gereksiz paket yükü oluşturmadığı için ağ dalgalanmalarını (net jitter) artırmaz.
* **UDP 443 / QUIC engeli olmadan çalışma:** Toplu port/protokol engellemek yerine seçili hedeflerde bağlantı başlangıcını bölmeye odaklanır.
* **Düşük sistem ve GPU tüketimi:** Simge durumuna küçültüldüğünde sistem tepsisinde çalışır; pencere arka plandayken gereksiz 60 FPS arayüz çizimini durdurur.
* **Hedef seçimi:** Discord ve Roblox seçeneklerini bağımsız olarak açıp kapatabilirsin.

## Kurulum

1. **Releases** sekmesinden son sürüm `.zip` dosyasını indir ve bir klasöre çıkar.
2. `Kurulum.bat` dosyasına sağ tıklayıp **Yönetici olarak çalıştır** de.
3. Dosyalar `C:\ErgunDPI` klasörüne kopyalanır, Cloudflare DNS (DoH) ayarlanır, IPv6 kapatılır, başlangıç görevi oluşturulur ve masaüstüne kısayol eklenir.

> **Güvenlik yazılımı notu:** Çekirdek sürücüsü (`WinDivert64.sys`) kullanan araçlar Windows Defender veya diğer güvenlik yazılımlarında yanlış pozitif uyarı üretebilir. Gerekirse `C:\ErgunDPI` ve çalışma klasörünü dışlamalara ekleyebilirsin.

## Kaldırma

`Kaldir.bat` dosyasına sağ tıklayıp **Yönetici olarak çalıştır** dediğinde uygulama durdurulur, sürücü servisleri silinir, `C:\ErgunDPI` klasörü, başlangıç görevi ve masaüstü kısayolu kaldırılır, DNS ve IPv6 ayarları varsayılana döndürülür.

## Derleme (MinGW-w64)

```powershell
windres resource.rc -O coff -o resource.res
g++ -O3 -std=c++17 -march=x86-64-v2 -flto -fno-exceptions -fno-rtti -s -municode -mwindows -static ErgunDPI.cpp dpi_engine.cpp resource.res -o ErgunDPI.exe -lws2_32 -lgdi32 -luser32 -lshell32 -ldwmapi -ld2d1 -ldwrite
```

## Sorumluluk Reddi

Bu yazılım yalnızca eğitim, test ve bağlantı davranışını inceleme amacıyla sunulmaktadır. Yazılımın indirilmesi, kurulması veya kullanılması sonucunda ortaya çıkabilecek ağ kesintileri, bağlantı hataları, hesap veya hizmet kısıtlamaları, güvenlik yazılımı uyarıları ya da sistem ve ağ yapılandırma değişikliklerinden doğacak tüm sorumluluk kullanıcıya aittir. Bu aracın kullanımında yürürlükteki yerel yasalara ve ilgili hizmetlerin kullanım koşullarına uymak kullanıcının yükümlülüğündedir. Yazılım "olduğu gibi" (as-is) sunulmaktadır ve kesintisiz çalışacağına dair garanti verilmez.

---

# ErgunDPI v2.2 (English)

A C++17 and WinDivert 2.2 based network utility for Windows focused on Discord and Roblox.

## Why ErgunDPI Over GoodbyeDPI?

In simple terms, while some tools inspect or route traffic broadly across the system, ErgunDPI targets only the initial connection packets of selected apps (Discord and Roblox):

* **~25–35% better overall efficiency/responsiveness on average:** Leaves general browsing, downloads, and unrelated traffic untouched instead of pushing everything through extra processing.
* **Game-friendly stability:** Does not interfere with game traffic (such as CS2) and avoids unnecessary background packet overhead that can worsen network jitter.
* **No broad UDP 443 / QUIC blocking:** Focuses on splitting initial handshake packets for selected targets rather than broadly blocking UDP 443.
* **Low overhead & focus awareness:** Minimizes to the system tray and stops unnecessary 60 FPS UI redraws when the window is not in focus.
* **Independent toggles:** Discord and Roblox can be toggled separately.

## Installation

1. Download the latest `.zip` from **Releases** and extract it to a folder.
2. Right-click `Kurulum.bat` and select **Run as administrator**.
3. Files are copied to `C:\ErgunDPI`, Cloudflare DNS (DoH) is configured, IPv6 is disabled, a logon task is created, and a Desktop shortcut is added.

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
