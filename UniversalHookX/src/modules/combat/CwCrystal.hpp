
#pragma once
#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h" // deine bestehende Klasse
#include <Windows.h>
#include <atomic>
#include <thread>

class CwCrystal {
public:
    CwCrystal(JavaVM* jvm, CMinecraft* mc) : p_jvm(jvm), p_mc(mc) { }

    void Start( ) {
        m_running = true;
        std::thread([this]( ) { TickLoop( ); }).detach( );
    }

    void Stop( ) { m_running = false; }
private:
    JavaVM* p_jvm;
    CMinecraft* p_mc;
    std::atomic<bool> m_running = false;

    // ── gecachte JNI-IDs ──────────────────────────────────────────
    jclass c_Minecraft = nullptr;
    jclass c_ClientPlayerEntity = nullptr;
    jclass c_InteractionManager = nullptr;
    jclass c_EntityHitResult = nullptr;
    jclass c_BlockHitResult = nullptr;
    jclass c_EndCrystalEntity = nullptr;
    jclass c_PlayerInventory = nullptr;
    jclass c_Items = nullptr;
    jclass c_BlockPos = nullptr;
    jclass c_BlockState = nullptr;
    jclass c_Block = nullptr;
    jclass c_Blocks = nullptr;
    jclass c_ClientWorld = nullptr;
    jclass c_Hand = nullptr;
    jclass c_ActionResult = nullptr;
    jclass c_Window = nullptr;

    bool m_ids_cached = false;

    // ── Hilfsfunktion: Klasse finden + GlobalRef ──────────────────
    jclass FindGlobal(JNIEnv* env, const char* name) {
        jclass local = env->FindClass(name);
        if (!local) {
            printf("[-] Class not found: %s\n", name);
            return nullptr;
        }
        jclass global = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        return global;
    }

    // ── Alle IDs einmalig cachen ──────────────────────────────────
    bool CacheIDs(JNIEnv* env) {
        if (m_ids_cached)
            return true;

        // Klassennamen ggf. an deine MC-Version anpassen!
        c_Minecraft = FindGlobal(env, "net/minecraft/client/Minecraft");
        c_ClientPlayerEntity = FindGlobal(env, "net/minecraft/client/player/LocalPlayer");
        c_InteractionManager = FindGlobal(env, "net/minecraft/client/multiplayer/MultiPlayerGameMode");
        c_EntityHitResult = FindGlobal(env, "net/minecraft/world/phys/EntityHitResult");
        c_BlockHitResult = FindGlobal(env, "net/minecraft/world/phys/BlockHitResult");
        c_EndCrystalEntity = FindGlobal(env, "net/minecraft/world/entity/boss/enderdragon/EndCrystal");
        c_PlayerInventory = FindGlobal(env, "net/minecraft/world/entity/player/Inventory");
        c_Items = FindGlobal(env, "net/minecraft/world/item/Items");
        c_BlockPos = FindGlobal(env, "net/minecraft/core/BlockPos");
        c_BlockState = FindGlobal(env, "net/minecraft/world/level/block/state/BlockState");
        c_Block = FindGlobal(env, "net/minecraft/world/level/block/Block");
        c_Blocks = FindGlobal(env, "net/minecraft/world/level/block/Blocks");
        c_ClientWorld = FindGlobal(env, "net/minecraft/client/multiplayer/ClientLevel");
        c_Hand = FindGlobal(env, "net/minecraft/world/InteractionHand");
        c_ActionResult = FindGlobal(env, "net/minecraft/world/InteractionResult");
        c_Window = FindGlobal(env, "com/mojang/blaze3d/platform/Window");

        // Prüfen ob alles geklappt hat
        if (!c_Minecraft || !c_ClientPlayerEntity || !c_InteractionManager) {
            printf("[-] CwCrystal: Critical class missing!\n");
            return false;
        }



        m_ids_cached = true;
        printf("[+] CwCrystal: All classes cached.\n");
        return true;

        
    }

