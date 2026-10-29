#include "death_counter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

DeathCounter g_deathCounter;

DeathCounter::DeathCounter()
    : m_totalDeaths(0),
      m_sessionDeaths(0),
      m_mapDeaths(0),
      m_totalEvents(0),
      m_initialized(false),
      m_playerSpawnedAlive(false),
      m_deathCountedThisLife(false) {
    m_currentMap[0] = '\0';
    m_overrideDir[0] = '\0';
}

void DeathCounter::ResolveDataDir(char *outBuf, size_t outSize) const {
    if (!outBuf || outSize == 0) {
        return;
    }
    if (m_overrideDir[0] != '\0') {
        snprintf(outBuf, outSize, "%s", m_overrideDir);
        return;
    }
    char gameDir[260];
    gameDir[0] = '\0';
    if (g_engfuncs.pfnGetGameDir) {
        g_engfuncs.pfnGetGameDir(gameDir);
    }
    if (gameDir[0] != '\0') {
        snprintf(outBuf, outSize, "%s", gameDir);
    } else {
        snprintf(outBuf, outSize, "hl_randomizer");
    }
}

void DeathCounter::Init(const char *overrideDir) {
    if (overrideDir && overrideDir[0] != '\0') {
        snprintf(m_overrideDir, sizeof(m_overrideDir), "%s", overrideDir);
    }
    if (!m_initialized) {
        LoadFromDisk();
        m_initialized = true;
    }
}

void DeathCounter::OnMapStart(const char *mapName) {
    if (!m_initialized) {
        LoadFromDisk();
        m_initialized = true;
    }
    if (mapName && mapName[0] != '\0') {
        if (strcmp(m_currentMap, mapName) != 0) {
            snprintf(m_currentMap, sizeof(m_currentMap), "%s", mapName);
            m_mapDeaths = 0;
        }
    }
    m_playerSpawnedAlive = false;
    m_deathCountedThisLife = false;
}

void DeathCounter::OnClientPutInServer() {
    m_playerSpawnedAlive = false;
    m_deathCountedThisLife = false;
}

void DeathCounter::OnClientKill() {
    if (m_playerSpawnedAlive && !m_deathCountedThisLife) {
        m_deathCountedThisLife = true;
        m_totalDeaths++;
        m_sessionDeaths++;
        m_mapDeaths++;
        SaveToDisk();
    }
}

bool DeathCounter::CheckPlayerState(const edict_t *pPlayer) {
    if (!pPlayer || pPlayer->free) {
        return false;
    }

    const bool isAlive = (pPlayer->v.deadflag == DEAD_NO && pPlayer->v.health > 0.0f);

    if (!m_playerSpawnedAlive) {
        if (isAlive) {
            m_playerSpawnedAlive = true;
            m_deathCountedThisLife = false;
        }
        return false;
    }

    if (!m_deathCountedThisLife && !isAlive) {
        m_deathCountedThisLife = true;
        m_totalDeaths++;
        m_sessionDeaths++;
        m_mapDeaths++;
        SaveToDisk();
        return true;
    }

    if (m_deathCountedThisLife && isAlive) {
        // Player respawned without map reload (e.g., listen server / custom map)
        m_deathCountedThisLife = false;
    }

    return false;
}

void DeathCounter::IncrementEvents() {
    m_totalEvents++;
    SaveToDisk();
}

void DeathCounter::ResetDeaths() {
    m_totalDeaths = 0;
    m_sessionDeaths = 0;
    m_mapDeaths = 0;
    SaveToDisk();
}

void DeathCounter::SaveToDisk() const {
    char dir[260];
    ResolveDataDir(dir, sizeof(dir));

    char statsPath[320];
    snprintf(statsPath, sizeof(statsPath), "%s/chaos_stats.cfg", dir);
    FILE *fp = fopen(statsPath, "wb");
    if (fp) {
        fprintf(fp, "// Half-Life 1 Randomizer Chaos Persistent Stats\n");
        fprintf(fp, "total_deaths=%d\n", m_totalDeaths);
        fprintf(fp, "total_events=%d\n", m_totalEvents);
        fclose(fp);
    }

    char txtPath[320];
    snprintf(txtPath, sizeof(txtPath), "%s/deaths.txt", dir);
    FILE *ft = fopen(txtPath, "wb");
    if (ft) {
        fprintf(ft, "Deaths: %d\n", m_totalDeaths);
        fclose(ft);
    }
}

void DeathCounter::LoadFromDisk() {
    char dir[260];
    ResolveDataDir(dir, sizeof(dir));

    char statsPath[320];
    snprintf(statsPath, sizeof(statsPath), "%s/chaos_stats.cfg", dir);
    FILE *fp = fopen(statsPath, "rb");
    if (!fp) {
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        int val = 0;
        if (sscanf(line, "total_deaths=%d", &val) == 1 && val >= 0) {
            m_totalDeaths = val;
        } else if (sscanf(line, "total_events=%d", &val) == 1 && val >= 0) {
            m_totalEvents = val;
        }
    }
    fclose(fp);
}
