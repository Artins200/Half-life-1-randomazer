#include "death_counter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

DeathCounter g_deathCounter;

DeathCounter::DeathCounter()
    : m_totalDeaths(0),
      m_sessionDeaths(0),
      m_mapDeaths(0),
      m_totalEvents(0),
      m_initialized(false),
      m_dirty(false),
      m_nextFlushTime(0.0f),
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
        if (m_totalDeaths < INT_MAX) ++m_totalDeaths;
        if (m_sessionDeaths < INT_MAX) ++m_sessionDeaths;
        if (m_mapDeaths < INT_MAX) ++m_mapDeaths;
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
        if (m_totalDeaths < INT_MAX) ++m_totalDeaths;
        if (m_sessionDeaths < INT_MAX) ++m_sessionDeaths;
        if (m_mapDeaths < INT_MAX) ++m_mapDeaths;
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
    if (m_totalEvents < INT_MAX) ++m_totalEvents;
    m_dirty = true;
}
void DeathCounter::FlushIfDue(float now) {
    if (!m_dirty) return;
    if (m_nextFlushTime <= 0.0f || now < m_nextFlushTime - 30.0f) m_nextFlushTime = now + 15.0f;
    else if (now >= m_nextFlushTime) {
        SaveToDisk();
        if (m_dirty) m_nextFlushTime = now + 30.0f; // retry failed writes, not every frame
    }
}
void DeathCounter::FlushNow() { if (m_dirty) SaveToDisk(); }

void DeathCounter::ResetDeaths() {
    m_totalDeaths = 0;
    m_sessionDeaths = 0;
    m_mapDeaths = 0;
    SaveToDisk();
}

static bool ReplaceFileAtomically(const char *temporary, const char *destination) {
#ifdef _WIN32
    return MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, destination) == 0;
#endif
}
void DeathCounter::SaveToDisk() {
    char dir[260];
    ResolveDataDir(dir, sizeof(dir));
    char statsPath[320], statsTmp[340], txtPath[320], txtTmp[340];
    snprintf(statsPath, sizeof(statsPath), "%s/chaos_stats.cfg", dir);
    snprintf(statsTmp, sizeof(statsTmp), "%s.tmp", statsPath);
    snprintf(txtPath, sizeof(txtPath), "%s/deaths.txt", dir);
    snprintf(txtTmp, sizeof(txtTmp), "%s.tmp", txtPath);

    bool statsSaved = false;
    FILE *fp = fopen(statsTmp, "wb");
    if (fp) {
        bool wrote = fprintf(fp, "// Half-Life 1 Randomizer Chaos Persistent Stats\ntotal_deaths=%d\ntotal_events=%d\n",
                             m_totalDeaths, m_totalEvents) >= 0;
        bool closed = fclose(fp) == 0;
        if (wrote && closed) statsSaved = ReplaceFileAtomically(statsTmp, statsPath);
    }
    if (!statsSaved && fp) remove(statsTmp);

    FILE *ft = fopen(txtTmp, "wb");
    if (ft) {
        bool wrote = fprintf(ft, "Deaths: %d\n", m_totalDeaths) >= 0;
        bool closed = fclose(ft) == 0;
        if (!(wrote && closed && ReplaceFileAtomically(txtTmp, txtPath))) remove(txtTmp);
    }
    m_dirty = !statsSaved;
    if (statsSaved) m_nextFlushTime = 0.0f;
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
        const char *value = nullptr;
        bool isDeath = false;
        if (strncmp(line, "total_deaths=", 13) == 0) { value = line + 13; isDeath = true; }
        else if (strncmp(line, "total_events=", 13) == 0) value = line + 13;
        if (!value) continue;
        errno = 0;
        char *end = nullptr;
        long parsed = strtol(value, &end, 10);
        if (errno == ERANGE || end == value || parsed < 0 || parsed > INT_MAX) continue;
        if (isDeath) m_totalDeaths = (int)parsed;
        else m_totalEvents = (int)parsed;
    }
    fclose(fp);
}
