#include "base.hpp"
#include "console/console.hpp"
#include "modules/ModuleManager.hpp"
#include <thread>
#include "utils/sdk/CMinecraft.h"
#include "modules/settings.hpp"

void Base::Init( ) {
    ModuleManager::Init( );
    LOG("[+] Base: Initialization complete.\n");
    ;
    Base::m_running = true;

    while (Base::m_running) {
        ModuleManager::UpdateModules( );
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}



