#include "HWID.h"
#include "Hash.h"
#include <Windows.h>
#include <intrin.h>

std::string GetVolumeID()
{
    DWORD serial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    return std::to_string(serial);
}

std::string GetCPU()
{
    int cpuInfo[4]{};
    __cpuid(cpuInfo, 0);
    return std::to_string(cpuInfo[0]) +
        std::to_string(cpuInfo[1]) +
        std::to_string(cpuInfo[2]) +
        std::to_string(cpuInfo[3]);
}

std::string GetMachineGUID()
{
    HKEY key;
    char value[64];
    DWORD size = sizeof(value);

    RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography",
        0,
        KEY_READ | KEY_WOW64_64KEY,
        &key);

    RegQueryValueExA(key, "MachineGuid", nullptr, nullptr, (LPBYTE)value, &size);
    RegCloseKey(key);

    return std::string(value);
}

std::string GetHWID()
{
    std::string raw =
        GetVolumeID() +
        GetCPU() +
        GetMachineGUID();

    return Sha256(raw); // NIE Klartext senden!
}
