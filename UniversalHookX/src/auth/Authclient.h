#pragma once
#include <string>

struct LicenseInfo
{
    bool success;
    int daysLeft;  // -1 = permanent/unbegrenzt
    std::string expiryDate;
    std::string errorMessage;
};

bool LoginRequest(const std::string& user, const std::string& passHash);
std::string LoginRequestWithResponse(const std::string& user, const std::string& passHash, const std::string& hwid);

// Neue Funktion: Parst die Response und gibt LicenseInfo zurück
LicenseInfo ParseLoginResponse(const std::string& response);
