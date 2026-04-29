#pragma once
#include <string>

namespace Paths {
    // SLC vol Paths
    inline const std::string SlcRoot = "/vol/storage_slc";
    inline const std::string SlcHaxDir = SlcRoot + "/sys/hax";
    inline const std::string SlcInstallerDir = SlcHaxDir + "/installer";
    inline const std::string SlcPluginsDir = SlcHaxDir + "/ios_plugins";
    inline const std::string SlcFwImg = SlcHaxDir + "/fw.img";
    inline const std::string SlcInstallerFwImg = SlcInstallerDir + "/fw.img";
    inline const std::string SlcInstallerSblockImg = SlcInstallerDir + "/sblock.img";
    inline const std::string SlcInstallerSblockSha = SlcInstallerDir + "/sblock.sha";
    inline const std::string SlcTmpDir = SlcRoot + "/sys/tmp";

    // MLC/USB Paths
    inline const std::string MlcRoot = "/vol/storage_mlc01";
    inline const std::string UsbRoot = "/vol/storage_usb01";
    inline const std::string MlcUsrDir = MlcRoot + "/usr";
    inline const std::string UsbUsrDir = UsbRoot + "/usr";

    // SD vol Paths
    inline const std::string SdRoot = "/vol/external01";
    inline const std::string SdPluginsDir = SdRoot + "/wiiu/ios_plugins";
    inline const std::string SdCfwDir = SdRoot + "/wiiu/cfw";
    inline const std::string IosSdCfwDir = "/vol/sdcard/wiiu/cfw";
    inline const std::string SdAromaDir = SdRoot + "/wiiu/environments/aroma";
    inline const std::string SdWafelInstallerDir = SdRoot + "/wiiu/apps/wafel_installer";
    inline const std::string SdWafelInstallerWuhb = SdWafelInstallerDir + "/wafel_installer.wuhb";
    inline const std::string SdMinuteDir = SdRoot + "/minute";
    inline const std::string SdFwImg = SdRoot + "/fw.img";

    // System vol paths (SLC alias)
    inline const std::string SystemRoot = "/vol/system";
    inline const std::string SystemHaxInstallerFwImg = SystemRoot + "/hax/installer/fw.img";
    inline const std::string SystemTmpFwImg = SystemRoot + "/tmp/fw.img";
}
