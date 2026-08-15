# Modding the vWii

This section will show you how to mod your Virtual Wii (vWii) with the **Homebrew Channel**, **d2x cIOS**, **USB Loader GX**, and the **Open Shop Channel**.

This section assumes you have already completed the initial setup with the Wafel Installer. If your console is not modded yet, [get started](GettingStarted.md).

::: {.callout-note}
**SD Card Emulation:** If you set up Wii U homebrew to run from USB via SD emulation, the SD emulation will not apply inside vWii mode. vWii applications will see no physical SD card inserted, but will access the FAT32 partition on your USB drive directly. This works seamlessly because most Wii homebrew supports USB storage, with two notable exceptions: **CTGP-R** and **Retro Revolution** (which strictly require a physical SD card).
:::

## Preparation

- If you plan to use a USB storage device for storing Wii games, connect it to **USB Port 0** (the upper USB port on the back of the console). If you use a USB Y-cable, ensure the main data connector (typically the middle/thicker cable) is connected to Port 0.
- Make sure the date and time on your console are set correctly. [Detailed Instructions](https://en-americas-support.nintendo.com/app/answers/detail/a_id/1079/~/how-to-change-the-date-and-time-on-wii-u)

## vWii Compat Title Installer

The **vWii Compat Title Installer** simplifies vWii modding by allowing you to install necessary channels, cIOS, and homebrew tools directly from Wii U mode.

### Installing vWii Compat Title Installer

1. Open the **HB App Store** from the Wii U Menu.
2. Search for `Compat` and select **vWii Compat Title Installer** by *DaThinkingChair*.
3. Press `A` to download.
4. Exit the HB App Store.

### Installing Homebrew via Express Setup

1. Launch the **vWii Compat Installer** app from the Wii U Menu.
2. Select **Express Setup**.
3. Leave all components checked (or toggle individual options with `A` if desired).
4. Press `+` to proceed with the installation of the selected components.
5. Select `Yes, start installation`.
6. Once the installation finishes, review the summary and press any button to return to the main menu. Press `HOME` to exit the installer.

**What Express Setup installs:**

- **Homebrew Channel:** Allows launching vWii homebrew applications.
- **d2x cIOS:** Custom IOS (installed to slots 248, 249, 250, and 251) required for USB loaders to run games from SD / USB.
- **IOS80 Patches:** Trucha, ES, and FS patches that allow running custom channels and forwarders from the SD card / System Menu.
- **USB Loader GX:** Back up your Wii game discs and play backups directly from storage.
  - Installs the homebrew application to `sd:/apps/usbloader_gx`.
  - Installs the forwarder channel (`UNEO`) to the vWii System Menu.
  - Installs the Aroma forwarder app (`USB-Loader-GX-UNEO.wuhb`) to the Wii U Menu.
- **Open Shop Channel:** Downloads **LibreShop** and **Homebrew Browser** to download additional Wii homebrew apps directly on the console.

## USB Loader GX

USB Loader GX (ULGX) can dump your Wii game discs to the FAT32 partition on either your SD card or USB storage device. The games can then be launched directly from USB Loader GX without further installation.

### Launching USB Loader GX

You can launch USB Loader GX in three ways:

- From the **Wii U Menu** by selecting the **USB Loader GX** forwarder icon.
- From the **vWii System Menu** by selecting the **USB Loader GX** channel.
- From the **Homebrew Channel** within the vWii Menu.

::: {.callout-note}

- **Using USB Storage:** USB Loader GX will automatically detect your drive and load directly into the game menu. If your USB drive is not detected, ensure it is connected to the top rear USB port (Port 0).
- **Using an SD Card:** If you do not have a USB drive connected, USB Loader GX will wait for a USB device with a 20-second countdown. Press `B` on your Wii Remote while it is waiting to switch to SD card mode.
  :::

### Basic Navigation & Settings

The bottom navigation bar provides essential controls:

- **SD Mode Toggle (leftmost icon):** Switch between SD card and USB drive storage modes.
- **Settings (gears icon):** Configure USB Loader GX settings.
  - *Recommended setting:* Go to `Settings` &rarr; `Loader Settings` &rarr; `Return To` and select `UNEO` (or `UNEO USB Loader GX`). When you exit a Wii game, the console will return straight to USB Loader GX instead of the vWii System Menu.
- **Power (standby icon):** Turn off the console.
- **View Display Mode:** Change how games are displayed (List View, Grid View, Carousel / CoverFlow, or Channel View). Channel View imitates the classic Wii Menu channels and displays animated banners without requiring downloaded 2D/3D cover art.
- **Game Source Filter:** Toggle visibility of different game types (Wii, GameCube, and NAND channels). If you do not have custom WiiWare channels installed, you can uncheck "NAND" to keep the game list clean.
- **Homebrew Launcher (TV icon in middle bar):** Launch vWii homebrew applications directly from USB Loader GX.

### Installing (Dumping) Games

1. Insert your Wii game disc into the console disc drive (or press the `+` button in the toolbar).
2. Select **Install** and confirm with **OK**.
3. Repeat for any other Wii game discs you want to dump.
4. Press `1` on your Wii Remote to automatically download box art and disc banners from GameTDB.

### Launching Games

Select the game you wish to play, then click **Start**.

## Installing Additional Homebrew Apps

Additional Wii homebrew applications can be downloaded and installed directly on your console using the [Open Shop Channel](https://oscwii.org/) (OSC). Both OSC clients can be launched from the **Homebrew Channel** on the vWii System Menu:

- **LibreShop:** A modern, text-based client for the Open Shop Channel. It is fast, lightweight, and very stable.
- **Homebrew Browser:** The classic graphical browser for Wii homebrew. Originally developed in 2008, it provides visual browsing and screenshots, but can be slower and less stable on modern Wi-Fi networks.
