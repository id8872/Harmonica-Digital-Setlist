# 🎵 Harmonica Digital Setlist

A smart, E-Ink based digital jukebox for harmonica tabs. This project runs on the **Seeed Studio reTerminal E1002** (ESP32) and reads song lyrics/tabs from an SD card. It features auto-scaling fonts, a wireless web editor, and ultra-low power deep sleep.

## ✨ Features

* **📄 E-Ink Display:** Crisp, high-contrast tabs that look like paper and are readable in sunlight.
* **🔠 Smart Auto-Scaling:** Automatically detects long lines of tabs and shrinks the font (Huge/Medium/Small) so they fit perfectly on screen.
* **💾 SD Card Storage:** Reads plain text files. Add as many songs as you want.
* **📡 Web Editor:** Connect to the device via Wi-Fi to edit tabs or upload new songs directly from your phone/laptop.
* **🔋 Deep Sleep:** Automatically turns off after 5 minutes of inactivity. Wakes up instantly to the same song when a button is pressed.
* **🔌 Offline Mode:** Works perfectly without Wi-Fi. If no network is found, it skips the web server and jumps straight to the tabs.
* **🚨 "Game Over" Mode:** If the SD card fails or is missing, it loads a fallback "Fail Riff" so you still have something to play!

## 🛠️ Hardware Requirements

* **Seeed Studio reTerminal E1002** (7.3" E-Ink Tablet with ESP32-S3)
* **MicroSD Card** (Formatted as FAT32)
* **USB-C Cable** (for power/programming)

## 📦 Software & Libraries

This project is built using the **Arduino IDE**. You will need to install the following libraries via the Library Manager:

1.  **GxEPD2** (by Jean-Marc Zingg) - *For the E-Ink display.*
2.  **Adafruit GFX Library** (by Adafruit) - *Graphics core.*
3.  **SD** (Built-in)
4.  **WebServer** (Built-in)
5.  **WiFi** (Built-in)

## 🚀 Setup Guide

### 1. SD Card Preparation
Format your SD card as **FAT32**. Create a folder and file exactly as shown below:

* Folder: `/HarmonicaTab/`
* File: `SETLIST.txt`

**Example content for `SETLIST.txt`:**

# Piano Man
5 6 6 6 6 6 6 -6 -6
It's nine o-clock on a sat-ur-day...

# Autumn Leaves
+2 -3 +4 -5  +3 -3 -4 +5

2. Wi-Fi ConfigurationCreate a new tab in your Arduino IDE named secrets.h and add your credentials:C++#ifndef SECRETS_H
#define SECRETS_H
#define SECRET_SSID "Your_WiFi_Name"
#define SECRET_PASS "Your_WiFi_Password"
#endif

3. UploadingOpen Harmonica.ino.
Select Board: "ESP32S3 Dev Module" (or specific reTerminal board definition).
Enable PSRAM in the Tools menu if available.
Upload the code.

🎮 Controls
Button                Action
Right Button (White)  Next Song / Wake Up
Left Button (Green)   Previous Song / Wake Up
Note: If the device is asleep (screen unchanged but unresponsive), pressing either button will wake it up and reload the current song.

🌐 Using the Web Editor
Turn on the device.
On the Splash Screen, note the IP Address (e.g., 192.168.1.50).
Open a browser on your phone or laptop and go to http://192.168.1.50.
You will see a text box with your current setlist.
Edit the text and click "Save & Update Device".
The device will instantly refresh to show your changes!

⚠️ Troubleshooting

"Game Over" Message
The device cannot read the SD card. Check that the card is inserted fully and formatted as FAT32. Ensure the folder is named /HarmonicaTab/ (case sensitive).

Wi-Fi Connects but says "Offline"
The router might be too far away, or the 10-second timeout expired. The device is still usable in offline mode.

Screen didn't refresh?
The device might be in Deep Sleep. Press a button to wake it.