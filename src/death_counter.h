#ifndef DEATH_COUNTER_H
#define DEATH_COUNTER_H

#include "hlsdk_mini.h"

class DeathCounter {
public:
    DeathCounter();

    void Init(const char *overrideDir = nullptr);
    void OnMapStart(const char *mapName);
    void OnClientPutInServer();
    void OnClientKill();

    // Returns true on the exact frame a new player death is detected
    bool CheckPlayerState(const edict_t *pPlayer);

    void IncrementEvents();
    void ResetDeaths();

    int GetTotalDeaths() const   { return m_totalDeaths; }
    int GetSessionDeaths() const { return m_sessionDeaths; }
    int GetMapDeaths() const     { return m_mapDeaths; }
    int GetTotalEvents() const   { return m_totalEvents; }
    const char *GetCurrentMap() const { return m_currentMap; }

    void SaveToDisk() const;
    void LoadFromDisk();

private:
    void ResolveDataDir(char *outBuf, size_t outSize) const;

    int  m_totalDeaths;
    int  m_sessionDeaths;
    int  m_mapDeaths;
    int  m_totalEvents;

    bool m_initialized;
    bool m_playerSpawnedAlive;
    bool m_deathCountedThisLife;

    char m_currentMap[64];
    char m_overrideDir[260];
};

extern DeathCounter g_deathCounter;

#endif // DEATH_COUNTER_H
