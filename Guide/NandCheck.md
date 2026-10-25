# NAND Health Check

The Wii U stores its system, games and saves on an internal eMMC chip (the MLC). Some consoles shipped with a Hynix eMMC, which is known to fail over time. When it fails, it usually does so silently at first, until the console eventually stops booting with errors like 160-0103 "System Memory Error".

The NAND Health Check in the Wafel Installer tells you:

- The **manufacturer** of your eMMC (Hynix, Samsung or Toshiba)
- If **previous NAND errors** were recorded in the system logs (SLC, SLCCMPT and MLC)
- If all 3 NANDs can still be **read without errors**

This not only tells you if you have the problematic Hynix eMMC, but also if it already began to silently fail.

::: {.callout-warning}
**No mods required!** The NAND Health Check does not make any changes to your console unless you explicitly tell it to. You can run it on a completely stock console straight from the browser.
:::



## Launching the NAND Health Check

### Option A: Stock Console (or booted without SD)

1. Remove the SD card and disconnect any USB storage device before turning on the Wii U
2. Open the Internet Browser Applet on the Wii U
3. Go to the address bar, right at the top (NOT the search)
4. Enter `https://wafel.xyz`
5. Press OK. You should now see the Wafel Installer web page
6. Tap on launch
7. Tap on launch again. After a few seconds the wafel installer app should load.

Since there is no SD card inserted, you will be greeted by this prompt:

```text
No SD card detected.

[ Retry SD ]
[ Use USB (do NOT connect it just yet) ]
[ Check NAND Health ]
[ Abort ]
```

Select `Check NAND Health`.

**Note:** The installer might get stuck while launching. If that happens and it hangs for 2 minutes, force power off the console (by holding the power button), turn it back on, and try again.

### Option B: Modded Console (Aroma)

If you used the Wafel Installer to install homebrew, you already have it, but older versions might not have the NAND Health Check yet or lack some of its tests. Either way, get the latest version from the HB App Store first:

