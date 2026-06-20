# Backups

This section assumes you already finished the setup with the wafel installer. If your console is not modded yet, [start here](START_HERE.md).

## OTP and SEEPROM

These are useful to recovery saves from a USB storage, in case your console ever dies. They are backed up automatically by Aroma.

You find them on the SD / USB in the `wiiu/backups/[SERIAL]` folder.



## Save Games

Backing up your save games is recommended before installing your Discs, but you can skip this if you want. Also creating a new Backup from time to time is a good idea, just in case something happens.

### Installing Save Mii

1. Open the`HB App Store` App from the Wii U Menu
2. *Search* for `SaveMii` and select `SaveMii /ProcessMod`
3. Press `A` to Download
4. Exit the HB App Store



### Creating the Backup

To Backup everything:

1. Launch the `SaveMii /ProcessMod` App from the Wii U Menu
2. `Batch Task Management`
3. `Batch Backup`
4. `Backup All`
5. Exit SaveMii
6. (Optional) Turn off the Wii U and copy the `wiiu/backups/` folder to the PC.

**NOTE:** You can also be more selective in the backup by using the appropriate options to backup only the titles you want.
You can find more [detailed SaveMii HowTos here](https://github.com/w3irDv/savemii/blob/main/README.md#detailed-howtos)

**IMPORTANT:** If you use the same media (SD or USB) for Homebrew and your Wii U games, it is highly recommend to copy the Backup to the PC. Else you would lose both the original and Backup in case the media dies.



## Next Step

[Dump and Installing Game Discs](DumpInstallGames.md)