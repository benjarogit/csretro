// The one ELF64 libsteam_api.so. Valve's i386 library is not loaded. Steam is
// not started, credentials are not stored, and a missing interface is not called.

#include "csretro_steam_api.h"

#include <steam/steam_api.h>

#include <cstring>

namespace
{

    class LoggedOutUser final : public ISteamUser
    {
    public:
        HSteamUser GetHSteamUser() override
        {
            return 0;
        }

        bool BLoggedOn() override
        {
            return false;
        }

        CSteamID GetSteamID() override
        {
            return k_steamIDNil;
        }

        int InitiateGameConnection(void*, int, CSteamID, uint32, uint16, bool) override
        {
            return 0;
        }

        void TerminateGameConnection(uint32, uint16) override
        {
        }

        void TrackAppUsageEvent(CGameID, int, const char*) override
        {
        }

        bool GetUserDataFolder(char* buffer, int size) override
        {
            if (buffer != nullptr && size > 0)
            {
                buffer[0] = '\0';
            }
            return false;
        }

        void StartVoiceRecording() override
        {
        }

        void StopVoiceRecording() override
        {
        }

        EVoiceResult GetAvailableVoice(uint32* compressed, uint32* uncompressed, uint32) override
        {
            if (compressed != nullptr)
            {
                *compressed = 0;
            }
            if (uncompressed != nullptr)
            {
                *uncompressed = 0;
            }
            return k_EVoiceResultNotRecording;
        }

        EVoiceResult GetVoice(bool, void*, uint32, uint32* written, bool, void*, uint32, uint32* uncompressed_written, uint32) override
        {
            if (written != nullptr)
            {
                *written = 0;
            }
            if (uncompressed_written != nullptr)
            {
                *uncompressed_written = 0;
            }
            return k_EVoiceResultNotRecording;
        }

        EVoiceResult DecompressVoice(const void*, uint32, void*, uint32, uint32* written, uint32) override
        {
            if (written != nullptr)
            {
                *written = 0;
            }
            return k_EVoiceResultNotRecording;
        }

        uint32 GetVoiceOptimalSampleRate() override
        {
            return 0;
        }

        HAuthTicket GetAuthSessionTicket(void*, int, uint32* ticket_size) override
        {
            if (ticket_size != nullptr)
            {
                *ticket_size = 0;
            }
            return k_HAuthTicketInvalid;
        }

        EBeginAuthSessionResult BeginAuthSession(const void*, int, CSteamID) override
        {
            return k_EBeginAuthSessionResultInvalidTicket;
        }

        void EndAuthSession(CSteamID) override
        {
        }

        void CancelAuthTicket(HAuthTicket) override
        {
        }

        EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID, AppId_t) override
        {
            return k_EUserHasLicenseResultDoesNotHaveLicense;
        }

        bool BIsBehindNAT() override
        {
            return false;
        }

        void AdvertiseGame(CSteamID, uint32, uint16) override
        {
        }

        SteamAPICall_t RequestEncryptedAppTicket(void*, int) override
        {
            return k_uAPICallInvalid;
        }

        bool GetEncryptedAppTicket(void*, int, uint32* ticket_size) override
        {
            if (ticket_size != nullptr)
            {
                *ticket_size = 0;
            }
            return false;
        }
    };

    class LoggedOutApps final : public ISteamApps
    {
    public:
        bool BIsSubscribed() override
        {
            return false;
        }

        bool BIsLowViolence() override
        {
            return false;
        }

        bool BIsCybercafe() override
        {
            return false;
        }

        bool BIsVACBanned() override
        {
            return false;
        }

        const char* GetCurrentGameLanguage() override
        {
            return "";
        }

        const char* GetAvailableGameLanguages() override
        {
            return "";
        }

        bool BIsSubscribedApp(AppId_t) override
        {
            return false;
        }

        bool BIsDlcInstalled(AppId_t) override
        {
            return false;
        }

        uint32 GetEarliestPurchaseUnixTime(AppId_t) override
        {
            return 0;
        }

        bool BIsSubscribedFromFreeWeekend() override
        {
            return false;
        }

        int GetDLCCount() override
        {
            return 0;
        }

        bool BGetDLCDataByIndex(int, AppId_t*, bool* available, char* name, int name_size) override
        {
            if (available != nullptr)
            {
                *available = false;
            }
            if (name != nullptr && name_size > 0)
            {
                name[0] = '\0';
            }
            return false;
        }

        void InstallDLC(AppId_t) override
        {
        }

        void UninstallDLC(AppId_t) override
        {
        }

        void RequestAppProofOfPurchaseKey(AppId_t) override
        {
        }

        bool GetCurrentBetaName(char* name, int name_size) override
        {
            if (name != nullptr && name_size > 0)
            {
                name[0] = '\0';
            }
            return false;
        }

        bool MarkContentCorrupt(bool) override
        {
            return false;
        }

        uint32 GetInstalledDepots(DepotId_t*, uint32) override
        {
            return 0;
        }

        uint32 GetAppInstallDir(AppId_t, char* folder, uint32 folder_size) override
        {
            if (folder != nullptr && folder_size > 0)
            {
                folder[0] = '\0';
            }
            return 0;
        }
    };

    LoggedOutUser& User()
    {
        static LoggedOutUser user;
        return user;
    }

    LoggedOutApps& Apps()
    {
        static LoggedOutApps apps;
        return apps;
    }

} // namespace

extern "C" void* csr_steam_proxy_interface(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    if (std::strcmp(name, STEAMUSER_INTERFACE_VERSION) == 0)
    {
        return &User();
    }
    if (std::strcmp(name, STEAMAPPS_INTERFACE_VERSION) == 0)
    {
        return &Apps();
    }
    return nullptr;
}

extern "C" void csr_steam_proxy_persona(csr::SteamPersona* persona)
{
    if (persona == nullptr)
    {
        return;
    }
    *persona = {};
    persona->logged_in = false;
}

S_API bool SteamAPI_Init()
{
    return false;
}

S_API void SteamAPI_Shutdown()
{
}

S_API bool SteamAPI_IsSteamRunning()
{
    return false;
}

S_API bool SteamAPI_RestartAppIfNecessary(uint32)
{
    return false;
}

S_API void SteamAPI_RunCallbacks()
{
}

S_API ISteamUser* SteamUser()
{
    return &User();
}

S_API ISteamApps* SteamApps()
{
    return &Apps();
}

S_API ISteamUtils* SteamUtils()
{
    return nullptr;
}

S_API ISteamMatchmaking* SteamMatchmaking()
{
    return nullptr;
}

S_API ISteamMatchmakingServers* SteamMatchmakingServers()
{
    return nullptr;
}
