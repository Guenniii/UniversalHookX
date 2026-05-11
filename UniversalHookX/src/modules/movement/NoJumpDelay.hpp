#ifndef NO_JUMP_DELAY_HPP
#define NO_JUMP_DELAY_HPP

#include <cstdio>
#include "../../dependencies/jni/jni.h"

class NoJumpDelay {
public:
    static void Run(JNIEnv* env, jobject minecraftInstance) {
        if (!env || !minecraftInstance)
            return;

        // 1. Klassen finden
        jclass mcClass = env->GetObjectClass(minecraftInstance);

        // 2. LocalPlayer Instanz holen
        // Hinweis: Feldname "player" ggf. an Mappings anpassen (z.B. f_91074_)
        jfieldID playerFieldID = env->GetFieldID(mcClass, "player", "Lnet/minecraft/client/player/LocalPlayer;");
        if (!playerFieldID) {
            std::printf("[-] NoJumpDelay: Player field not found.\n");
            return;
        }

        jobject playerObj = env->GetObjectField(minecraftInstance, playerFieldID);
        if (!playerObj)
            return;

        // 3. jumpDelay Feld finden
        // In den meisten Versionen ist jumpDelay ein 'int' in LivingEntity (Basisklasse von LocalPlayer)
        // Der Mapping-Name variiert (oft "jumpDelay" oder "f_20954_")
        jclass playerClass = env->GetObjectClass(playerObj);
        jfieldID jumpDelayFieldID = env->GetFieldID(playerClass, "noJumpDelay", "I");

        if (jumpDelayFieldID) {
            // Den Delay auf 0 setzen
            env->SetIntField(playerObj, jumpDelayFieldID, 0);
        } else {
            // Falls das Feld nicht in LocalPlayer direkt ist, in der Superklasse suchen
            jclass livingEntityClass = env->GetSuperclass(playerClass);
            jumpDelayFieldID = env->GetFieldID(livingEntityClass, "noJumpDelay", "I");

            if (jumpDelayFieldID) {
                env->SetIntField(playerObj, jumpDelayFieldID, 0);
            } else {
                std::printf("[-] NoJumpDelay: jumpDelay field not found (check mappings).\n");
            }
            env->DeleteLocalRef(livingEntityClass);
        }

        // Cleanup
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(mcClass);
    }
};

#endif // NO_JUMP_DELAY_HPP
