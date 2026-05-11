#pragma once

#include "utils/sdk/CMinecraft.h"


extern CMinecraft* g_Minecraft;

struct Base {
    static void Init( );
     
    static inline bool m_running;

    static inline const char* m_version = "BETA";
};