1. Open the `HB App Store` App from the Wii U Menu
2. *Search* for `wafel` and select [`Wafel Installer`](https://hb-app.store/wiiu/WafelInstaller)
3. Press `A` to Download (or Update, if you already have an older version). If it is already up to date, there is nothing to do.
4. Exit the HB App Store
5. Run the Wafel Installer Homebrew App
6. If the installer asks you to install or download something, choose `Cancel` / `No` to reach the main menu
7. In the main menu, select `Check NAND Health`

You can also use Option A on a modded console, just boot without the SD card.



## Running the Check

### Log Report

First the installer reads the manufacturer of the eMMC and scans the system logs for previously recorded NAND errors. The result is shown in the Log Report:

```text
=== Log Report ===
MLC Manufacturer: Samsung (Good)

MLC Errors:
Corruption: 0, Media: 0

SLC Errors:
Corruption: 0, Media: 0

SLCCMPT Errors:
Corruption: 0, Media: 0

[ OK ]
```

- **MLC Manufacturer:** `Samsung` and `Toshiba` are good. `Hynix` is marked with `(WARNING)`, as these are the ones known to fail. Having a Hynix eMMC doesn't mean it is already failing, but it is more likely to fail in the future.
- **Media errors** mean the console failed to read data from the chip. Media errors on the MLC confirm that the eMMC is failing.
- **Corruption errors** mean data was read, but it was damaged. This can be an early sign of a failing chip.

A clean Log Report doesn't guarantee that everything is fine. Errors are only logged when the system actually tries to read a file stored on degraded cells. If none of the affected data has been accessed yet, there is nothing in the logs. That's why the read tests below are important, as they read all the data.

If the report contains `SCFM cache is corrupted!` or errors on the SLC or SLCCMPT, it will tell you that this is **NOT the usual eMMC (MLC) failure**. In that case please ask for assistance before taking action (installing ISFShax is always fine), for example on the [GBAtemp Wii U forum](https://gbatemp.net/forums/nintendo-wii-u.245/) or on [r/wiiu](https://www.reddit.com/r/wiiu/). Include a photo of the Log Report.

### SLC Read Test

```text
Do you want to run a file read test on the SLC?
(Takes a few minutes)

[ Yes ]
[ No ]
```

The SLC is a small internal NAND that holds the core Wii U system. Select `Yes` to read every file on it. You can abort the test with `B`. When finished, it will tell you how many files were scanned and list any files that could not be read.

### SLCCMPT Read Test

```text
Do you want to run a file read test on SLCCMPT (vWii)?
(Takes a few minutes)

[ Yes ]
[ No ]
```

The SLCCMPT is the NAND of the Virtual Wii (vWii). Select `Yes` to read every file on it, same as the SLC test.

### MLC Read Test

```text
Do you want to run the MLC read test?
(Takes ~30 minutes). It is recommended.

[ Yes ]
[ No ]
```

::: {.callout-warning}
**Important:** Make sure to run the full MLC read test! This is the most important part of the check, as the MLC is the eMMC that fails.
:::

Select `Yes`. This reads the whole MLC from start to finish and counts the read errors. It takes about 30 minutes. Keep the console powered on and don't touch it until it's done. You can abort the test with `B`, but then the result is incomplete.

If no errors were found:

```text
Finished reading MLC!
No media errors found.
```

Your eMMC is fine at the moment. If you have a Hynix eMMC, it is a good idea to repeat the check from time to time.

If errors were found:

```text
Finished reading MLC.
Found X read errors.
This indicates a failing eMMC.
```

See [My eMMC is failing](#my-emmc-is-failing) below.

### Finishing

If ISFShax is not installed yet, the installer might offer to install it depending on the result (see [Install ISFShax](#install-isfshax) below). If you only want to check your NAND, choose `No, skip`. If you install it, the console will reboot to install ISFShax.

When the check is finished, you return to where you started it from:

- **Option A:** Select `Abort` on the `No SD card detected.` prompt, then press `B` in the main menu to exit the Wafel Installer.
- **Option B:** Press `B` in the main menu to exit the Wafel Installer.



## My eMMC is failing

If the MLC read test found errors or the logs contain MLC media errors, your eMMC is failing. It will most likely get worse over time, until the console no longer boots.

```text
Your eMMC (MLC) appears to be failing.
Consider one of these fixes:

- redNAND: Uses your SD card as replacement
  https://gbatemp.net/threads/642268/

- USBMLC: Uses a USB device as replacement
  https://gbatemp.net/threads/672971/
```

### 1. Install ISFShax

```text
CRITICAL: You should install ISFShax IMMEDIATELY
as a safety net against an impending brick!

Would you like to install ISFShax now?

[ Yes, install ISFShax ]
[ No, skip ]
```

ISFShax runs before the system on the eMMC is even loaded. This allows you to recover the console, even if it no longer boots because of the failing eMMC. Install it as soon as possible, ideally right away by selecting `Yes, install ISFShax`.

The installer will also recommend ISFShax if corruption errors were found, and offer it if you have a Hynix eMMC, to allow easy recovery of potential problems in the future.

::: {.callout-warning}
**Installed ISFShax from the NAND Health Check?** This only installs the bare ISFShax, without Stroopwafel. To complete the setup, follow the guide from [Getting Started](GettingStarted.md) and run the Wafel Installer again, so it can also install Stroopwafel (and Aroma).
:::

### 2. Replace the eMMC

To keep using the console, the failing eMMC needs to be replaced. There are two options that don't require soldering:

- **[redNAND](https://gbatemp.net/threads/642268/):** Uses your SD card as replacement for the MLC.
- **[USBMLC](https://gbatemp.net/threads/672971/):** Uses a USB device as replacement for the MLC.

If you can solder, there is also **[MLC2SD](https://gbatemp.net/threads/mlc2sd-a-wii-u-nand-emmc-replacement-interposer.651917/)**, an interposer board that replaces the eMMC with an SD card.

**Note:** Do not attempt to do a system update, if your eMMC is failing or you saw 160-0103 "System Memory Errors" in the past.
