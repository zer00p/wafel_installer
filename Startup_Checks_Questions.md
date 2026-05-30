# Startup Check Questions

This document lists all the possible questions a user might be asked during the startup checks in the Wafel Installer, along with the condition that triggers each question.

## 1. Missing SD Card
- **Condition:** Shown when the application is launched, but the SD card is not accessible and no SD card is detected at the hardware level.
- **Prompt:**
  ```text
  No SD card detected.
  
  [ Retry SD ]
  [ Use USB ]
  [ Abort ]
  ```

## 2. Inaccessible SD Card
- **Condition:** Shown when an SD card is physically detected but cannot be mounted as a usable file system.
- **Prompt:**
  ```text
  The SD card isn't formatted for the Wii U.
  Do you want to format it for homebrew or also store Wii U games on it?
  
  [ Homebrew only ]
  [ Homebrew + Games ]
  [ Cancel ]
  ```
  *(Note: If the SD card is smaller than 2 GiB, it will only offer `[ Format (Homebrew only) ]` and `[ Cancel ]`)*

## 3. Inaccessible USB Device
- **Condition:** Shown when the user chose `Use USB` (after no SD card was detected), and a USB device is found but cannot be mounted.
- **Prompt:**
  ```text
  The USB device isn't formatted for the Wii U.
  Do you want to format it for homebrew or also store Wii U games on it?
  
  [ Homebrew only ]
  [ Homebrew + Games ]
  [ Cancel ]
  ```

## 4. Non-First FAT32 Partition
- **Condition:** Shown when a FAT32 partition exists on the device but is not the first partition in the MBR partition table.
- **Prompt:**
  ```text
  FAT32 partition found but it is not the first partition.
  This may cause issues with some homebrew.
  Do you want to fix the partition order or repartition?
  
  [ Fix order ]
  [ Repartition ]
  [ Cancel ]
  ```

## 5. New SD Card Setup
- **Condition:** Shown when the SD card mounts successfully, but it does not contain a `wiiu` directory, and its total size is at least 2 GiB.
- **Prompt:**
  ```text
  The SD card seems to be new (no 'wiiu' folder found).
  Do you want to format it for homebrew or also store Wii U games on it?
  
  [ Homebrew only ]
  [ Homebrew + Games ]
  [ Cancel ]
  ```

## 6. Reformat Non-Optimal SD Card
- **Condition:** Shown if the user chooses `Homebrew only` (to storing Wii U games) on a new SD card, but the SD card is not perfectly formatted as a single FAT32 partition spanning the whole drive.
- **Prompt:**
  ```text
  The SD card is not formatted to use the full space for homebrew [optional reason suffix].
  Do you want to reformat it to use the entire card or keep it as is?
  
  [ Format ]
  [ Keep ]
  [ Cancel ]
  ```
  *(Suffix can be " (it has multiple partitions).", " (unknown partition type: ...).", or ".")*

## 7. Storage Partitioning Selection
- **Condition:** Shown when the user chooses `Homebrew + Games` for an unmountable USB device, when they choose `Homebrew + Games` on a new SD card, or when accessing the partition action menu manually.
- **Prompt:**
  ```text
  How do you want to partition the [deviceTypeName]?
  
  [ Keep current partitioning ]
  [ Create additional Wii U partition ]
  [ Repartition ]
  [ Cancel ]
  ```
  *(Where `deviceTypeName` is typically "USB device" or "SD card". Options are dynamic based on available space and existing partitions.)*

## 8. Partition Size Selector
- **Condition:** Shown when the user chooses `Repartition` or `Create additional Wii U partition` from the storage partitioning selection menu.
- **Prompt:**
  ```text
  FAT32:  [ 50%] ( XX.XX GB)    Homebrew and vWii USB Loader
  WFS:      [ 50%] ( YY.YY GB)    Wii U games and VC
  
  Use Left/Right to adjust (1% increments)
  Use Up/Down to adjust (10% increments)
  Press A to confirm, B to cancel
  ```

## 9. Format Warning
- **Condition:** Shown as a final confirmation before the application completely formats a device (e.g. when formatting an inaccessible SD card or reformatting an SD card to use full space).
- **Prompt:**
  ```text
  WARNING: This will format the whole device and DELETE ALL DATA on it.
  Do you want to continue?
  
  [ Yes ]
  [ No ]
  ```

## 10. Repartition Warning
- **Condition:** Shown as a final confirmation after the user has configured partition sizes to partition the device for both homebrew and Wii U games.
- **Prompt:**
  ```text
  WARNING: This will RE-PARTITION the whole device
  and DELETE ALL DATA on it.
  Do you want to continue?
  
  [ Yes ]
  [ No ]
  ```

## 11. Unconfigured Wii U Partition on SD Card
- **Condition:** Shown when the SD card has a `wiiu` folder, contains a WFS (Wii U) partition in its MBR, but the required partition plugins (`5upartsd.ipx` or `5sdusb.ipx`) are not present.
- **Prompt:**
  ```text
  A Wii U or NTFS partition was detected on the SD card.
  Do you want to use it to store Wii U games?
  
  [ Yes ]
  [ No ]
  ```

## 12. Aroma Installation
- **Condition:** Shown when the SD card is mounted successfully, but the Aroma environment directory (`wiiu/environments/aroma`) does not exist on the SD card.
- **Prompt:**
  ```text
  (Unofficial) Do you want to download Aroma by Maschell and the HB Appstore now?
   
  Note: Aroma plugins like Inkay (Pretendo) or FTP can be
  downloaded with the Aroma Updater later.
  
  [ Download ]
  [ Cancel ]
  ```

## 13. Stroopwafel Check
- **Condition:** Shown when the Stroopwafel plugin path is empty, its directory does not exist, or Stroopwafel is not running on the system.
- **Prompt:**
  ```text
  Stroopwafel is missing, outdated or not running
  Do you want to download stroopwafel by shinyquagsire23?
  
  [ Yes ]
  [ No ]
  ```

## 14. ISFShax Check
- **Condition:** Shown when ISFShax is not installed on the system.
- **Prompt:**
  ```text
  ISFShax is not detected.
  Do you want to install ISFShax by rw_r_r_0644?
  This is required for Stroopwafel.
  
  [ Yes ]
  [ No ]
  ```

## 15. Incompatible Plugin Deletion
- **Condition:** Shown if a downloaded plugin during the post-setup checks is found to be incompatible with another already installed plugin.
- **Prompt:**
  ```text
  Warning: [incompatibleFile] is already installed and is incompatible with [pluginName]!
  Do you want to delete the incompatible plugin first?
  
  [ Delete ]
  [ Keep both ]
  [ Cancel ]
  ```
