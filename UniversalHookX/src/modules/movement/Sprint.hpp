#ifndef SPRINT_HPP
#define SPRINT_HPP

#include <iostream>
#include "../../dependencies/jni/jni.h"

class AutoSprint {
public:
    static void Run(JNIEnv* env, jobject minecraftInstance) {
        if (!env || !minecraftInstance)
            return;

        auto checkException = [&env](const char* location) -> bool {
            if (env->ExceptionCheck( )) {
                std::cout << "[AutoSprint] JNI Exception bei: " << location << std::endl;
                env->ExceptionDescribe( );
                env->ExceptionClear( );
                return true;
            }
            return false;
        };

        jclass mcClass = env->GetObjectClass(minecraftInstance);
        jfieldID playerFieldID = env->GetFieldID(mcClass, "player", "Lnet/minecraft/client/player/LocalPlayer;");

        if (!checkException("GetFieldID player")) {
            jobject playerObj = env->GetObjectField(minecraftInstance, playerFieldID);
            if (playerObj) {
                jclass playerClass = env->GetObjectClass(playerObj);

                // 1. Das 'input' Feld holen (Typ ist jetzt vermutlich ClientInput oder KeyboardInput)
                jfieldID inputFieldID = env->GetFieldID(playerClass, "input", "Lnet/minecraft/client/player/ClientInput;");
                if (checkException("GetFieldID input")) {
                    // Fallback falls der Typ im Mapping anders ist
                    env->ExceptionClear( );
                    inputFieldID = env->GetFieldID(playerClass, "input", "Lnet/minecraft/client/player/Input;");
                }

                if (inputFieldID) {
                    jobject inputObj = env->GetObjectField(playerObj, inputFieldID);
                    if (inputObj) {
                        jclass inputClass = env->GetObjectClass(inputObj);

                        // 2. moveVector (Vec2) holen statt forwardImpulse
                        jfieldID moveVectorFieldID = env->GetFieldID(inputClass, "moveVector", "Lnet/minecraft/world/phys/Vec2;");

                        if (!checkException("GetFieldID moveVector")) {
                            jobject moveVectorObj = env->GetObjectField(inputObj, moveVectorFieldID);
                            if (moveVectorObj) {
                                jclass vec2Class = env->GetObjectClass(moveVectorObj);
                                // In Vec2 ist 'y' der Vorwärts-Wert
                                jfieldID yFieldID = env->GetFieldID(vec2Class, "y", "F");
                                jfloat forwardImpulse = env->GetFloatField(moveVectorObj, yFieldID);

                                // 3. Sneaking prüfen
                                jmethodID isSneakingMethod = env->GetMethodID(playerClass, "isShiftKeyDown", "()Z");
                                jboolean isSneaking = env->CallBooleanMethod(playerObj, isSneakingMethod);

                                // 4. Sprint setzen wenn forward > 0
                                if (forwardImpulse > 0.0f && !isSneaking) {
                                    jmethodID setSprintingMethod = env->GetMethodID(playerClass, "setSprinting", "(Z)V");
                                    if (setSprintingMethod) {
                                        env->CallVoidMethod(playerObj, setSprintingMethod, JNI_TRUE);
                                    }
                                }
                                env->DeleteLocalRef(vec2Class);
                                env->DeleteLocalRef(moveVectorObj);
                            }
                        }
                        env->DeleteLocalRef(inputClass);
                        env->DeleteLocalRef(inputObj);
                    }
                }
                env->DeleteLocalRef(playerClass);
                env->DeleteLocalRef(playerObj);
            }
        }
        env->DeleteLocalRef(mcClass);
    }
};

#endif
