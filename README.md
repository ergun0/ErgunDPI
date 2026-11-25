# ErgunDPI v2.2

Windows için **C++17** ve **WinDivert 2.2** ile geliştirilmiş; Discord ve Roblox bağlantıları için düşük gecikmeli, hafif ve Direct2D arayüzlü DPI aşma aracı.

## Neden GoodbyeDPI Yerine ErgunDPI?

* **Hedefli Çekirdek Filtreleme (Genel Hız Kaybı Yok):** Tüm sistemin HTTP/HTTPS trafiğini kullanıcı alanına çekmek yerine yalnızca seçili uygulamaların (Discord ve Roblox) ilk TLS `ClientHello` el sıkışma paketlerini işler. Tarayıcı ve diğer uygulamalardaki genel internet ve indirme hızına müdahale etmez.
* **Oyun Dostu Düşük Gecikme:** CS2 gibi rekabetçi oyunların UDP paket akışına dokunmaz ve arka plandaki gereksiz paket işleme yükünü ortadan kaldırarak ağ dalgalanmalarını (net jitter) azaltmaya yardımcı olur.
* **QUIC / UDP 443 Engeli Yok:** UDP 443 trafiğini toplu engellemek yerine doğrudan TLS `ClientHello` bölme yaklaşımı kullanır.
* **Post-Quantum (Kyber / ML-KEM) & IPv6 Desteği:** Büyük boyutlu modern el sıkışma paketlerini ve IPv6 uzantı başlıklarını işleyecek şekilde tasarlanmıştır.
* **Akıcı Direct2D Arayüzü & Odak Koruması:** Donanım hızlandırmalı arayüz simge durumuna küçültüldüğünde sistem tepsisinde çalışır; pencere odak dışındayken gereksiz 60 FPS GPU çizimini durdurur.

## Kurulum

1. Sağ taraftaki **Releases** bölümünden son sürüm `.zip` dosyasını indirin ve bir klasöre çıkarın.
2. `Kurulum.bat` dosyasına sağ tıklayıp **Yönetici olarak çalıştır** deyin.
3. Kurulum dosyaları `C:\ErgunDPI` dizinine kopyalar, Cloudflare DNS (DoH) yapılandırır, IPv6'yı devre dışı bırakır, oturum açılış görevi oluşturur ve masaüstüne kısayol ekler.

> **Windows Defender Notu:** Çekirdek sürücüsü olan `WinDivert64.sys` güvenlik yazılımları tarafından yanlış pozitif olarak işaretlenebilir. Sorun yaşanırsa `C:\ErgunDPI` ve çalışma klasörünüzü Windows Güvenliği dışlamalarına ekleyebilirsiniz.

## Kaldırma

`Kaldir.bat` dosyasına sağ tıklayıp **Yönetici olarak çalıştır** dediğinizde `ErgunDPI.exe` kapatılır, `WinDivert` servisleri kaldırılır, `C:\ErgunDPI` klasörü ve başlangıç görevi silinir, masaüstü kısayolu kaldırılır ve DNS/IPv6 ayarları varsayılana döndürülür.

## Kaynak Koddan Derleme (MinGW-w64)

```powershell
windres resource.rc -O coff -o resource.res
g++ -O3 -std=c++17 -march=x86-64-v2 -flto -fno-exceptions -fno-rtti -s -municode -mwindows -static ErgunDPI.cpp dpi_engine.cpp resource.res -o ErgunDPI.exe -lws2_32 -lgdi32 -luser32 -lshell32 -ldwmapi -ld2d1 -ldwrite
```

## Sorumluluk Reddi (Disclaimer)

Bu yazılım eğitim, ağ analizi ve bağlantı kararlılığını test etme amacıyla sunulmaktadır. Yazılımın kullanımından doğabilecek ağ kesintileri, bağlantı sorunları, güvenlik yazılımı uyarıları veya sistem/ağ yapılandırma değişikliklerinden tamamen kullanıcı sorumludur. Lütfen yürürlükteki yerel yasalara ve ilgili hizmetlerin kullanım koşullarına uygun şekilde kullanın.