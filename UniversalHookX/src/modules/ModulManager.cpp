#include "ModuleManager.hpp"
#include "../console/console.hpp"
#include "combat/cw.hpp"


void ModuleManager::Init( ) {
    m_modules.push_back(std::make_unique<CW>( ));
    LOG("[+] ModuleManager: Initialization complete.\n");
}


void ModuleManager::UpdateModules( ) {

    for (auto& module : m_modules) {
        if (module->IsEnabled( )) {
            module->Update( );
        }
    }
}
