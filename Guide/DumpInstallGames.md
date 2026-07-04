# Dumping and Installing Game Discs

This explains how to dump your Games from the Discs and install them to the Internal Memory or USB/SD partition.
Since Wii U Discs are very susceptible to Disc rot, it is highly recommended you at least dump your Discs, while you still can. 

If you chose to partition your SD to also hold Wii U games, this will also apply when this guide or tools refer to USB, since the Wii U sees the partition as USB.
SD will refer to the FAT32 partition where your homebrew was installed. This can be either the real SD or the emulated SD Partition on the USB.

This section assumes you already finished the setup with the wafel installer. If your console is not modded yet, [get started](GettingStarted.md).

## Preparation

### Moving existing Data

If you plan to install a game to USB, make sure to move its data, which you might already have on the Internal Memory, to the USB Storage using the Data Management: [Detailed Instructions](https://en-americas-support.nintendo.com/app/answers/detail/a_id/1532/~/how-to-copy-or-move-data-between-two-external-storage-devices)
You have to do this before installing your games. You can't move it afterwards, as it would delete the Game again. If the game is installed to a different location than your existing saves, it won't see the save.

**NOTE:** If you missed this, you can still copy your saves manually using [SaveMii ProcessMod](SaveBackup.md)

### Installing Tools

1. Open the`HB App Store` App from the Wii U Menu
2. *Search* for `wudd` and select `WUDD`
3. Press `A` to Download
4. *Search* for `wupinstaller` and select `WUP Installer GX2 (Aroma)`
5. Press `A` to Download
6. Exit the HB App Store



## Dumping Games

Games will be dumped to the FAT32 partition where your homebrew is. So either the real SD or the emulated SD on the USB device. They don't get written to the special Wii U partition / USB yet.

1. Open the`Wii U Disc Dumper` App from the Wii U Menu
2. `Dump partition as .app`
3. `Game`

You can dump multiple games in one go, if you have enough storage.



## (Optional) Copy Dump to the PC

1. Shutdown the Wii U
2. Copy the games from the `wudump` folder on the SD / USB to the PC.

## Installing Dumps

1. Open the`WUP Installer GX2` App from the Wii U Menu
2. Select your games. Press `+` to select all. (They have cryptic titles like WUP-P-ANSP: GM000...)
3. (Optional) Select `Del. after install` (This will delete the dump from the SD, you can also keep them there if you want)
4. Press `Install` then `Yes`
5. Select where you want to install it to. `NAND` is the internal memory. `USB` is the USB HDD or the Wii U SD partition, if you set that up.