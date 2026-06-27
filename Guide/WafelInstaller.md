# Wafel Installer

During startup the Wafel Installer checks what is already installed and how the SD or USB is formatted. Depending on the current state, you will not see certain questions; in this case, skip to the next.



## Startup Checks

### SD not detected

- **Condition:** No SD card is detected at the hardware level. (`/dev/sdcard01`)

- **Prompt:**

  ```text
  No SD card detected.
  
  [ Retry SD ]
  [ Use USB (do NOT connect it just yet) ]
  [ Check NAND Health ]
  [ Abort ]
  ```

**If you want to use USB for homebrew**, this is expected. Do **NOT** connect the USB Device yet, just select USB. Only connect the USB device once it tells you to. The SD card will be emulated on USB device. If other guides say SD card, you will use the USB device instead.

**If you have a SD card inserted**, try replugging the SD or changing the Adapter in case of a Micro SD. The SD needs to be inserted into the front slot of the console (not via a USB Adapter). If you want to use a SD with a USB Adapter, it will be treated as any other USB storage device.
You can select Retry to try the detection again.

### USB / SD Format

- **Condition:** Shown when an SD card or USB is physically detected but is not MBR + FAT32 formatted or doesn't contain a `wiiu` folder yet.

- **Prompt:**

  ```text
  The SD / USB device isn't formatted for the Wii U.
  Do you want to format it for homebrew or also store Wii U games on it?
  
  [ Homebrew only ]
  [ Homebrew + Games ]
  [ Cancel ]
  ```

Homebrew needs a FAT32, which also your PC can read. The Wii U can only install games to and run them from a WFS formatted device. You have the choice to format the whole device FAT32, so it can be used for Homebrew, or you can split it in two parts: one FAT32 part for Homebrew and a WFS part to install the Wii U games to.



### Partition Size Selector

- **Condition:** Shown when the user chooses `Homebrew + Games` for a USB or SD device, or when choosing `Repartition`.

- **Prompt:**

  ```text
  FAT32:  [ 50%] ( XX.XX GB)    Homebrew and vWii USB Loader
  WFS:      [ 50%] ( YY.YY GB)    Wii U games and Virtual Console
  
  Use Left/Right to adjust (1% increments)
  Use Up/Down to adjust (10% increments)
  Press A to confirm, B to cancel
  ```

Here you can choose how much space you want to allocate for Wii U games. When you later dump your Discs, they will first be temporarily written to the FAT32 partition, before you install them to the WFS partition. So the FAT32 partition should be at least as big as your largest game. Also if you want to add mods to your games they will be stored here.
Your Wii (not Wii U) games will also be stored on the FAT32 partition.

If you don't have a large Wii library, ~25GB for the FAT32 partition is usually enough. But if you have enough space and plan on having multiple mod packs you should make it larger than that.

This can't be easily changed later, without wiping everything and starting over, so choose wisely.

**Note:** It is also possible to install your Wii games to the Wii U (WFS) partition using a PC tool called UWUVCI but it requires a lot more work.

```WARNING: This will RE-PARTITION the whole device
WARNING: This will RE-PARTITION the whole device
and DELETE ALL DATA on it.
Do you want to continue?

[ Yes ]
[ No ]
```

Once you confirmed the size selection, you have to confirm again that everything on the USB or SD will be deleted
You have to say `Yes` here.



### Aroma

- **Condition:** Shown when the Aroma environment directory (`wiiu/environments/aroma`) does not exist on the SD card.

- **Prompt:**

  ```text
  (Unofficial) Do you want to download Aroma by Maschell and the HB Appstore now?
   
  Note: Aroma plugins like Inkay (Pretendo) or FTP can be
  downloaded with the Aroma Updater later.
  
  [ Download ]
  [ Cancel ]
  ```

