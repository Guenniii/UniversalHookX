#pragma once

#include <Windows.h>
#include <string>

namespace Menu {
    void InitializeContext(HWND hwnd);
    void Render( );

    inline bool bShowMenu = true;
} // namespace Menu
