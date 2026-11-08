/*
 * Steam stub for the CSRETRO test server.
 *
 * ReHLDS 3.15 was built against the pre-25th-anniversary Steamworks SDK: it
 * imports SteamGameServer_Init, SteamGameServer and SteamApps, and it expects
 * the ISteamGameServer011 interface. The Default Public libsteam_api.so that
 * App 90 ships only exports the newer SteamInternal_GameServer_Init and
 * SteamGameServer015, and the two vtable layouts are not interchangeable.
 * Loading the new library directly either crashes in
 * CSteam3Server::Activate or floods the log with interface-map assertions.
 *
 * The test server is LAN-only and runs with -insecure, so Steam authentication
 * is not part of what it verifies. This preloaded library therefore answers the
 * old ABI itself instead of forwarding to the Steam client:
 *
 *   - SteamGameServer_Init reports success and SteamGameServer() returns a
 *     no-op ISteamGameServer011 implementation.
 *   - The user API entry points report "Steam not running", which is the path
 *     ReGameDLL already handles when no Steam client is installed.
 *
 * Nothing here is used by a production server; the interface order below is a
 * transcription of rehlds/public/steam/isteamgameserver.h (MIT) at tag
 * 3.15.0.896.
 */

#include <cstdint>

namespace {

using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using AppId_t = uint32;
using HAuthTicket = uint32;
using SteamAPICall_t = uint64;
using EBeginAuthSessionResult = int;
using EUserHasLicenseForAppResult = int;

// CSteamID is an eight-byte value type in the SDK; only its size and its
// by-value ABI matter to the stub.
struct CSteamID {
    uint64 id;

    CSteamID() : id(0) {}
    explicit CSteamID(uint64 value) : id(value) {}
};

class ISteamGameServer {
public:
    virtual bool InitGameServer(uint32 unIP, uint16 usGamePort, uint16 usQueryPort,
                                uint32 unFlags, AppId_t nGameAppId,
                                const char *pchVersionString) = 0;
    virtual void SetProduct(const char *pszProduct) = 0;
    virtual void SetGameDescription(const char *pszGameDescription) = 0;
    virtual void SetModDir(const char *pszModDir) = 0;
    virtual void SetDedicatedServer(bool bDedicated) = 0;
    virtual void LogOn(const char *pszAccountName, const char *pszPassword) = 0;
    virtual void LogOnAnonymous() = 0;
    virtual void LogOff() = 0;
    virtual bool BLoggedOn() = 0;
    virtual bool BSecure() = 0;
    virtual CSteamID GetSteamID() = 0;
    virtual bool WasRestartRequested() = 0;
    virtual void SetMaxPlayerCount(int cPlayersMax) = 0;
    virtual void SetBotPlayerCount(int cBotplayers) = 0;
    virtual void SetServerName(const char *pszServerName) = 0;
    virtual void SetMapName(const char *pszMapName) = 0;
    virtual void SetPasswordProtected(bool bPasswordProtected) = 0;
    virtual void SetSpectatorPort(uint16 unSpectatorPort) = 0;
    virtual void SetSpectatorServerName(const char *pszSpectatorServerName) = 0;
    virtual void ClearAllKeyValues() = 0;
    virtual void SetKeyValue(const char *pKey, const char *pValue) = 0;
    virtual void SetGameTags(const char *pchGameTags) = 0;
    virtual void SetGameData(const char *pchGameData) = 0;
    virtual void SetRegion(const char *pszRegion) = 0;
    virtual bool SendUserConnectAndAuthenticate(uint32 unIPClient, const void *pvAuthBlob,
                                                uint32 cubAuthBlobSize,
                                                CSteamID *pSteamIDUser) = 0;
    virtual CSteamID CreateUnauthenticatedUserConnection() = 0;
    virtual void SendUserDisconnect(CSteamID steamIDUser) = 0;
    virtual bool BUpdateUserData(CSteamID steamIDUser, const char *pchPlayerName,
                                 uint32 uScore) = 0;
    virtual HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket,
                                             uint32 *pcbTicket) = 0;
    virtual EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket,
                                                     CSteamID steamID) = 0;
    virtual void EndAuthSession(CSteamID steamID) = 0;
    virtual void CancelAuthTicket(HAuthTicket hAuthTicket) = 0;
    virtual EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID,
                                                             AppId_t appID) = 0;
    virtual bool RequestUserGroupStatus(CSteamID steamIDUser, CSteamID steamIDGroup) = 0;
    virtual void GetGameplayStats() = 0;
    virtual SteamAPICall_t GetServerReputation() = 0;
    virtual uint32 GetPublicIP() = 0;
    virtual bool HandleIncomingPacket(const void *pData, int cbData, uint32 srcIP,
                                      uint16 srcPort) = 0;
    virtual int GetNextOutgoingPacket(void *pOut, int cbMaxOut, uint32 *pNetAdr,
                                      uint16 *pPort) = 0;
    virtual void EnableHeartbeats(bool bActive) = 0;
    virtual void SetHeartbeatInterval(int iHeartbeatInterval) = 0;
    virtual void ForceHeartbeat() = 0;
    virtual SteamAPICall_t AssociateWithClan(CSteamID steamIDClan) = 0;
    virtual SteamAPICall_t ComputeNewPlayerCompatibility(CSteamID steamIDNewPlayer) = 0;
};

