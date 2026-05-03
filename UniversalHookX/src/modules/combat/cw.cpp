// CwCrystal.cpp
// Crystal-PvP automation module translated from Fabric/Java to C++
// Designed for injection-based Minecraft clients (e.g. with MinHook / custom SDK)
//
// Requirements:
//   - A Minecraft C++ SDK / offset layer that exposes:
//       MC::getLocalPlayer(), MC::getWorld(), MC::getInteractionManager()
//       MC::getCrosshairTarget(), MC::getWindow()
//   - GLFW linked (or swap glfwGetMouseButton with your client's input API)
//   - Called once per game tick (e.g. from a hooked GameRenderer::tick or similar)

#pragma once
#include <memory>

// ── Adjust these headers to match your client's SDK ──────────────────────────
#include "sdk/BlockPos.hpp"           // BlockPos, BlockPos::down()
#include "sdk/Blocks.hpp"             // Blocks::OBSIDIAN, Blocks::BEDROCK, Blocks::AIR
#include "sdk/Entities.hpp"           // Entity, EndCrystalEntity
#include "sdk/Hand.hpp"               // Hand::MAIN_HAND
#include "sdk/HitResult.hpp"          // HitResult, BlockHitResult, EntityHitResult
#include "sdk/InteractionManager.hpp" // InteractionManager
#include "sdk/Inventory.hpp"          // ItemStack, Items
#include "sdk/MinecraftClient.hpp"    // MC namespace, MinecraftClient singleton
#include "sdk/Player.hpp"             // LocalPlayer, PlayerEntity
#include "sdk/World.hpp"              // ClientWorld, BlockState
#include <GLFW/glfw3.h>
// ─────────────────────────────────────────────────────────────────────────────

namespace CwCrystal {

    // ── Helpers ───────────────────────────────────────────────────────────────

    // Returns true when the right mouse button is physically held down.
    static bool isRightMouseHeld( ) {
        GLFWwindow* window = MC::MinecraftClient::get( )->getWindow( )->getHandle( );
        return glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    }

    // Returns true when the local player has at least one End Crystal in any slot.
    static bool playerHasEndCrystal(MC::LocalPlayer* player) {
        return player->getInventory( )->contains(MC::Items::END_CRYSTAL);
    }

    // ── Crystal attack (Case 1) ───────────────────────────────────────────────

    // Attacks an End Crystal entity and swings the main hand.
    static void attackCrystal(MC::LocalPlayer* player,
                              MC::InteractionManager* im,
                              MC::EndCrystalEntity* crystal) {
        im->attackEntity(static_cast<MC::PlayerEntity*>(player), crystal);
        player->swingHand(MC::Hand::MAIN_HAND);
    }

    // ── Crystal placement (Case 2) ────────────────────────────────────────────

    // Returns true when the block at `pos` is Obsidian or Bedrock.
    static bool isObsidianOrBedrock(MC::ClientWorld* world, const MC::BlockPos& pos) {
        const MC::Block* block = world->getBlockState(pos).getBlock( );
        return block == MC::Blocks::OBSIDIAN || block == MC::Blocks::BEDROCK;
    }

    // Attempts to place an End Crystal on a valid Obsidian/Bedrock surface.
    // Valid surface: the block itself is Obsidian/Bedrock AND the block below it
    // is also Obsidian (mirrors vanilla End Crystal placement rules used in PvP).
    static void tryPlaceCrystal(MC::LocalPlayer* player,
                                MC::InteractionManager* im,
                                MC::ClientWorld* world,
                                MC::BlockHitResult* bhr) {
        const MC::BlockPos& targetPos = bhr->getBlockPos( );
        const MC::BlockPos belowPos = targetPos.down( );

        bool belowIsObsidian = world->getBlockState(belowPos).getBlock( ) == MC::Blocks::OBSIDIAN;
        bool targetIsValidSurf = isObsidianOrBedrock(world, targetPos);

        if (!belowIsObsidian || !targetIsValidSurf)
            return;

        MC::ActionResult result = im->interactBlock(player, MC::Hand::MAIN_HAND, bhr);

        if (result.isAccepted( ) && result.shouldSwingHand( ))
            player->swingHand(MC::Hand::MAIN_HAND);
    }

    // ── Main tick function ────────────────────────────────────────────────────
    // Call this once per client tick from your hook.

    void onClientTick( ) {
        MC::MinecraftClient* mc = MC::MinecraftClient::get( );

        // Guard: player and world must exist
        MC::LocalPlayer* player = mc->getLocalPlayer( );
        MC::ClientWorld* world = mc->getWorld( );
        if (!player || !world)
            return;

        // Guard: right mouse button must be held
        if (!isRightMouseHeld( ))
            return;

        // Guard: player must carry at least one End Crystal
        if (!playerHasEndCrystal(player))
            return;

        MC::InteractionManager* im = mc->getInteractionManager( );
        if (!im)
            return;

        MC::HitResult* hit = mc->getCrosshairTarget( );
        if (!hit)
            return;

        // ── Case 1: Looking at an entity ─────────────────────────────────────
        if (auto* ehr = dynamic_cast<MC::EntityHitResult*>(hit)) {
            if (auto* crystal = dynamic_cast<MC::EndCrystalEntity*>(ehr->getEntity( ))) {
                attackCrystal(player, im, crystal);
                return;
            }
        }

        // ── Case 2: Looking at a block ────────────────────────────────────────
        if (auto* bhr = dynamic_cast<MC::BlockHitResult*>(hit)) {
            tryPlaceCrystal(player, im, world, bhr);
        }
    }

} // namespace CwCrystal
