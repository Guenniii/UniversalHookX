#pragma once

#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h"
#include <Windows.h>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

class AutoTotem final {
public:
    AutoTotem(JavaVM* jvm, CMinecraft* mc) : p_jvm(jvm), p_mc(mc) { }

    bool Init( ) {
        if (m_initialized)
            return true;
        JNIEnv* env = nullptr;
        if (p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK || !env)
            return false;

        std::printf("[*] AutoTotem: Finaler Snapshot 26.2 Init...\n");

        auto GetClass = [&](const char* name) -> jclass {
            jclass l = env->FindClass(name);
            if (env->ExceptionCheck( )) {
                env->ExceptionClear( );
                return nullptr;
            }
            jclass g = (jclass)env->NewGlobalRef(l);
            env->DeleteLocalRef(l);
            return g;
        };

        c_minecraft = GetClass("net/minecraft/client/Minecraft");
        c_player = GetClass("net/minecraft/client/player/LocalPlayer");
        c_items = GetClass("net/minecraft/world/item/Items");
        c_item_stack = GetClass("net/minecraft/world/item/ItemStack");
        c_multiplayer_mode = GetClass("net/minecraft/client/multiplayer/MultiPlayerGameMode");
        c_inv_screen = GetClass("net/minecraft/client/gui/screens/inventory/InventoryScreen");
        c_abstract_menu = GetClass("net/minecraft/world/inventory/AbstractContainerMenu");

        const char* paths[] = {
            "net/minecraft/world/inventory/ClickType",
            "net/minecraft/world/inventory/ContainerInput"};
        for (const char* p : paths) {
            c_click_type = GetClass(p);
            if (c_click_type) {
                m_click_path = p;
                break;
            }
        }

        if (!c_minecraft || !c_player || !c_click_type)
            return false;

        f_player = env->GetFieldID(c_minecraft, "player", "Lnet/minecraft/client/player/LocalPlayer;");
        f_game_mode = env->GetFieldID(c_minecraft, "gameMode", "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;");
        f_gui = env->GetFieldID(c_minecraft, "gui", "Lnet/minecraft/client/gui/screens/Screen;");
        f_inventory_menu = env->GetFieldID(c_player, "inventoryMenu", "Lnet/minecraft/world/inventory/InventoryMenu;");
        if (!f_inventory_menu) {
            env->ExceptionClear( );
            f_inventory_menu = env->GetFieldID(c_player, "containerMenu", "Lnet/minecraft/world/inventory/AbstractContainerMenu;");
        }

        f_container_id = env->GetFieldID(c_abstract_menu, "containerId", "I");
        f_totem_static = env->GetStaticFieldID(c_items, "TOTEM_OF_UNDYING", "Lnet/minecraft/world/item/Item;");

        m_set_screen = env->GetMethodID(c_minecraft, "setScreen", "(Lnet/minecraft/client/gui/screens/Screen;)V");
        m_inv_screen_init = env->GetMethodID(c_inv_screen, "<init>", "(Lnet/minecraft/world/entity/player/Player;)V");
        m_stack_is_empty = env->GetMethodID(c_item_stack, "isEmpty", "()Z");
        m_stack_get_item = env->GetMethodID(c_item_stack, "getItem", "()Lnet/minecraft/world/item/Item;");

        std::string click_sig = "(IIIL" + m_click_path + ";Lnet/minecraft/world/entity/player/Player;)V";
        m_handle_inv_click = env->GetMethodID(c_multiplayer_mode, "handleInventoryMouseClick", click_sig.c_str( ));
        if (!m_handle_inv_click) {
            env->ExceptionClear( );
            m_handle_inv_click = env->GetMethodID(c_multiplayer_mode, "handleContainerInput", click_sig.c_str( ));
        }

        jfieldID fid_pickup = env->GetStaticFieldID(c_click_type, "PICKUP", ("L" + m_click_path + ";").c_str( ));
        if (fid_pickup)
            o_pickup = env->NewGlobalRef(env->GetStaticObjectField(c_click_type, fid_pickup));

        jobject totem_obj = env->GetStaticObjectField(c_items, f_totem_static);
        if (totem_obj)
            o_totem = env->NewGlobalRef(totem_obj);

        m_initialized = (o_totem && o_pickup && m_handle_inv_click);
        if (m_initialized)
            std::printf("[+] AutoTotem: System bereit für Snapshot 3!\n");

        return m_initialized;
    }

