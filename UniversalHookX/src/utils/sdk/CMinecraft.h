#pragma once
#include <thread>
#include <Windows.h>
#include "../../dependencies/jni/jni.h"


class CMinecraft final {
public:

    

    CMinecraft(JavaVM* p_jvm)
        : p_jvm(p_jvm) {
        p_jvm->AttachCurrentThread((void**)&p_env, nullptr);
        Init( );
    }
    bool m_initialized = false;

    bool IsInitialized( ) const {
        return class_ptr != nullptr && class_instance != nullptr;
    }

    bool Init( ) {
        class_ptr = p_env->FindClass("net/minecraft/client/Minecraft");

        if (class_ptr == nullptr) {
            printf("[-] Failed to get %s class pointer\n", name);
            return false;
        }

        jfieldID class_instance_field_id{p_env->GetStaticFieldID(class_ptr, "instance", "Lnet/minecraft/client/Minecraft;")};

        if (class_instance_field_id == 0) {
            printf("[-] Failed to get class instance field id\n");
            return false;
        }

        class_instance = p_env->GetStaticObjectField(class_ptr, class_instance_field_id);

        if (class_instance == nullptr) {
            printf("[-] Failed to get %s class instance\n", name);
            return false;
        }
        ;
        printf("[+] Successfully initialized %s class\n", name);

        static jfieldID mouse_delay_id{p_env->GetFieldID(class_ptr, "rightClickDelay", "I")};
        if (mouse_delay_id == nullptr) {
            printf("[-] rightClickDelay field not found!\n");
            return false;
        }
        printf("[+] rightClickDelay field found!\n");

        static jfieldID player_id{p_env->GetFieldID(class_ptr, "player", "Lnet/minecraft/client/player/LocalPlayer;")};
        if (player_id == nullptr) {
            printf("[-] player field not found!\n");
            return false;
        }
        printf("[+] player field found!\n");


        jobject player_obj = p_env->GetObjectField(class_instance, player_id);
        // player_obj kann nullptr sein, wenn kein Level geladen ist!
        if (player_obj != nullptr) {
            printf("[+] player object found!\n");
        }

         

        m_initialized = true;
        return true;     

    }

    void SetRightClickDelay(int new_delay) {
        JNIEnv* env = nullptr;
        p_jvm->AttachCurrentThread((void**)&env, nullptr);

        jfieldID mouse_delay_id = env->GetFieldID(class_ptr, "rightClickDelay", "I");
        if (!mouse_delay_id || !class_instance)
            return;

        env->SetIntField(class_instance, mouse_delay_id, new_delay);
    }

    void StartRightClickLoop( ) {
        std::thread([this]( ) {
            JNIEnv* env = nullptr;
            p_jvm->AttachCurrentThread((void**)&env, nullptr); // ← Eigenes env!

            jfieldID mouse_delay_id = env->GetFieldID(class_ptr, "rightClickDelay", "I");
            while (true) {
                if (IsInitialized( ) && mouse_delay_id) {
                    env->SetIntField(class_instance, mouse_delay_id, 0);
                }
                Sleep(50);
            }

            p_jvm->DetachCurrentThread( );
        }).detach( );
    }


    //
    // Java methods
    //
private:
    jclass class_ptr;
    jobject class_instance;
    JNIEnv* p_env;
    JavaVM* p_jvm;

    const char name[10] = {"Minecraft"};

public:
    jobject GetInstance( ) const { return class_instance; }
    jclass GetClass( ) const { return class_ptr; }
    JNIEnv* GetEnv( ) const { return p_env; }
};
