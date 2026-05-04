#include <cstdio>
#include <mutex>
#include <thread>

#include "hooks.hpp"
#include "../dependencies/imgui/imgui.h"

#include "backend/opengl/hook_opengl.hpp"
#include "backend/vulkan/hook_vulkan.hpp"

#include "../console/console.hpp"
#include "../menu/menu.hpp"
#include "../utils/utils.hpp"

#include "../dependencies/minhook/MinHook.h"

static HWND g_hWindow = NULL;
static std::mutex g_mReinitHooksGuard;

static DWORD WINAPI ReinitializeGraphicalHooks(LPVOID lpParam) {
    std::lock_guard<std::mutex> guard{g_mReinitHooksGuard};

    LOG("[!] Hooks will reinitialize!\n");

    HWND hNewWindow = U::GetProcessWindow( );
    while (hNewWindow == reinterpret_cast<HWND>(lpParam)) {
        hNewWindow = U::GetProcessWindow( );
    }

    H::bShuttingDown = true;

    H::Free( );
    H::Init( );

    H::bShuttingDown = false;
    Menu::bShowMenu = true;

    return 0;
}

static WNDPROC oWndProc;
static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_INSERT) {
            Menu::bShowMenu = !Menu::bShowMenu;

            // Cursor sichtbar/unsichtbar beim Toggle
            ImGuiIO& io = ImGui::GetIO( );
            io.MouseDrawCursor = Menu::bShowMenu;

            return 0;
        } else if (wParam == VK_HOME) {
            HANDLE hHandle = CreateThread(NULL, 0, ReinitializeGraphicalHooks, NULL, 0, NULL);
            if (hHandle != NULL)
                CloseHandle(hHandle);
            return 0;
        } else if (wParam == VK_DELETE) {
            H::bShuttingDown = true;
            U::UnloadDLL( );
            return 0;
        }
    } else if (uMsg == WM_DESTROY) {
        HANDLE hHandle = CreateThread(NULL, 0, ReinitializeGraphicalHooks, hWnd, 0, NULL);
        if (hHandle != NULL)
            CloseHandle(hHandle);
    }

    LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    if (Menu::bShowMenu) {
        // Rückgabewert prüfen - wenn ImGui den Input will, NICHT weiterleiten
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return true;

        // Zusätzlich: Maus-Events blockieren wenn ImGui sie haben will
        ImGuiIO& io = ImGui::GetIO( );
        switch (uMsg) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_MOUSEMOVE:
                if (io.WantCaptureMouse)
                    return 0;
                break;
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
                if (io.WantCaptureKeyboard)
                    return 0;
                break;
        }
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

namespace Hooks {
    void Init( ) {
        g_hWindow = U::GetProcessWindow( );

#ifdef DISABLE_LOGGING_CONSOLE
        bool bNoConsole = GetConsoleWindow( ) == NULL;
        if (bNoConsole) {
            AllocConsole( );
        }
#endif

        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend( );
        switch (eRenderingBackend) {
            case OPENGL:
                GL::Hook(g_hWindow);
                break;
            case VULKAN:
                VK::Hook(g_hWindow);
                break;
        }

#ifdef DISABLE_LOGGING_CONSOLE
        if (bNoConsole) {
            FreeConsole( );
        }
#endif

        oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    }

    void Free( ) {
        if (oWndProc) {
            SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        }

        MH_DisableHook(MH_ALL_HOOKS);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend( );
        switch (eRenderingBackend) {
                break;
            case OPENGL:
                GL::Unhook( );
                break;
            case VULKAN:
                VK::Unhook( );
                break;
        }
    }
} // namespace Hooks
