<div style="text-align: center;">
    <img src="dist/dumpling-logo.png" alt="ISFShax Loader Logo" style="width: 80px;" />
    <h3>ISFShax Loader</h3>
    <p>ISFShax Loader is a tool designed to load and install ISFShax on the Wii U, providing an environment for system recovery and advanced modifications.</p>
</div>

## How to install
**Method 1:**  
Use the Wii U App Store to download and install it in the homebrew launcher. See Dumpling's page [here](https://apps.fortheusers.org/wiiu/dumpling).

**Method 2:**  
Download the [latest release from GitHub](https://github.com/zer00p/isfshax-loader/releases), and extract the `isfshax_loader.zip` file to the root of your SD card.

**Method 3:**  
Use [dumplingapp.com](https://dumplingapp.com) on your Wii U to launch ISFShax Loader without any setup or SD card.

## How to use

If you want to fully homebrew your Wii U, we recommend using [wiiu.hacks.guide](https://wiiu.hacks.guide) to install Tiramisu and installing ISFShax Loader using the first two methods mentioned above!

You don't need to run/have Mocha CFW or Haxchi, just launch ISFShax Loader from the Homebrew Launcher.

## How to compile
 - Install [DevkitPro](https://devkitpro.org/wiki/Getting_Started) for your platform.
 - Install xxd and zip if you don't have it already through your Linux package manager (or something equivalent for msys2 on Windows).
 - Install [wut](https://github.com/devkitpro/wut) through DevkitPro's pacman or compile (and install) the latest source yourself.
 - Compile [libmocha](https://github.com/wiiu-env/libmocha).
 - Then, with all those dependencies installed, you can just run `make` to get the .rpx file that you can run on your Wii U.


## Credits
 - [Crementif](https://github.com/Crementif) for the original [dumpling](https://github.com/dumpling-app/dumpling)
 - [emiyl](https://github.com/emiyl) for [dumpling-classic](https://github.com/emiyl/dumpling-classic)
 - [zer00p](https://github.com/zer00p) for ISFShax Loader
 - chriz, Tomk007 and Jaimie for testing
 - [wut](https://github.com/devkitpro/wut) for providing the Wii U toolchain that ISFShax Loader is built with
 - FIX94, Maschell, Quarky, GaryOderNichts and koolkdev for making and maintaining homebrew (libraries)
 - smea, plutoo, yellows8, naehrwert, derrek, dimok and kanye_west for making the exploits and CFW possible

## License
ISFShax Loader is licensed under [MIT](https://github.com/zer00p/isfshax-loader/blob/master/LICENSE.md).
ISFShax Loader also includes [libschrift](https://github.com/tomolt/libschrift), see its ISC-styled license [here](https://github.com/tomolt/libschrift/blob/master/LICENSE).
