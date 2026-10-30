#ifndef CHAOS_EVENTS_H
#define CHAOS_EVENTS_H

#include "chaos_catalog.h"

#define CHAOS_HISTORY_SIZE 8
#define CHAOS_BREADCRUMB_COUNT 8

class ChaosManager {
public:
    ChaosManager();

    void RegisterCvarsAndCommands();
    void OnRegUserMsg(const char *pszName, int iMsgId);
    void OnMapPrecacheReset();
    void OnWorldPrecacheBegin();
    void OnWorldPrecacheEnd();
    void OnWorldEntityCreated(int classname, edict_t *entity);
    void OnMapEntitySpawned(const char *classname, edict_t *entity, int result);
    void OnTrackPrecacheSound(const char *soundName);
    void OnServerActivate(edict_t *pEdictList, int edictCount, int clientMax);
    void OnServerDeactivate();
    void OnClientPutInServer(edict_t *pPlayer);
    void OnClientDisconnect(edict_t *pPlayer);
    bool OnClientCommand(edict_t *pPlayer);

    void OnStartFrame();
    void OnPlayerPreThink(edict_t *pPlayer);
    void OnPlayerPostThink(edict_t *pPlayer);

    // Serialize the player's pre-effect values, while keeping the live effect
    // active after the engine finishes writing the save.
    struct PlayerSaveSnapshot {
        unsigned changed;
        int flags, renderfx;
        float maxspeed, fov, renderamt;
        Vec3 rendercolor, punchangle;
    };
    bool BeforePlayerSave(edict_t *player, PlayerSaveSnapshot &snapshot) const;
    void AfterPlayerSave(edict_t *player, const PlayerSaveSnapshot &snapshot) const;

    // Server console command entry points
    void CmdTriggerRandom();
    void CmdTriggerById(int eventId);
    void CmdResetDeaths();
    void CmdToggle();
    void CmdRecover();
    void CmdPrintList(edict_t *pPlayer, int page = 1);
    void CmdPrintStatus(edict_t *pPlayer);
    void CmdTotem();
    void CmdPortalGun();
    void CmdPortalPlace(int slot);
    void CmdPortalEnter();
    void CmdSpeedrun();
    void CmdC1A1(bool confirm);

    int GetEventCount() const { return CHAOS_EVENT_COUNT; }
    const ChaosEventDef *GetEventDef(int id) const;
    int GetLastEventId() const { return m_lastEventId; }
    float GetTimeRemaining() const;

private:
    edict_t *FindActivePlayer() const;
    bool IsIntroTramMap() const;
    bool PlayerHasSuit(const edict_t *pPlayer) const;
    bool IsClassReady(const char *classname) const;
    int ClassStringId(const char *classname);
    bool IsCueReady(const char *soundName) const;
    bool IsChaosSessionAvailable(const edict_t *player) const;
    bool IsRestrictedState(const edict_t *player) const;
    bool SyncSafetyContext(edict_t *player);
    bool IsEventAllowed(const edict_t *player, int eventId) const;
    bool IsEventAllowedInSession(const edict_t *player, int eventId) const;
    bool HasReadyWeaponForEvent(int eventId) const;
    bool IsMovementSafe(const edict_t *player) const;
    int CountChaosEntities() const;
    bool HasEntityHeadroom() const;

    int PickNextEventId(const edict_t *pPlayer);
    void ExecuteEvent(edict_t *pPlayer, int eventId);
    void ClearAllModifiers(edict_t *pPlayer, bool restoreEngineCvars);
    void SuspendUnsafeEffects(edict_t *pPlayer);
    void UpdateTimedModifiers(edict_t *pPlayer);
    void RecordBreadcrumb(const edict_t *pPlayer);
    void ResetTransientState();
    void ActivateTotemIfNeeded(edict_t *player);
    void ShowNewGameTroll(edict_t *player);
    void StartCsAnimeMeme(edict_t *player);
    void UpdateCsAnimeMeme(edict_t *player);
    void UpdateCheckpoint(edict_t *player);
    void PlacePortal(edict_t *player, int slot);
    void EnterPortal(edict_t *player);
    void UpdatePortalUse(edict_t *player);
    void RunTheme(edict_t *player, int theme);
    void RunVariant(edict_t *player, int variant);
    void ArmBomb(edict_t *player);
    void BlinkForward(edict_t *player);
    void GiveOpforInspiredLoadout(edict_t *player);

