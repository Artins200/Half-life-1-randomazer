#ifndef CHAOS_EVENTS_H
#define CHAOS_EVENTS_H

#include "chaos_types.h"

#define CHAOS_EVENT_COUNT 38
#define CHAOS_HISTORY_SIZE 8
#define CHAOS_BREADCRUMB_COUNT 8

class ChaosManager {
public:
    ChaosManager();

    void RegisterCvarsAndCommands();
    void OnRegUserMsg(const char *pszName, int iMsgId);
    void OnMapPrecacheReset();
    void OnTrackPrecacheModel(const char *modelName);
    void OnTrackPrecacheSound(const char *soundName);
    void OnWorldSpawnPost();
    void OnServerActivate(edict_t *pEdictList, int edictCount, int clientMax);
    void OnServerDeactivate();
    void OnClientPutInServer(edict_t *pPlayer);
    bool OnClientCommand(edict_t *pPlayer);

    void OnStartFrame();
    void OnPlayerPreThink(edict_t *pPlayer);
    void OnPlayerPostThink(edict_t *pPlayer);

    // Server console command entry points
    void CmdTriggerRandom();
    void CmdTriggerById(int eventId);
    void CmdResetDeaths();
    void CmdToggle();
    void CmdPrintList(edict_t *pPlayer);
    void CmdPrintStatus(edict_t *pPlayer);

    int GetEventCount() const { return CHAOS_EVENT_COUNT; }
    const ChaosEventDef *GetEventDef(int id) const;
    int GetLastEventId() const { return m_lastEventId; }
    float GetTimeRemaining() const;

private:
    edict_t *FindActivePlayer() const;
    bool IsIntroTramMap() const;
    bool PlayerHasSuit(const edict_t *pPlayer) const;
    bool CanPrecacheMore() const;
    bool PrecacheMonsterViaMaker(const char *monsterClassname);

    int PickNextEventId(const edict_t *pPlayer);
    void ExecuteEvent(edict_t *pPlayer, int eventId);
    void ClearAllModifiers(edict_t *pPlayer, bool restoreEngineCvars);
    void UpdateTimedModifiers(edict_t *pPlayer);
    void RecordBreadcrumb(const edict_t *pPlayer);

    // Spawning & entity helpers
    bool FindSafeSpawnSpot(const edict_t *pPlayer, int hullType, float minDist, float maxDist, Vec3 &outPos) const;
    int SpawnMonstersAroundPlayer(edict_t *pPlayer, const char *classname, int count, int hullType, bool friendly);
    void GiveItemToPlayer(edict_t *pPlayer, const char *itemClassname);
    void StripPlayerWeapons(edict_t *pPlayer);
    int LaunchNearbyEnemies(edict_t *pPlayer, float radius, float upwardVel);

    // Visual, sound & HUD helpers
    void PlayCueSound(edict_t *pPlayer, const char *sample, int pitch = PITCH_NORM);
    void SendHudText(edict_t *pPlayer, const hudtextparms_t &parms, const char *message, bool reliable);
    void SendScreenShake(edict_t *pPlayer, float amplitude, float frequency, float duration);
    void SendScreenFade(edict_t *pPlayer, byte r, byte g, byte b, byte alpha, float fadeTime, float holdTime, int flags);
    void SendDynamicLight(const Vec3 &pos, int radius10, byte r, byte g, byte b, int life10, int decay10);
    void SendTeleportSplash(const Vec3 &pos);
    void SendImplosion(const Vec3 &pos, int radius, int count, int life10);
    void SendSetFOV(edict_t *pPlayer, int fov);
    void SendChatAndConsole(edict_t *pPlayer, const char *msg);

    void RefreshStatusHud(edict_t *pPlayer);
    void ShowEventBanner(edict_t *pPlayer, const ChaosEventDef &ev, int eventNum);
    void ShowDeathBanner(edict_t *pPlayer, int totalDeaths, int mapDeaths);
    void BuildActiveModifierSummary(char *buf, size_t bufSize) const;

private:
    // Cvars
    cvar_t m_cvarEnabled;
    cvar_t m_cvarInterval;
    cvar_t m_cvarSound;
    cvar_t m_cvarRuChat;
    bool   m_cvarsRegistered;

    // Captured UserMsg IDs
    int m_msgSetFOV;
    int m_msgScreenShake;
    int m_msgScreenFade;
    int m_msgSayText;
    int m_msgTextMsg;

    // Precache tracking per map
    int  m_precachedModelCount;
    int  m_precachedSoundCount;
    bool m_bMonstersPrecached;

    // Server / player runtime state
    edict_t *m_pEdictList;
    int      m_edictCount;
    int      m_clientMax;
    bool     m_serverActive;

    float m_flLastFrameTime;
    float m_flNextChaosTime;
    float m_flNextHudRefresh;
    int   m_lastBeepSecond;

    int   m_lastEventId;
    char  m_lastEventName[64];
    int   m_recentEvents[CHAOS_HISTORY_SIZE];
    int   m_recentHead;
    uint32_t m_rngState;

    // Breadcrumb ring buffer for "Deja Vu" rewind
    PlayerBreadcrumb m_breadcrumbs[CHAOS_BREADCRUMB_COUNT];
    int   m_breadcrumbHead;
    float m_flNextBreadcrumbTime;

    // Timed modifier end timestamps (0.0f = inactive)
    float m_flGravityEnd;
    float m_flTargetGravity;
    const char *m_szGravityLabel;

    float m_flSpeedEnd;
    float m_flTargetSpeed;
    const char *m_szSpeedLabel;

    float m_flFrictionEnd;
    float m_flTargetFriction;
    float m_flTargetAirAccelerate;

    float m_flFovEnd;
    int   m_iTargetFov;
    const char *m_szFovLabel;

    float m_flGodmodeEnd;
    float m_flFallProtectEnd;
    float m_flSuperJumpEnd;

    float m_flPogoEnd;
    float m_flNextPogoTick;

    float m_flRegenEnd;
    float m_flNextRegenTick;

    float m_flDrunkEnd;

    float m_flQuakeEnd;
    float m_flNextQuakeTick;

    float m_flDiscoEnd;
    float m_flNextDiscoTick;
    int   m_discoPhase;

    // Delayed anomaly bomb (non-lethal knockback shockwave)
    bool  m_bAnomalyArmed;
    Vec3  m_vecAnomalyOrigin;
    float m_flAnomalyDetonateTime;
    float m_flNextAnomalyBeep;
};

extern ChaosManager g_chaos;

#endif // CHAOS_EVENTS_H
