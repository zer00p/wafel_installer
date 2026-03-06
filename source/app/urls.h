#pragma once
#include <string>

namespace URLs {
    // Repositories
    inline const std::string RepoEnvironmentLoader = "wiiu-env/EnvironmentLoader";
    inline const std::string RepoCustomRPXLoader = "wiiu-env/CustomRPXLoader";
    inline const std::string RepoPayloadLoaderPayload = "wiiu-env/PayloadLoaderPayload";
    inline const std::string RepoAroma = "wiiu-env/Aroma";
    inline const std::string RepoAppstore = "fortheusers/hb-appstore";
    inline const std::string RepoWafelInstaller = "zer00p/wafel_installer";
    inline const std::string RepoMinute = "StroopwafelCFW/minute_minute";
    inline const std::string RepoIsfshax = "isfshax/isfshax";
    inline const std::string RepoIsfshaxInstaller = "isfshax/isfshax_installer";

    // Direct URLs
    inline const std::string PluginsCsv = "https://raw.githubusercontent.com/zer00p/wafel_installer/refs/heads/master/plugins.csv";
    
    inline const std::string MinuteFwImg = "https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw.img";
    inline const std::string MinuteFwFastbootImg = "https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw_fastboot.img";
    
    inline const std::string IsfshaxSblockImg = "https://github.com/isfshax/isfshax/releases/latest/download/superblock.img";
    inline const std::string IsfshaxSblockSha = "https://github.com/isfshax/isfshax/releases/latest/download/superblock.img.sha";
    inline const std::string IsfshaxInstallerIosImg = "https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img";

    inline const std::string WafelInstallerWuhb = "https://github.com/zer00p/wafel_installer/releases/latest/download/wafel_installer.wuhb";

    // API
    inline const std::string GitHubApiLatestRelease = "https://api.github.com/repos/";
}