    struct CvarOverride { const char *name; float original; float applied; bool active; };
    void BeginOverride(CvarOverride &overrideState, float value);
    void EndOverride(CvarOverride &overrideState);
    void BeginGravity(float value, float seconds, const char *label);
    void ExtendFallProtection(float seconds);
    void UpdateFallProtection(edict_t *player);
    void BeginSpeed(edict_t *player, float value, float acceleration, float seconds, const char *label);
    void BeginIce(float seconds);
    void BeginFov(edict_t *player, int fov, float seconds, const char *label);
    void BeginShield(edict_t *player, float seconds);
    void EndSpeed(edict_t *player);
    void EndFov(edict_t *player);
    void EndShield(edict_t *player);

    // Spawning & entity helpers
    bool FindSafeSpawnSpot(const edict_t *pPlayer, int hullType, float minDist, float maxDist, Vec3 &outPos) const;
    int SpawnMonstersAroundPlayer(edict_t *pPlayer, const char *classname, int count, int hullType, bool friendly);
    void GiveItemToPlayer(edict_t *pPlayer, const char *itemClassname);
    void QueueItemToPlayer(edict_t *pPlayer, const char *itemClassname);
    void UpdateItemQueue(edict_t *pPlayer);
    void ClearItemQueue();
    void StripPlayerWeapons(edict_t *pPlayer);
    int LaunchNearbyEnemies(edict_t *pPlayer, const Vec3 &center, float radius, float upwardVel);

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
    cvar_t m_cvarMonsters;
    cvar_t m_cvarSpawnLimit;
    cvar_t m_cvarIntroTroll;
    bool   m_cvarsRegistered;

    // Captured UserMsg IDs
    int m_msgSetFOV;
    int m_msgScreenShake;
    int m_msgScreenFade;
    int m_msgSayText;
    int m_msgTextMsg;

    // Only use stock classes/sounds already precached by the original game/map.
    bool m_classReady[64];
    int  m_classStringIds[64]; // one engine string allocation per class/map
    bool m_soundReady[16];
    bool m_inWorldPrecache;

    // Server / player runtime state
    edict_t *m_pEdictList;
    int      m_edictCount;
    int      m_clientMax;
    bool     m_serverActive;
    // An FL_CLIENT edict alone is not evidence of an initialized netchan:
    // save restoration creates it before the client has completed signon.
    edict_t *m_networkPlayer;
    int      m_networkPlayerIndex;
    int      m_networkPlayerSerial;
    bool     m_restrictedContext;

    float m_flLastFrameTime;
    float m_flNextChaosTime;
    float m_flNextHudRefresh;
    float m_nextManualEventTime;
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

    // Preserve settings supplied by the game/player when temporary effects end.
    CvarOverride m_gravityOverride, m_speedOverride, m_accelOverride;
    CvarOverride m_frictionOverride, m_airOverride;
    float m_previousPlayerSpeed;
    float m_previousFov;
    bool m_previousGodmode;
    int m_previousRenderFx;
    float m_previousRenderAmt;
    Vec3 m_previousRenderColor;
    Vec3 m_previousPunchAngle;

    bool m_totemArmed;
    bool m_portalEnabled;
    struct PortalAnchor { Vec3 origin; bool valid; } m_portals[2];
    float m_nextPortalUse;
    float m_nextPortalVisual;
    bool m_portalUseHeld;
    bool m_speedrunActive;
    float m_speedrunStart;
    bool m_introSeen;
    bool m_introPending;
    float m_introAt;
    int m_csAnimeFrame; // -1 = no animation
    float m_nextCsAnimeFrame;
    char m_itemQueue[32][32];
    int m_itemQueueHead;
    int m_itemQueueCount;
    float m_nextItemTick;
    bool m_checkpointPending;
    bool m_checkpointPrepared;
    float m_checkpointAt;

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
