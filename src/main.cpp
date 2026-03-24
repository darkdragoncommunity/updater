#include <iostream>
#include <curl/curl.h>
#include "config.h"
#include "language.h"
#include "updater.h"
#include "ui.h"
#include "utils.h"

int main(int argc, char** argv) {
    Utils::ResetLog();
    Utils::Log("[INFO] launcher startup");

    // Global Curl Init
    curl_global_init(CURL_GLOBAL_ALL);
    Utils::Log("[INFO] curl initialized");

    // Init Config
    Config& config = Config::GetInstance();
    config.Load();

    // Init Language
    Language& lang = Language::GetInstance();
    lang.SetLanguage(config.GetLanguage());

    {
        // Keep the UI and all updater/downloader objects scoped so they are
        // destroyed before curl_global_cleanup().
        LauncherUI ui;
        if (!ui.Init()) {
            Utils::Log("[ERROR] failed to initialize UI");
            std::cerr << "Failed to init UI" << std::endl;
            return -1;
        }

        // Run UI (Main loop)
        ui.Run();
    }

    // Cleanup
    curl_global_cleanup();
    Utils::Log("[INFO] launcher shutdown");

    return 0;
}