    void Tick( ) {
        if (!Init( ))
            return;
        JNIEnv* env = nullptr;
        if (p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK)
            return;

        jobject mc = p_mc->GetInstance( );
        jobject player = env->GetObjectField(mc, f_player);
        if (!player)
            return;

        if (!HasOffhandTotem(env, player)) {
            if (!m_is_refilling) {
                m_is_refilling = true;
                m_last_action = std::chrono::steady_clock::now( );
            }

            auto now = std::chrono::steady_clock::now( );
            // Erhöht auf 200ms für stabilere Server-Synchronisation
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_action).count( ) > 200) {
                OpenInventory(env, mc);
                printf("[*] AutoTotem: Inventar geöffnet, suche Totem...\n");
                Sleep(100); // Kurze Pause, damit das Inventar vollständig geöffnet ist (kann je nach System variieren)
                MoveTotem(env, mc, player);
                Sleep(100); // Kurze Pause, damit die Klicks verarbeitet werden
                CloseInventory(env, mc);
                m_is_refilling = false;
            }
        } else {
            m_is_refilling = false;
        }

        env->DeleteLocalRef(player);
    }
private:
    bool HasOffhandTotem(JNIEnv* env, jobject player) {
        jclass c_p = env->GetObjectClass(player);
        jmethodID m_offhand = env->GetMethodID(c_p, "getOffhandItem", "()Lnet/minecraft/world/item/ItemStack;");
        jobject stack = env->CallObjectMethod(player, m_offhand);
        if (!stack) {
            env->DeleteLocalRef(c_p);
            return false;
        }
        jboolean empty = env->CallBooleanMethod(stack, m_stack_is_empty);
        jobject item = env->CallObjectMethod(stack, m_stack_get_item);
        bool result = (!empty && item && env->IsSameObject(item, o_totem));
        env->DeleteLocalRef(item);
        env->DeleteLocalRef(stack);
        env->DeleteLocalRef(c_p);
        return result;
    }

    void MoveTotem(JNIEnv* env, jobject mc, jobject player) {
        jclass c_p = env->GetObjectClass(player);
        jmethodID m_get_inv = env->GetMethodID(c_p, "getInventory", "()Lnet/minecraft/world/entity/player/Inventory;");
        jobject inv = env->CallObjectMethod(player, m_get_inv);
        jclass c_inv = env->GetObjectClass(inv);
        jmethodID m_get_item = env->GetMethodID(c_inv, "getItem", "(I)Lnet/minecraft/world/item/ItemStack;");

        int totem_slot = -1;
        for (int i = 0; i < 36; i++) {
            jobject s = env->CallObjectMethod(inv, m_get_item, i);
            if (!s)
                continue;
            jobject it = env->CallObjectMethod(s, m_stack_get_item);
            if (it && env->IsSameObject(it, o_totem)) {
                totem_slot = i;
                env->DeleteLocalRef(it);
                env->DeleteLocalRef(s);
                break;
            }
            if (it)
                env->DeleteLocalRef(it);
            env->DeleteLocalRef(s);
        }

        if (totem_slot != -1) {
            // Inventar Screen öffnen, falls nicht offen (erforderlich für Klicks)
            jobject current = env->GetObjectField(mc, f_gui);
            if (!current) {
                jobject screen = env->NewObject(c_inv_screen, m_inv_screen_init, player);
                env->CallVoidMethod(mc, m_set_screen, screen);
                env->DeleteLocalRef(screen);
            }

            jobject gm = env->GetObjectField(mc, f_game_mode);
            jobject menu = env->GetObjectField(player, f_inventory_menu);
            jint cid = env->GetIntField(menu, f_container_id);

            // WICHTIG: Slot-Konvertierung
            // Inventory Slot 0-8 (Hotbar) -> Container Slot 36-44
            // Inventory Slot 9-35 (Main)  -> Container Slot 9-35
            int server_slot = (totem_slot < 9) ? totem_slot + 36 : totem_slot;
            int offhand_slot = 45;

            // Sequenz: Pick Item -> Offhand Slot -> Drop rest back
            env->CallVoidMethod(gm, m_handle_inv_click, cid, server_slot, 0, o_pickup, player);
            env->CallVoidMethod(gm, m_handle_inv_click, cid, offhand_slot, 0, o_pickup, player);
            env->CallVoidMethod(gm, m_handle_inv_click, cid, server_slot, 0, o_pickup, player);

            std::printf("[SUCCESS] Totem aus Slot %d (Server: %d) verschoben!\n", totem_slot, server_slot);

            env->DeleteLocalRef(gm);
            env->DeleteLocalRef(menu);
        }
        env->DeleteLocalRef(c_inv);
        env->DeleteLocalRef(inv);
        env->DeleteLocalRef(c_p);
    }

    void OpenInventory(JNIEnv* env, jobject minecraftInstance) {
        // 1. Benötigte Klassen finden
        jclass mcClass = env->GetObjectClass(minecraftInstance);
        // Pfad laut deinem Import: net.minecraft.client.gui.Gui
        jclass guiClass = env->FindClass("net/minecraft/client/gui/Gui");
        // Pfad für das Inventar-Screen
        jclass invScreenClass = env->FindClass("net/minecraft/client/gui/screens/inventory/InventoryScreen");
        jclass playerClass = env->FindClass("net/minecraft/client/player/LocalPlayer");

        // 2. Die 'gui' Instanz aus Minecraft holen
        // Hinweis: Prüfe mit einem Mapper, ob das Feld in deiner JAR "gui" oder z.B. "f_91064_" heißt
        jfieldID guiFieldID = env->GetFieldID(mcClass, "gui", "Lnet/minecraft/client/gui/Gui;");
        jobject guiObj = env->GetObjectField(minecraftInstance, guiFieldID);

        // 3. Den LocalPlayer holen (wird für den Konstruktor des InventoryScreens benötigt)
        jfieldID playerFieldID = env->GetFieldID(mcClass, "player", "Lnet/minecraft/client/player/LocalPlayer;");
        jobject playerObj = env->GetObjectField(minecraftInstance, playerFieldID);

        // 4. InventoryScreen instanziieren
        // Konstruktor-Signatur: InventoryScreen(Player player)
        jmethodID invConstructor = env->GetMethodID(invScreenClass, "<init>", "(Lnet/minecraft/world/entity/player/Player;)V");
        jobject invScreenObj = env->NewObject(invScreenClass, invConstructor, playerObj);

        // 5. setScreen(Screen screen) in der Gui-Klasse aufrufen
        // Laut deinem Code: public void setScreen(Screen screen)
        jmethodID setScreenMethod = env->GetMethodID(guiClass, "setScreen", "(Lnet/minecraft/client/gui/screens/Screen;)V");
        ;
        if (setScreenMethod && guiObj) {
            env->CallVoidMethod(guiObj, setScreenMethod, invScreenObj);
        } else if (setScreenMethod && !guiObj) {
            std::printf("[-] Failed to open inventory screen: Method found but instance not created.\n");
        } else if (!setScreenMethod && guiObj) {
            std::printf("[-] Failed to open inventory screen: Method not found but instance created.\n");
        } else {
            std::printf("[-] Failed to open inventory screen: Method or instance not found.\n");
        }
    //    env->CallVoidMethod(minecraftInstance, setScreenMethod, guiInstance);

// Cleanup
        env->DeleteLocalRef(invScreenObj);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(guiObj);
    }

    void CloseInventory(JNIEnv* env, jobject minecraftInstance) {
        // 1. Benötigte Klassen finden
        jclass mcClass = env->GetObjectClass(minecraftInstance);
        jclass guiClass = env->FindClass("net/minecraft/client/gui/Gui");

        // 2. Die 'gui' Instanz aus Minecraft holen
        jfieldID guiFieldID = env->GetFieldID(mcClass, "gui", "Lnet/minecraft/client/gui/Gui;");
        jobject guiObj = env->GetObjectField(minecraftInstance, guiFieldID);

        // 3. Die setScreen Methode finden
        jmethodID setScreenMethod = env->GetMethodID(guiClass, "setScreen", "(Lnet/minecraft/client/gui/screens/Screen;)V");

        if (setScreenMethod && guiObj) {
            // Wir übergeben 'nullptr' als Screen-Argument, um das GUI zu schließen
            env->CallVoidMethod(guiObj, setScreenMethod, nullptr);
        } else {
            std::printf("[-] Failed to close inventory: Gui instance or setScreen method not found.\n");
        }

        // Cleanup
        env->DeleteLocalRef(guiObj);
        env->DeleteLocalRef(mcClass);
        // mcClass wurde via GetObjectClass erstellt, sollte ebenfalls aufgeräumt werden,
        // falls diese Funktion oft aufgerufen wird.
    }

    JavaVM* p_jvm;
    CMinecraft* p_mc;
    bool m_initialized = false;
    bool m_is_refilling = false;
    std::chrono::steady_clock::time_point m_last_action;
    jclass c_minecraft, c_player, c_items, c_item_stack, c_multiplayer_mode, c_click_type, c_inv_screen, c_abstract_menu;
    jfieldID f_player, f_gui, f_game_mode, f_inventory_menu, f_container_id, f_totem_static;
    jmethodID m_set_screen, m_inv_screen_init, m_stack_is_empty, m_stack_get_item, m_handle_inv_click;
    jobject o_totem = nullptr, o_pickup = nullptr;
    std::string m_click_path;
};
