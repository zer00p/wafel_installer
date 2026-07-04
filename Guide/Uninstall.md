# Uninstall

If you wish to completely remove the homebrew environment, Stroopwafel, and ISFShax from your console, you can use the guided uninstall option within the Wafel Installer.

<div class="callout-warning">
<strong>Important:</strong> Please ensure you have undone any custom themes or custom keyboards installed to the system before proceeding. Failure to do so might cause a BRICK!
</div>

## Preparation

1. **Boot your console** without the SD Card or USB device connected.
2. Open the Internet Browser Applet on the Wii U.
3. Go to the address bar and enter `https://wafel.xyz`.
4. Tap on **launch**.
5. Tap on **launch** again. After a few seconds, the Wafel Installer should load.
6. Once the installer has started, **connect your homebrew SD / USB device**.
7. The installer will present some startup checks. **Abort** or cancel out of the initial startup checks to reach the main menu.
8. In the main menu, select the **Guided Uninstall** option.

## Uninstall Process

Once you select the Guided Uninstall option, you will be walked through the removal of all modifications. 

### Initial Warning

```text
Please read carefully:
 
Reinstalling ISFShax won't fix any issue. It is recommended
to always keep ISFShax.
 
This will undo all modifications this tool might have made to
the console, turning it stock again. It will also offer to reset
the SD card if it was partitioned.
Modifications made by other tools might still persist.
 
IMPORTANT: If you installed custom keyboards or themes, you
MUST undo these changes BEFORE uninstalling. Removing
stroopwafel/isfshax otherwise might cause a BRICK.

[ Continue ]
[ Cancel ]
```

Select `[ Continue ]` to proceed with the uninstallation.

### SD Card Formatting

If your SD card has multiple partitions (for example, if you partitioned it for games) or large amounts of unallocated space, you will be prompted to format it back to a single FAT32 partition:

```text
Your SD card seems to have multiple partitions or unallocated
space. Do you want to format the entire SD card to FAT32?
This will delete ALL data on it.

[ Yes, format SD ]
[ No, keep as is ]
```

Select `[ Yes, format SD ]` to reset the entire SD card to FAT32. This will delete everything on it, including games and homebrew files.

### Stroopwafel Removal

The installer will search for Stroopwafel files on both your console's internal memory (SLC) and your SD card. Depending on where they are found, you will be asked to remove them:

```text
Stroopwafel files found on both SLC and SD card.
Where do you want to remove them from?

[ Both ]
[ SLC only ]
[ SD card only ]
[ Skip ]
```
Select `[ Both ]` (or `[ Yes ]` if prompted for a single location) to completely remove Stroopwafel.

### ISFShax Removal

Finally, you will be asked if you want to uninstall ISFShax. It is generally recommended to keep ISFShax since it provides brick protection, but you can remove it if you wish to return your console entirely to stock.

```text
Do you want to uninstall ISFShax?
 
WARNING: It is STRONGLY recommended to KEEP ISFShax installed.
It provides brick protection
There is usually no reason to uninstall it.

[ Yes, uninstall ISFShax ]
[ No, keep ISFShax ]
```

If you select `[ Yes, uninstall ISFShax ]`, you will see a final confirmation warning:

```text
WARNING: Before Uninstalling ISFShax make sure the console
boots correctly using the 'Patch ISFShax and boot IOS (slc)'
option in minute. If your console can't boot correctly,
uninstalling ISFShax will BRICK the console!!!
 
WARNING: You are about to make modifications to the console.
This software comes with ABSOLUTELY NO WARRANTY!
You are choosing to use this at your own risk.
The author(s) will not be held liable for any damage.
 
Do you want to proceed with Uninstall?

[ Yes ]
[ No ]
```

Ensure your console can boot normally before confirming. Select `[ Yes ]` to finish the uninstallation.
Your console will reboot when the uninstall process finishes.
