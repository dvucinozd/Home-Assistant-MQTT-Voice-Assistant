# ESP32-P4 Voice Assistant - JC-ESP32P4-M3-DEV

**Lokalni AI Glasovni Asistent za Home Assistant na naprednoj ESP32-P4 platformi.**

Ovaj projekt predstavlja stabilan, produkcijski spreman firmware za **JC-ESP32P4-M3-DEV** razvojnu ploču. Firmware pretvara vaš ESP32-P4 u pametni zvučnik s podrškom za offline wake word detekciju, lokalno prepoznavanje naredbi i duboku integraciju s Home Assistant "Assist" sustavom.

---

## 🌟 Ključne Značajke

*   **🗣️ Napredni AI Audio sustav (ESP-SR AFE):**
    *   **Wake Word:** Lokalna detekcija "Hi ESP" (WakeNet).
    *   **AEC (Acoustic Echo Cancellation):** Softversko poništavanje jeke koje omogućuje "Barge-in" (prekidanje asistenta dok svira glazbu ili govori).
    *   **Noise Suppression & VAD:** AI-bazirano uklanjanje pozadinske buke i detekcija govora.
*   **⚡ Lokalno Prepoznavanje (MultiNet):** Prepoznavanje naredbi bez interneta (npr. "Turn on the light", "Play music").
*   **🏠 Home Assistant Integracija:**
    *   **Assist Pipeline:** WebSocket streaming za STT/TTS (Speech-to-Text / Text-to-Speech).
    *   **MQTT HA:** Kontrola uređaja, dijagnostika i status senzora izravno u Home Assistantu.
*   **🎵 Multimedija i Alarmi:**
    *   Lokalni MP3 player (podrška za SD karticu).
    *   Upravljanje alarmima i timerima spremljenim u NVS (rade i bez mreže).
*   **🛡️ Robustan Sustav:**
    *   **Safe Mode:** Automatska zaštita od boot-loopa (nakon 3 rušenja sustav ulazi u mod za oporavak).
    *   **Task Watchdog:** Monitor kritičnih procesa za automatski reset u slučaju blokiranja.
*   **⚙️ Web Sučelje:** Web-bazirani dashboard za konfiguraciju, nadzor sustava i pregled logova u stvarnom vremenu (WebSerial).

---

## 🔧 Hardverska Specifikacija

*   **MCU:** ESP32-P4 (Dual-core RISC-V @ 400MHz, 32MB PSRAM).
*   **WiFi:** ESP32-C6 (povezan preko SDIO sučelja).
*   **Audio Codec:** ES8311.
*   **LED Indikacija:** 
    *   Pins: Crvena (45), Zelena (46), Plava (47).
    *   Logic: **Active Low** (Common Anode) s LEDC PWM kontrolom svjetline.

---

## 🚀 Instalacija i Konfiguracija

### 1. Preduvjeti
*   Instaliran **ESP-IDF v5.5**.
*   Postavljen `PYTHONIOENCODING=utf-8` i `chcp 65001` (za Windows korisnike kako bi se izbjegle Unicode greške).

### 2. Konfiguracija
Kopirajte predložak konfiguracije:
```bash
cp main/config.h.example main/config.h
```
U `main/config.h` unesite svoje WiFi i Home Assistant (Token/URL) podatke.

### 3. Build i Flash
Sustav koristi prilagođenu particijsku tablicu (4MB za AI modele). Preporuča se brisanje flasha prije prvog snimanja:

```powershell
# Brisanje svega (preporučeno za prvi put)
idf.py erase-flash

# Build i Flash
.\build.bat
.\flash.bat
```

---

## 🛠️ Stabilizacija i Fixes (V2)

U verziji V2 implementirana su kritična poboljšanja stabilnosti:
1.  **AFE Fix:** Ispravljena inicijalizacija AFE sustava (postavljen "MR" mod za Mic/Reference) čime je riješen inicijalni boot-loop.
2.  **Stack Management:** Povećan stack size glavnog taska na **12KB** kako bi se spriječio Stack Overflow tijekom kompleksne inicijalizacije AI modela.
3.  **LED Hardware Integration:** Potpuno redefiniran `led_status.c` za rad s Active Low hardverom na JC ploči, koristeći preciznu LEDC PWM kontrolu.
4.  **Unicode Sanity:** Uklonjeni svi specijalni karakteri (emoji, strelice) iz logova koji su uzrokovali padanje Python monitor alata na Windows sustavima.
5.  **Netif Guard:** Dodana provjera postojanja mrežnog sučelja u `wifi_manager.c` kako bi se izbjegao fatalni pad sustava pri pokušaju ponovnog povezivanja.

---

## 📂 Struktura Projekta

*   `main/voice_pipeline.c` - Upravljanje stanjima asistenta (Slušanje, Obrada, TTS).
*   `main/audio_capture.c` - Audio ulaz, obrada šuma, AEC i AI detekcija.
*   `main/ha_client.c` - Komunikacija s Home Assistant WebSocket API-jem.
*   `main/led_status.c` - Vizualna signalizacija stanja (LEDC PWM).
*   `main/sys_diag.c` - Dijagnostika, Watchdog i Safe Mode logika.
*   `main/webserial.c` - HTTP poslužitelj za dashboard i logove.

---

## 🛡️ Sigurnost i Oporavak

### Safe Mode
Ako se uređaj sruši 3 puta unutar jedne minute, LED će početi bljeskati **CRVENO**. U ovom modu:
*   Audio podsustav je isključen (štedi memoriju i spriječava pad).
*   WiFi i HTTP server ostaju aktivni.
*   Korisnik može pristupiti Dashboardu na IP adresi uređaja i izvršiti **OTA ažuriranje** ili promijeniti postavke.

---

## 📝 Licence i Zasluge

Ovaj projekt koristi:
*   [ESP-SR](https://github.com/espressif/esp-sr) za AI obradu govora.
*   [ESP-IDF](https://github.com/espressif/esp-idf) framework.
*   [Home Assistant](https://www.home-assistant.io/) za pametno upravljanje domom.

**Autor:** Daniel  
**Asistent:** Gemini AI Agent  
**Datum:** Prosinac 2025.