    // ── Haupt-Loop (entspricht dem ClientTickEvent) ───────────────
    void TickLoop( ) {
        JNIEnv* env = nullptr;
        jint attach_result = p_jvm->AttachCurrentThread((void**)&env, nullptr);
        if (attach_result != JNI_OK || !env) {
            printf("[-] CwCrystal: AttachCurrentThread failed!\n");
            return;
        }

        if (!CacheIDs(env))
            return;

        // ── TEMPORÄRER DEBUG: Methoden ausgeben ──
        auto PrintMethods = [&](jclass cls, const char* className) {
            jclass classClass = env->FindClass("java/lang/Class");
            jmethodID getDeclaredMethods = env->GetMethodID(classClass, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");
            jobjectArray methods = (jobjectArray)env->CallObjectMethod(cls, getDeclaredMethods);

            jclass methodClass = env->FindClass("java/lang/reflect/Method");
            jmethodID getName = env->GetMethodID(methodClass, "getName", "()Ljava/lang/String;");

            jint count = env->GetArrayLength(methods);
            printf("[*] Methods of %s (%d):\n", className, count);
            for (jint i = 0; i < count; i++) {
                jobject method = env->GetObjectArrayElement(methods, i);
                jstring name = (jstring)env->CallObjectMethod(method, getName);
                const char* str = env->GetStringUTFChars(name, nullptr);
                printf("  [%d] %s\n", i, str);
                env->ReleaseStringUTFChars(name, str);
                env->DeleteLocalRef(name);
                env->DeleteLocalRef(method);
            }
            env->DeleteLocalRef(methods);
        };

        PrintMethods(c_Window, "Window");
        PrintMethods(c_InteractionManager, "InteractionManager");
        PrintMethods(c_ActionResult, "ActionResult");


        auto SafeGetField = [&](jclass cls, const char* name, const char* sig) -> jfieldID {
            jfieldID id = env->GetFieldID(cls, name, sig);
            if (!id) {
                printf("[-] FieldID not found: %s %s\n", name, sig);
                env->ExceptionClear( ); // ← CRITICAL, sonst crash beim nächsten JNI call
            }
            return id;
        };

        auto SafeGetMethod = [&](jclass cls, const char* name, const char* sig) -> jmethodID {
            jmethodID id = env->GetMethodID(cls, name, sig);
            if (!id) {
                printf("[-] MethodID not found: %s %s\n", name, sig);
                env->ExceptionClear( ); // ← CRITICAL
            }
            return id;
        };

        auto SafeGetStaticField = [&](jclass cls, const char* name, const char* sig) -> jfieldID {
            jfieldID id = env->GetStaticFieldID(cls, name, sig);
            if (!id) {
                printf("[-] Static FieldID not found: %s %s\n", name, sig);
                env->ExceptionClear( );
            }
            return id;
        };

        // ── Alle benötigten Field/Method IDs ──
        // Minecraft fields
        jfieldID f_player = SafeGetField(c_Minecraft, "player",
                                            "Lnet/minecraft/client/player/LocalPlayer;");
        jfieldID f_level = SafeGetField(c_Minecraft, "level",
                                           "Lnet/minecraft/client/multiplayer/ClientLevel;");
        jfieldID f_gameMode = SafeGetField(c_Minecraft, "gameMode",
                                              "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;");
        jfieldID f_hitResult = SafeGetField(c_Minecraft, "hitResult",
                                               "Lnet/minecraft/world/phys/HitResult;");
        jfieldID f_window = SafeGetField(c_Minecraft, "window",
                                            "Lcom/mojang/blaze3d/platform/Window;");

        // Window
        jmethodID m_getHandle = SafeGetMethod(c_Window, "handle", "()J");

        // Player
        jfieldID f_inventory = SafeGetField(c_ClientPlayerEntity, "inventory",
                                               "Lnet/minecraft/world/entity/player/Inventory;");
        jmethodID m_swingHand = SafeGetMethod(c_ClientPlayerEntity, "swing",
                                                 "(Lnet/minecraft/world/InteractionHand;)V");

        // Inventory
        jmethodID m_hasItem = SafeGetMethod(c_PlayerInventory, "hasAnyMatching",
                                               "(Ljava/util/function/Predicate;)Z");
        // Einfachere Alternative: direkt nach END_CRYSTAL-Item suchen
        // → wir nutzen contains() via Reflection oder direkte Slot-Iteration

        // Items.END_CRYSTAL (static field)
        jfieldID f_endCrystalItem = SafeGetStaticField(c_Items, "END_CRYSTAL",
                                                          "Lnet/minecraft/world/item/Item;");
        jobject END_CRYSTAL_ITEM = env->NewGlobalRef(
            env->GetStaticObjectField(c_Items, f_endCrystalItem));

        // InteractionManager
        jmethodID m_attackEntity = SafeGetMethod(c_InteractionManager, "attack",
                                                    "(Lnet/minecraft/world/entity/player/Player;Lnet/minecraft/world/entity/Entity;)V");
        jmethodID m_interactBlock = SafeGetMethod(c_InteractionManager, "useItemOn",
                                                  "(Lnet/minecraft/client/player/LocalPlayer;"
                                                  "Lnet/minecraft/world/InteractionHand;"
                                                  "Lnet/minecraft/world/phys/BlockHitResult;)"
                                                  "Lnet/minecraft/world/InteractionResult;");

        // EntityHitResult
        jmethodID m_getEntity = SafeGetMethod(c_EntityHitResult, "getEntity",
                                                 "()Lnet/minecraft/world/entity/Entity;");

        // BlockHitResult
        jmethodID m_getBlockPos = SafeGetMethod(c_BlockHitResult, "getBlockPos",
                                                   "()Lnet/minecraft/core/BlockPos;");

        // BlockPos
        jmethodID m_blockPosDown = SafeGetMethod(c_BlockPos, "below",
                                                    "()Lnet/minecraft/core/BlockPos;");

        // ClientWorld
        jmethodID m_getBlockState = SafeGetMethod(c_ClientWorld, "getBlockState",
                                                     "(Lnet/minecraft/core/BlockPos;)Lnet/minecraft/world/level/block/state/BlockState;");

        // BlockState
        jmethodID m_getBlock = SafeGetMethod(c_BlockState, "getBlock",
                                                "()Lnet/minecraft/world/level/block/Block;");

        // Blocks.OBSIDIAN / BEDROCK (static fields)
        jfieldID f_obsidian = SafeGetStaticField(c_Blocks, "OBSIDIAN",
                                                    "Lnet/minecraft/world/level/block/Block;");
        jfieldID f_bedrock = SafeGetStaticField(c_Blocks, "BEDROCK",
                                                   "Lnet/minecraft/world/level/block/Block;");
        jobject OBSIDIAN = env->NewGlobalRef(env->GetStaticObjectField(c_Blocks, f_obsidian));
        jobject BEDROCK = env->NewGlobalRef(env->GetStaticObjectField(c_Blocks, f_bedrock));

        // Hand.MAIN_HAND (static field)
        jfieldID f_mainHand = env->GetStaticFieldID(c_Hand, "MAIN_HAND",
                                                    "Lnet/minecraft/world/InteractionHand;");
        jobject MAIN_HAND = env->NewGlobalRef(env->GetStaticObjectField(c_Hand, f_mainHand));

        // ActionResult
        jmethodID m_isAccepted = SafeGetMethod(c_ActionResult, "consumesAction", "()Z");
        jmethodID m_swingSide = SafeGetMethod(c_ActionResult, "shouldSwing", "()Z");

    //    printf("[+] CwCrystal: All method/field IDs resolved. Starting tick loop.\n");

        // ── Inventory-Hilfsfunktion: hat der Spieler END_CRYSTAL? ──
        // Wir iterieren über die 36 Hauptslots der Inventory-Klasse
        jfieldID f_items = SafeGetField(c_PlayerInventory, "items",
                                           "Lnet/minecraft/core/NonNullList;");
        jclass c_NonNullList = FindGlobal(env, "net/minecraft/core/NonNullList");
        jmethodID m_listGet = SafeGetMethod(c_NonNullList, "get", "(I)Ljava/lang/Object;");
        jmethodID m_listSize = SafeGetMethod(c_NonNullList, "size", "()I");

        jclass c_ItemStack = FindGlobal(env, "net/minecraft/world/item/ItemStack");
        jmethodID m_getItem = SafeGetMethod(c_ItemStack, "getItem",
                                               "()Lnet/minecraft/world/item/Item;");


        if (!f_player || !f_level || !f_gameMode || !f_hitResult) {
            printf("[-] CwCrystal: Critical field IDs missing, aborting!\n");
            p_jvm->DetachCurrentThread( );
            return;
        }

        printf("[+] CwCrystal: Entering tick loop\n");

        while (m_running) {
        //    Sleep(5); // ~20 ticks/s

            jobject mc = p_mc->GetInstance( ); // Getter für class_instance hinzufügen!
            if (!mc)
                continue;

            // ── player & world null-check ──
            jobject player = env->GetObjectField(mc, f_player);
            if (!player)
                continue;

            jobject world = env->GetObjectField(mc, f_level);
            if (!world) {
                env->DeleteLocalRef(player);
                continue;
            }

            // ── RMB gedrückt? (GLFW aus nativem Code) ──
            // Window-Handle holen
            jobject windowObj = env->GetObjectField(mc, f_window);
            jlong hwnd = env->CallLongMethod(windowObj, m_getHandle);
            env->DeleteLocalRef(windowObj);

            // GLFW direkt aus nativem Context aufrufen
            // (funktioniert da wir im gleichen Prozess sind)
            // RMB = Button 1
            if (GetAsyncKeyState(VK_RBUTTON) == 0) {
                env->DeleteLocalRef(player);
                env->DeleteLocalRef(world);
                continue;
            }

            // ── Hat Spieler END_CRYSTAL? ──
            jobject inventory = env->GetObjectField(player, f_inventory);
            jobject itemsList = env->GetObjectField(inventory, f_items);
            bool hasEndCrystal = false;
            jint slotCount = env->CallIntMethod(itemsList, m_listSize);

            for (jint i = 0; i < slotCount && !hasEndCrystal; i++) {
                jobject stack = env->CallObjectMethod(itemsList, m_listGet, i);
                if (!stack)
                    continue;
                jobject item = env->CallObjectMethod(stack, m_getItem);
                if (env->IsSameObject(item, END_CRYSTAL_ITEM))
                    hasEndCrystal = true;
                env->DeleteLocalRef(item);
                env->DeleteLocalRef(stack);
            }
            env->DeleteLocalRef(itemsList);
            env->DeleteLocalRef(inventory);

            if (!hasEndCrystal) {
                env->DeleteLocalRef(player);
                env->DeleteLocalRef(world);
                continue;
            }

            // ── crosshairTarget / hitResult ──
            jobject hitResult = env->GetObjectField(mc, f_hitResult);
            if (!hitResult) {
                env->DeleteLocalRef(player);
                env->DeleteLocalRef(world);
                continue;
            }

            jobject gameMode = env->GetObjectField(mc, f_gameMode);

            // ── Case 1: EntityHitResult ──
            if (env->IsInstanceOf(hitResult, c_EntityHitResult)) {
                jobject entity = env->CallObjectMethod(hitResult, m_getEntity);
                if (entity && env->IsInstanceOf(entity, c_EndCrystalEntity)) {
                    if (gameMode)
                        env->CallVoidMethod(gameMode, m_attackEntity, player, entity);
                    env->CallVoidMethod(player, m_swingHand, MAIN_HAND);
                }
                if (entity)
                    env->DeleteLocalRef(entity);
            }
            // ── Case 2: BlockHitResult ──
//            else if (env->IsInstanceOf(hitResult, c_BlockHitResult)) {
//                jobject blockPos = env->CallObjectMethod(hitResult, m_getBlockPos);
//                jobject blockPosDown = env->CallObjectMethod(blockPos, m_blockPosDown);
//
//                jobject stateBelow = env->CallObjectMethod(world, m_getBlockState, blockPosDown);
//                jobject stateTarget = env->CallObjectMethod(world, m_getBlockState, blockPos);
//
//                jobject blockBelow = env->CallObjectMethod(stateBelow, m_getBlock);
//                jobject blockTarget = env->CallObjectMethod(stateTarget, m_getBlock);
//
//                bool belowIsObsidian = env->IsSameObject(blockBelow, OBSIDIAN);
//                bool targetIsValid = env->IsSameObject(blockTarget, OBSIDIAN) || env->IsSameObject(blockTarget, BEDROCK);
//
//                if (belowIsObsidian && targetIsValid && gameMode) {
//                    jobject result = env->CallObjectMethod(
//                        gameMode, m_interactBlock, player, MAIN_HAND, hitResult);
//                    if (result) {
//                        bool accepted = env->CallBooleanMethod(result, m_isAccepted);
//                        if (accepted) // ← shouldSwing weg, nur noch accepted prüfen
//                            env->CallVoidMethod(player, m_swingHand, MAIN_HAND);
//                       env->DeleteLocalRef(result);
//                    }
//                }
//
//                env->DeleteLocalRef(blockTarget);
//                env->DeleteLocalRef(blockBelow);
//                env->DeleteLocalRef(stateTarget);
//                env->DeleteLocalRef(stateBelow);
//                env->DeleteLocalRef(blockPosDown);
//                env->DeleteLocalRef(blockPos);
//            }

            // ── Cleanup ──
            if (gameMode)
                env->DeleteLocalRef(gameMode);
            env->DeleteLocalRef(hitResult);
            env->DeleteLocalRef(world);
            env->DeleteLocalRef(player);
        }

        p_jvm->DetachCurrentThread( );
    }
};
