#include "../../dependencies/jni/jni.h"
#include "../../console/console.hpp"
#include <Windows.h>
#include <thread>
#include <cstdio>
#include "../../base.hpp"
#include "CMinecraft.h"
#include "java.hpp"


bool m_initialized = false;
CMinecraft* g_Minecraft = nullptr;

void InitMinecraft(JavaVM* jvm) {
    g_Minecraft = new CMinecraft(jvm);

    if (g_Minecraft->IsInitialized( )) {
        g_Minecraft->StartRightClickLoop( );
    }
}
