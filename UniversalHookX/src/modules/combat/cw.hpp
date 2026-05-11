#pragma once
#include "../ModulBase.hpp"

class CW : public ModuleBase
{
public:
    virtual void Update( );
    virtual void RenderOverlay( );
    virtual void RenderHud( );

    virtual void RenderMenu( );

    virtual std::string GetName( );
    virtual std::string GetCategory( );
    virtual int GetKey( );

    virtual bool IsEnabled( );
    virtual void SetEnabled(bool enabled);
    virtual void Toggle( );
    
private:
    std::string CW;
    std::string combat;
};
