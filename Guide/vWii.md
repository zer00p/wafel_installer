# Modding the vWii

This will show you how to mod your vWii with the **Homebrew Channel**, **d2x cIOS**, **USB Loader GX** and the **Open Shop Channel**.

**Note:** if you set up Wii U homebrew to run from USB via SD emulation, the SD emulation will not apply inside vWii. vWii Apps will see no SD in that case, but as USB. This is fine Since most Wii Hombrew supports USB already, with two notable exception CTGP-R and Retro Revolution (these require a real SD card). 

## Preparation

- If you plan to use a USB Device for storing Wii Games, connet it to port 0. This is the upper port on the back of the console. If you use a Y-splitter cable, make sure the connector in the middle of the cable is connected to port 0, as that one is for data.

- Make sure the date is set correctly. [Detailed Instructions](https://en-americas-support.nintendo.com/app/answers/detail/a_id/1079/~/how-to-change-the-date-and-time-on-wii-u)

## vWii Compat Title Installer

### Installing vWii Compat Title Installer

1. Open the`HB App Store` App from the Wii U Menu
2. *Search* for `Compat` and select `vWii Compat Title Installer` by `DaThinkingChair`
3. Press `A` to Download
4. Exit the HB App Store

### Installing Homebrew

1. Launch the `vWii Compat Installer` App from the Wii U Menu
2. Select Express Setup
3. If you are not sure, leave everything checked
4. Press + to select the installation of the selected option.
5. `Yes, start instalation`

This will install:

- Homebrew Channel: Launches vWii Homebrew from the vWii Menu
- d2x cIOS: Needed for USB Loaders to work
- IOS80 Patches: Launch fake signed Apps from the SD menu
- USB Loader GX: Backup your Wii Discs and Play the Backups
  - Also includes Forwarders for the Wii U and vWii Menu
- Open Shop Channel: Download additional Wii Homebrew via Libre Shop or Homebrew Browser



## USB Loader GX

USB Loader GX (ULGX)can dump your Wii Games to the FAT32 Partition on either the SD card or USB storage. The games can then be launched directly from ULGX without further installation.

### Launching ULGX

You can launch ULGX from the Wii U Menu or the vWii Menu by launching the USB Loader GX Forwarder / App channel. You can also launch it from the Homebrew Channel in the vWii Menu, like other Homebrew apps.

If you are using a USB storage device, it should launch directly into the USB Loader GX menu.
If you are using a SD you need to press B one the Wii Mote, while it is waiting for the USB HDD.

If your USB storage isn't detected, make sure it is plugged into the correct USB port. (The upper one at the back)

### Basic Navigation

Going through the Controls at the bottom navigation bar from left to right:

- SD Mode Toggle: If you accidentally switched to SD mode, you can swtich back to USB and vice versa
- Settings: Here you can configure Settings of the USB Loader
  - Optional: In the `Loader Settings` I recommend to configure the `Return to` to `UNEO USB Loader GX`. This way the USB Loader will load again when you exit a game instead of the vWii System Menu.
- Shutdown: Turns off the console
- View: How the Games are Displayed. I prefer the Channel View, as it imitates the Wii Menu and doesn't need extra cover art, but you are free to chose what you like
- Game Source Switch: Cylce through the different Game Types: Wii, NAND (Installed channels), Gamecube
- Game Source Menu: Here you can select what Game Types you want to be shown. If you don't have Wii Ware, I recommend turning off "NAND".

You can also launch Homebrew Apps, by pressing the TV Homebrew Channel Icon in the bar in the middle.

### Installing Games

1. Instert the Disc **or** press the `+` button in the middle bar
2. Select Install
3. Repeat for all your games
4. Press `1` on the Wii Mote to download Covers



### Launching Games

Just click on the Game you want to launch



## Installing Additional Hombrew Apps



