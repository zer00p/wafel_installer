<div style="text-align: center;">
    <img src="dist/wafel_installer-logo.png" alt="Wafel Installer Logo" style="width: 80px;" />
    <h3>Wafel Installer</h3>
    <p>Wafel Installer is a tool designed to load and install ISFShax on the Wii U, providing an environment for system recovery and advanced modifications.</p>
</div>

## How to use
**Method 1:**  
Use [wafel.xyz](https://wafel.xyz) on your Wii U to launch Wafel Installer without any setup or SD card.

**Method 2:**
Use the Wii U App Store to download ot. See Dumpling's page [here](https://apps.fortheusers.org/wiiu/wafel_installer). (comming soon)

**Method 2:**  
Download the [latest release from GitHub](https://zer00p/wafel_installer/releases), and extract the `wafel_installer.zip` file to the root of your SD card.

## How to use

If you want to fully homebrew your Wii U, we recommend using [wiiu.hacks.guide](https://wiiu.hacks.guide) to install Tiramisu and installing Wafel Installer using the first two methods mentioned above!

You don't need to run/have Mocha CFW or Haxchi, just launch Wafel Installer from the Homebrew Launcher.

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
 - chriz, Tomk007 and Jaimie for testing
 - [wut](https://github.com/devkitpro/wut) for providing the Wii U toolchain that Wafel Installer is built with
 - [Dimok](https://github.com/dimok789) for the original fw_img_payload.
 - The wiiu-env developers (especially [Maschell](https://github.com/Maschell)) for Aroma.
 - [shinyquagsire23](https://github.com/shinyquagsire23) for stroopwafel.
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
