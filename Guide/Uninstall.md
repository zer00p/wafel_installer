# Uninstall

If you wish to completely remove the homebrew environment, Stroopwafel, and ISFShax from your console, you can use the guided uninstall option within the Wafel Installer.

To ensure your console can safely boot without modifications, the uninstall process is split into two phases. The first phase removes your plugins, and the second phase removes ISFShax.

<div class="callout-warning">
<strong>Important:</strong> Please ensure you have undone any custom themes or custom keyboards installed to the system before proceeding. Failure to do so might cause a BRICK!
</div>

## Phase 1: Removing Stroopwafel and Plugins

1. **Boot your console** and launch the Wafel Installer (either from the Home Menu or via `https://wafel.xyz` in the Internet Browser).
2. If the installer presents startup checks, **Abort** or cancel out of them to reach the main menu.
3. In the main menu, select the **Guided Uninstall** option.
4. You will see an initial warning:

```text
Please read carefully:
 
Reinstalling ISFShax won't fix any issue. It is recommended
to always keep ISFShax.
 
This will undo all modifications this tool might have made to
the console, turning it stock again. It will also offer to reset
the SD card if it was partitioned.
Modifications made by other tools might still persist.
 
IMPORTANT: If you use redNAND, do NOT proceed.
If you installed custom keyboards or themes, you MUST
undo these changes BEFORE uninstalling. Removing
stroopwafel/isfshax otherwise might cause a BRICK.

[ I have undone everything, continue ]
[ Cancel ]
```

5. **Carefully read the warning.** Ensure you have completely removed any custom keyboards or custom themes. If you use redNAND, do NOT proceed. If you proceed with these modifications in place, your console might BRICK. Only select `[ I have undone everything, continue ]` if you are absolutely certain it is safe.
6. The installer will search for Stroopwafel files on your console's internal memory (SLC) and your SD card. Depending on where they are found, you will be asked to remove them. For example, if they are found on the SD card:

```text
Stroopwafel files found on SD card.
Do you want to remove them?

[ Yes ]
[ No ]
```

Select `[ Yes ]` (or `[ Both ]` if prompted for multiple locations) to completely remove Stroopwafel.
7. Once Stroopwafel is removed, the installer will prompt you to reboot your console.

## Phase 2: Removing ISFShax

By removing Stroopwafel first, we ensure that the console can successfully boot without loading any plugins. If your console fails to boot after completing Phase 1, do **NOT** proceed. You can recover by manually placing the Stroopwafel and plugin files back onto your SD card.

If your console boots successfully:

1. Open the Internet Browser Applet on the Wii U.
2. Go to the address bar and enter `https://wafel.xyz`.
3. Tap on **launch**.
4. Tap on **launch** again. After a few seconds, the Wafel Installer should load.
5. During the startup checks, the installer will detect the incomplete uninstall and prompt you:

```text
An incomplete uninstall was detected.
Do you want to continue uninstalling?

[ Yes, continue uninstall ]
[ No, cancel uninstall ]
```

7. Select `[ Yes, continue uninstall ]`.

### SD Card Formatting

The installer will ask if you want to format your SD card back to FAT32. This is useful if you had previously partitioned it for Wii U games and want to reclaim the space:

```text
Do you want to format the entire SD card to FAT32?
This will delete ALL data on it.

[ Yes, format SD ]
[ No, keep as is ]
```

Select `[ Yes, format SD ]` to reset the entire SD card to FAT32. This will delete everything on it, including games and homebrew files.

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
WARNING: You are about to make modifications to the console.
This software comes with ABSOLUTELY NO WARRANTY!
You are choosing to use this at your own risk.
The author(s) will not be held liable for any damage.
 
Do you want to proceed with Uninstall?

[ Yes ]
[ No ]
```

Select `[ Yes ]` to finish the uninstallation. Your console will reboot when the uninstall process finishes.
