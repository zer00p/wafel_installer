<div style="text-align: center;">
    <img src="dist/wafel_installer-logo.png" alt="Wafel Installer Logo" style="width: 80px;" />
    <h3>Wafel Installer</h3>
    <p>Wafel Installer is a tool for the Wii U designed to be launched via the browser (at <a href="https://wafel.xyz">wafel.xyz</a>) or directly from Aroma. It assists in preparing storage devices by formatting and partitioning SD cards or USB drives to support both homebrew and game storage. The application automates the setup of Aroma, Stroopwafel CFW, and the ISFShax exploit.</p>
</div>

## Features
- **CFW Setup**: Installs Stroopwafel CFW, ISFShax, and the Aroma environment.
- **Storage Preparation**: Formats and partitions SD cards and USB drives, supporting FAT32 on devices larger than 32GB.
- **Plugin Management**: Downloads, syncs, and manages Stroopwafel plugins across SD and SLC (internal storage).
- **Advanced Storage Options**: Configures SDUSB or USB Partition.
- **Boot Configuration**: Edits autoboot and timeout settings for the `minute` bootloader.
- **No PC required**
- **No SD required** can use USB instead.

## How to use
**Method 1:**  
Use [wafel.xyz](https://wafel.xyz) on your Wii U to launch Wafel Installer without any setup or SD card.

**Method 2:**
Use the Wii U App Store to download it. See Dumpling's page [here](https://apps.fortheusers.org/wiiu/wafel_installer). (coming soon)

**Method 3:**  
Download the [latest release from GitHub](https://github.com/zer00p/wafel_installer/releases), and extract the `wafel_installer.zip` file to the root of your SD card.

## How to compile
 - Install [DevkitPro](https://devkitpro.org/wiki/Getting_Started) for your platform.
 - Install xxd and zip if you don't have it already through your Linux package manager (or something equivalent for msys2 on Windows).
 - Install [wut](https://github.com/devkitpro/wut) through DevkitPro's pacman or compile (and install) the latest source yourself.
 - Compile [libmocha](https://github.com/wiiu-env/libmocha).
 - Then, with all those dependencies installed, you can just run `make` to get the .rpx file that you can run on your Wii U.


## Credits
 - [Crementif](https://github.com/Crementif) for the original [dumpling](https://github.com/dumpling-app/dumpling)
 - [emiyl](https://github.com/emiyl) for [dumpling-classic](https://github.com/emiyl/dumpling-classic)
 - [zer00p](https://github.com/zer00p) for Wafel Installer
 - chriz, Tomk007 and Jaimie for testing (dumpling)
 - [wut](https://github.com/devkitpro/wut) for providing the Wii U toolchain that Wafel Installer is built with
 - [Dimok](https://github.com/dimok789) for the original fw_img_payload.
 - The [wiiu-env](https://github.com/wiiu-env) developers (especially [Maschell](https://github.com/Maschell)) for Aroma.
 - [shinyquagsire23](https://github.com/shinyquagsire23) for stroopwafel and fw_img_loader additions.
 - [rw_r_r_0644](https://github.com/rw_r_r_0644) for ISFShax.
 - [Maschell](https://github.com/Maschell) for Aroma and especially the [AromaUpdater](https://github.com/wiiu-env/AromaUpdater) for code and inspiration
 - The [fortheusers](https://github.com/fortheusers) team for the Homebrew Appstore.
 - [FIX94](https://github.com/FIX94), [Maschell](https://github.com/Maschell), [Quarky](https://github.com/ashquarky), [GaryOderNichts](https://github.com/GaryOderNichts) and [koolkdev](https://github.com/koolkdev) for making and maintaining homebrew (libraries)
 - [smea](https://github.com/smealum), [plutoo](https://github.com/plutooo), [yellows8](https://github.com/yellows8), [naehrwert](https://github.com/naehrwert), derrek, [dimok](https://github.com/dimok789) and kanye_west for making the exploits and CFW possible
 - [Google Jules](https://jules.google.com)

## License

This project is licensed under the **GNU General Public License v2.0**. See the `LICENSE` file for details.

Some parts of this project were originally licensed under the MIT license. The original MIT license is preserved in the `LICENSE-MIT.md` file.

Wafel Installer also includes [libschrift](https://github.com/tomolt/libschrift), see its ISC-styled license [here](https://github.com/tomolt/libschrift/blob/master/LICENSE).