// Every call is a no-op. The return values describe a server that is not
// logged on to Steam, which is exactly what a LAN-only test server is.
class SteamGameServerStub final : public ISteamGameServer {
public:
    bool InitGameServer(uint32, uint16, uint16, uint32, AppId_t, const char *) override {
        return true;
    }
    void SetProduct(const char *) override {}
    void SetGameDescription(const char *) override {}
    void SetModDir(const char *) override {}
    void SetDedicatedServer(bool) override {}
    void LogOn(const char *, const char *) override {}
    void LogOnAnonymous() override {}
    void LogOff() override {}
    bool BLoggedOn() override { return false; }
    bool BSecure() override { return false; }
    CSteamID GetSteamID() override { return CSteamID(); }
    bool WasRestartRequested() override { return false; }
    void SetMaxPlayerCount(int) override {}
    void SetBotPlayerCount(int) override {}
    void SetServerName(const char *) override {}
    void SetMapName(const char *) override {}
    void SetPasswordProtected(bool) override {}
    void SetSpectatorPort(uint16) override {}
    void SetSpectatorServerName(const char *) override {}
    void ClearAllKeyValues() override {}
    void SetKeyValue(const char *, const char *) override {}
    void SetGameTags(const char *) override {}
    void SetGameData(const char *) override {}
    void SetRegion(const char *) override {}
    bool SendUserConnectAndAuthenticate(uint32, const void *, uint32, CSteamID *) override {
        return false;
    }
    CSteamID CreateUnauthenticatedUserConnection() override { return CSteamID(); }
    void SendUserDisconnect(CSteamID) override {}
    bool BUpdateUserData(CSteamID, const char *, uint32) override { return false; }
    HAuthTicket GetAuthSessionTicket(void *, int, uint32 *pcbTicket) override {
        if (pcbTicket != nullptr) {
            *pcbTicket = 0;
        }
        return 0;
    }
    EBeginAuthSessionResult BeginAuthSession(const void *, int, CSteamID) override { return 2; }
    void EndAuthSession(CSteamID) override {}
    void CancelAuthTicket(HAuthTicket) override {}
    EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID, AppId_t) override { return 0; }
    bool RequestUserGroupStatus(CSteamID, CSteamID) override { return false; }
    void GetGameplayStats() override {}
    SteamAPICall_t GetServerReputation() override { return 0; }
    uint32 GetPublicIP() override { return 0; }
    bool HandleIncomingPacket(const void *, int, uint32, uint16) override { return false; }
    int GetNextOutgoingPacket(void *, int, uint32 *, uint16 *) override { return 0; }
    void EnableHeartbeats(bool) override {}
    void SetHeartbeatInterval(int) override {}
    void ForceHeartbeat() override {}
    SteamAPICall_t AssociateWithClan(CSteamID) override { return 0; }
    SteamAPICall_t ComputeNewPlayerCompatibility(CSteamID) override { return 0; }
};

SteamGameServerStub g_gameServer;

} // namespace

extern "C" {

// The engine passes six arguments; the unused ones are ignored.
bool SteamGameServer_Init(uint32, uint16, uint16, uint16, int, const char *) {
    return true;
}

ISteamGameServer *SteamGameServer() {
    return &g_gameServer;
}

// The dedicated server only asks SteamApps for the language and low-violence
// flags, and it guards the call with a null check.
void *SteamApps() {
    return nullptr;
}

void SteamGameServer_RunCallbacks() {}
void SteamGameServer_Shutdown() {}

// ReGameDLL probes the user API on startup. Reporting "not running" keeps it on
// the code path it uses when no Steam client is installed.
bool SteamAPI_Init() {
    return false;
}
bool SteamAPI_IsSteamRunning() {
    return false;
}
void SteamAPI_Shutdown() {}
void SteamAPI_RunCallbacks() {}
void SteamAPI_SetBreakpadAppID(uint32) {}
void SteamAPI_RegisterCallback(void *, int) {}
void SteamAPI_UnregisterCallback(void *, int) {}
void SteamAPI_RegisterCallResult(void *, SteamAPICall_t) {}
void SteamAPI_UnregisterCallResult(void *, SteamAPICall_t) {}
void SteamAPI_UseBreakpadCrashHandler(const char *, const char *, const char *, bool, void *,
                                      void *) {}

} // extern "C"
