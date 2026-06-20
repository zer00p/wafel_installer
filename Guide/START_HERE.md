# Easy Wii U Homebrew Guide

This will help you to fully mod your Wii U. 
This will install:

- [**ISFShax:**](isfsh.ax) a coldboot exploit, that acts as brick protection and is needed to use Stroopwafel
- [**Stroopwafel:**](https://github.com/StroopwafelCFW/stroopwafel) It allows patching of the system early during boot. It is used here also to load Aroma
- [**Aroma:**](https://aroma.foryour.cafe/) This allows you to run Homebrew and use Plugins like Pretendo or FTP 



## Disclaimer

Everything in this Guide has been tested and has been designed to be safe. However, you are doing this at your own risk.

## What you need

- A Wii U Console
- The Gamepad Controller
- An Internet connection on the Wii U
- Either A SD card **OR** USB Storage Device

*Note*: If the USB Device already has Wii U games on it and you choose to use it for homebrew, then anything on it will be deleted.
You can also use both, a SD for homebrew and vWii and keep the USB for Wii U games.
If you want to use the SD card for Wii U games, make sure to get a reliable one, since it will also store the saves.
I recommend against using a USB flash drive for storing Wii U games. Even from reputable brands they are often unreliable (but they are good enough for storing homebrew only)



## Preparation

### Date and Time

Make sure the Date and Time is set to the current time. [Detailed Instructions](https://en-americas-support.nintendo.com/app/answers/detail/a_id/1079/~/how-to-change-the-date-and-time-on-wii-u)

### Internet

Make sure your Wii U is connected to the Internet. [Detailed Instructions](https://en-americas-support.nintendo.com/app/answers/detail/a_id/1126/~/how-to-connect-a-wii-u-console-to-the-internet)

If you set a custom DNS (to block updates for example) revert the DNS setting to Auto.

### Update

All 5.5.x Firmwares should work, but to avoid potential incompatibilities I strongly recommend updating to the latest Firmware (5.5.5E / 5.5.5J / 5.5.6U). [Detailed Instructions](https://en-americas-support.nintendo.com/app/answers/detail/a_id/1136/~/how-to-perform-a-system-update-on-wii-u)

**Warning:** Do not attempt to update, if you saw 160-0103 "System Memory Errors" in the past.

## Running Wafel Installer

If you are already running Aroma or Tiramisu go to Option B.

If the console is stock (unmodded) continue with Option A.

**Note:** if you booted without SD or the SD is new / empty consider it as stock and continue with Option A, even if the Payloadloader is already installed.

### Option A: Stock Console

1. Disconnect any USB Storage device before turning on the Wii U
2. Open the Internet Browser Applet on the Wii U
3. Go to the address bar, right at the top (NOT the search)
4. Enter `https://wafel.xyz`
5. Press OK. You should now see the Wafel Installer web page
6. Tap on launch
7. Tap on launch again. After a few seconds the wafel installer app should load. If it freezes for 3 Minutes, restart and try again.
8. Continue to [Wafel Installer](WafelInstaller.md)

### Option B: Modded Console

1. Download the Wafel Installer from [GitHub](https://github.com/zer00p/wafel_installer/releases). If you are running Aroma download `wafel_installer.wuhb` , for Tiramisu use `wafel_installer.rpx`
2. Copy the Wafel Installer to `wiiu/apps/` on the SD card
3. Safety Eject the SD card
4. Put the SD back in the Wii U
5. Run the Wafel Installer Homebrew App
6. Continue to [Wafel Installer](WafelInstaller.md)