This will download the Aroma Base Environment `wiiu/environments/aroma` and default apps in `wiiu/apps` on the (emulated) SD. Also the Environment Loader is downloaded to `wiiu/payload.rpx` on the SD card.
This is the same as downloading the `EnvironmentLoader` Payload and `Base-Aroma` from [aroma.foryour.cafe](https://aroma.foryour.cafe) and extracting them on the SD.

The Homebrew App store and the Wafel Installer will also be installed to `wiiu/apps` on the (emulated) SD.
You will not have to use the Payloadloader Installer.

More aroma plugins can later be downloaded using the Aroma Updater app.
Other homebrew Apps can later be downloaded using the Homebrew App Store.



### Stroopwafel

- **Condition:** Shown when Stroopwafel is not running on the system or is too old for the IPC Interface.

- **Prompt:**

  ```text
  Stroopwafel is missing, outdated or not running
  Do you want to download stroopwafel by shinyquagsire23?
  
  [ Yes ]
  [ No ]
  ```

In this setup we will use Stroopwafel to load Aroma, so say `Yes` here. It is also needed for running homebrew from USB.
Stroopwafel Plugins can be managed with the Wafel Installer after the setup is finished.



```
Where do you want to download Stroopwafel?
SD card is recommended.

[ SD Card ]
[ SLC ]
[ Cancel ]
```

If you are using a real `SD Card`, it is recommended to choose SD card. In that case you have all the homebrew stuff on the SD, which makes troubleshooting easier.
But if you are using a USB device, then you need to select `SLC` as it can't be loaded from the emulated SD.

Stroopwafel consists of two main parts: 

- the minute bootloader
  - `fw.img` in the root of the SD
  - `sys/hax/fw.img` on the SLC
- The plugins
  - `wiiu/ios_plugins/*.ipx`on the SD
  - `sys/hax/ios_plugins/*.ipx` on the SLC

When installed to the SD, it will also have configuration in the `minute` folder on the SD.



### ISFShax

- **Condition:** Shown when ISFShax is not installed on the system (checks a memory location, so it also detects it in fallback mode).

- **Prompt:**

  ```text
  ISFShax is not detected.
  Do you want to install ISFShax by rw_r_r_0644?
  This is required for Stroopwafel.
  
  [ Yes ]
  [ No ]
  ```

ISFShax is a coldboot exploit that targets the bootloader (boot1) and so runs before the OS is even loaded. This gives you a very robust brick protection, if you have a functioning SD slot. Here it is used to load minute, which will then load Stroopwafel and its plugins, which then loads Aroma.

For the Installation it will temporarily download the ISFShax Installer and superblock image to `/sys/hax/installer` on the SLC. ISFShax itself gets installed to 4 of the superblocks of the SLC. It is not visible as a file.



```text
WARNING: You are about to make modifications to the console.
This software comes with ABSOLUTELY NO WARRANTY!
You are choosing to use this at your own risk.
The author(s) will not be held liable for any damage.
 
Do you want to proceed with Install?

[ Yes ]
[ No ]
```

Confirm the Install with `Yes`. The console will reboot into the ISFShax installer, install ISFShax and reboot again.



## First Boot

If everything worked, you will be greeted by the Environment Loader

### Environment Loader

Select `aroma`, then press `Y` to mark it as default and then `A` to load it.

### Boot Selector

Select `Wii U Menu`, then press `Y` to mark it as default and then `A` to load it.

### Blocking Updates

Press `X` to block updates.
Note: it is also fine to skip this with `B`. It is very unlikely that Nintendo will ever release another update for the Wii U and ISFShax is already preventing boot1 updates (so even if there was an update, ISFShax will continue to work)

### Formatting USB Storage

If you chose to partition the SD or USB for Homebrew and Wii U games, the Wii U will ask you if you want to format the connected USB storage device.

```
 You have connect a USB storage
device that has not been setup for
    use with this console.
 Do you want to format the device so
that it can be used with this console?

   (Cancel)        (Format)
```

Select `Format` . It will only format the part set aside for the Wii U games.

if you don't get that message, despite partitioning it for Wii U games, then do:

1. Launch the `System Settings` App from the Wii U Menu
2. Go to `Data Management` (second tab)
3. `Format USB Storage Device`
4. Press next until it is done formatting

## Summary

Here is a quick overview of what the installer just did:

### 1. Storage Preparation
If you chose to set up your SD card or USB drive for both homebrew and games, the installer created two separate sections (partitions) on it:
- A **FAT32 partition** for homebrew apps and Wii games.
- A **WFS partition** (which the Wii U formatted) exclusively for Wii U games.

### 2. Downloaded Components
The installer automatically downloaded the necessary files directly from GitHub and placed them in the correct spots for you. If you ever need to update or download these manually, you can find them here:
- **Aroma Environment & Apps** (info at [aroma.foryour.cafe](https://aroma.foryour.cafe)): Placed in the `wiiu` folder on your (emulated) SD card. This is the main homebrew environment.
- **Stroopwafel & minute** (info at [isfsh.ax](https://isfsh.ax)): Installed to either your SD card or the console's internal memory (SLC). These help launch Aroma smoothly.
- **ISFShax** (info at [isfsh.ax](https://isfsh.ax)): Installed directly into the console's low-level memory (SLC). This provides robust protection against bricking.

### 3. Autoboot Configuration
Finally, if you installed stroopwafel to your SD card, a small configuration file (`minute/minute.ini`) was created on your SD card. This tells your console to automatically start the correct option from minute on boot.

## Next Step

Continue on [Backups](SaveBackup.md)
