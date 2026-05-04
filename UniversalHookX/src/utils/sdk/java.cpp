#include "../../dependencies/jni/jni.h"
#include "../../console/console.hpp"
#include <Windows.h>
#include <thread>
#include <cstdio>

#include "java.hpp"

void Java::Init()
{
    JavaVM* p_jvm{nullptr};
    jint result = JNI_GetCreatedJavaVMs(&p_jvm, 1, nullptr);
    if (result != JNI_OK) {
        LOG("[-] Failed to get created Java VMs. Error code: %d\n", result);
        return;
    }
    JNIEnv* p_env{nullptr};
    result = p_jvm->AttachCurrentThread((void**)&p_env, nullptr);
    if (result != JNI_OK) {
        LOG("[-] Failed to attach current thread to JVM. Error code: %d\n", result);
        return;
    }
    jclass mouse_class = p_env->FindClass("net/minecraft/player/PlayerEntity");
    if (mouse_class == 0) {
        LOG("[-] Failed to find Mouse class.\n");
    } else {
        LOG("[+] Successfully found Mouse class.\n");
    }
    jmethodID is_button_down = p_env->GetStaticMethodID(mouse_class, "isButtonDown", "(I)Z");
    if (is_button_down == 0) {
        LOG("[-] Failed to find isButtonDown method.\n");
    } else {
        LOG("[+] Successfully found isButtonDown method.\n");
    }
}
