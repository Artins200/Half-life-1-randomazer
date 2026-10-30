#include "chaos_events.h"
#include "death_counter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <errno.h>

ChaosManager g_chaos;

// No extra map precaches: the original game and map get the whole GoldSrc
// resource budget. Only these already-observed stock resources may be used.
static const char *const kTrackedClasses[] = {
    "monster_headcrab", "monster_babycrab", "monster_houndeye", "monster_bullchicken",
    "monster_zombie", "monster_alien_slave", "monster_human_grunt", "monster_alien_grunt",
    "monster_snark", "monster_cockroach", "monster_barney", "monster_scientist",
    "item_suit", "item_healthkit", "item_battery", "item_longjump",
    "weapon_crowbar", "weapon_9mmhandgun", "weapon_357", "weapon_shotgun",
    "weapon_9mmAR", "weapon_crossbow", "weapon_rpg", "weapon_gauss", "weapon_egon",
    "weapon_hornetgun", "weapon_handgrenade", "weapon_satchel", "weapon_tripmine", "weapon_snark",
    "ammo_9mmclip", "ammo_357", "ammo_buckshot", "ammo_9mmAR", "ammo_ARgrenades",
    "ammo_crossbow", "ammo_rpgclip", "ammo_gaussclip", "player_weaponstrip"
};
static const char *const kCueSounds[] = {
    "fvox/beep.wav", "debris/beamstart7.wav", "items/suitchargeok1.wav",
    "items/medshot4.wav", "items/gunpickup2.wav", "items/weapondrop1.wav",
    "items/suitchargeno1.wav", "debris/zap4.wav", "buttons/button3.wav", "buttons/button10.wav"
};
static_assert(sizeof(kTrackedClasses) / sizeof(kTrackedClasses[0]) <= 64, "tracked class capacity");
static_assert(sizeof(kCueSounds) / sizeof(kCueSounds[0]) <= 16, "tracked sound capacity");

static int ClassIndex(const char *name) {
    if (!name) return -1;
    for (size_t i = 0; i < sizeof(kTrackedClasses) / sizeof(kTrackedClasses[0]); ++i)
        if (strcmp(name, kTrackedClasses[i]) == 0) return (int)i;
    return -1;
}
static int SoundIndex(const char *name) {
    if (!name) return -1;
    for (size_t i = 0; i < sizeof(kCueSounds) / sizeof(kCueSounds[0]); ++i)
        if (strcmp(name, kCueSounds[i]) == 0) return (int)i;
    return -1;
}
static float ChaosInterval() {
    float value = g_engfuncs.pfnCVarGetFloat ? g_engfuncs.pfnCVarGetFloat("chaos_interval") : 10.0f;
    if (!isfinite(value)) return 10.0f;
    if (value < 2.0f) return 2.0f;
    if (value > 300.0f) return 300.0f;
    return value;
}



static short FixedSigned16(float value, float scale) {
    int out = (int)(value * scale);
    if (out > 32767) out = 32767;
    if (out < -32768) out = -32768;
    return (short)out;
}

static unsigned short FixedUnsigned16(float value, float scale) {
    int out = (int)(value * scale);
    if (out < 0) out = 0;
    if (out > 0xFFFF) out = 0xFFFF;
    return (unsigned short)out;
}

static bool StrCaseEq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

// Reject junk and integer overflow instead of interpreting e.g. "abc" as ID 0.
static bool ParseInteger(const char *text, int &out) {
    if (!text || !*text) return false;
    errno = 0;
    char *end = nullptr;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || !end || *end || value < INT_MIN || value > INT_MAX) return false;
    out = (int)value;
    return true;
}
static int CmdArgInt(int arg, int fallback) {
    int value;
    return g_engfuncs.pfnCmd_Argc && g_engfuncs.pfnCmd_Argv && g_engfuncs.pfnCmd_Argc() > arg &&
           ParseInteger(g_engfuncs.pfnCmd_Argv(arg), value) ? value : fallback;
}
static void SrvCmd_ChaosNow() { g_chaos.CmdTriggerRandom(); }
static void SrvCmd_ChaosResetDeaths() { g_chaos.CmdResetDeaths(); }
static void SrvCmd_ChaosToggle() { g_chaos.CmdToggle(); }
static void SrvCmd_ChaosRecover() { g_chaos.CmdRecover(); }
static void SrvCmd_ChaosList() { g_chaos.CmdPrintList(nullptr, CmdArgInt(1, 1)); }
static void SrvCmd_ChaosStatus() { g_chaos.CmdPrintStatus(nullptr); }
static void SrvCmd_ChaosTrigger() {
    if (g_engfuncs.pfnCmd_Argc && g_engfuncs.pfnCmd_Argc() >= 2) g_chaos.CmdTriggerById(CmdArgInt(1, -1));
    else g_chaos.CmdTriggerRandom();
}
static void SrvCmd_Totem() { g_chaos.CmdTotem(); }
static void SrvCmd_PortalGun() { g_chaos.CmdPortalGun(); }
static void SrvCmd_PortalBlue() { g_chaos.CmdPortalPlace(0); }
static void SrvCmd_PortalOrange() { g_chaos.CmdPortalPlace(1); }
static void SrvCmd_PortalEnter() { g_chaos.CmdPortalEnter(); }
static void SrvCmd_Speedrun() { g_chaos.CmdSpeedrun(); }
static void SrvCmd_C1A1() { g_chaos.CmdC1A1(false); }
static void SrvCmd_C1A1Go() { g_chaos.CmdC1A1(true); }

ChaosManager::ChaosManager()
    : m_cvarsRegistered(false),
      m_msgSetFOV(0),
      m_msgScreenShake(0),
      m_msgScreenFade(0),
      m_msgSayText(0),
      m_msgTextMsg(0),
      m_inWorldPrecache(false),
      m_pEdictList(nullptr),
      m_edictCount(0),
      m_clientMax(1),
      m_serverActive(false),
      m_restrictedContext(false),
      m_flLastFrameTime(0.0f),
      m_flNextChaosTime(10.0f),
      m_flNextHudRefresh(0.0f),
      m_nextManualEventTime(0.0f),
      m_lastBeepSecond(-1),
      m_lastEventId(-1),
      m_recentHead(0),
      m_rngState(0x9E3779B9u),
      m_breadcrumbHead(0),
      m_flNextBreadcrumbTime(0.0f),
      m_previousPlayerSpeed(320.0f),
      m_previousFov(0.0f),
      m_previousGodmode(false),
      m_previousRenderFx(0),
      m_previousRenderAmt(0.0f),
      m_totemArmed(false),
      m_portalEnabled(false),
      m_nextPortalUse(0.0f),
      m_nextPortalVisual(0.0f),
      m_portalUseHeld(false),
      m_speedrunActive(false),
      m_speedrunStart(0.0f),
      m_introSeen(false),
      m_introPending(false),
      m_introAt(0.0f),
      m_csAnimeFrame(-1),
      m_nextCsAnimeFrame(0.0f),
      m_itemQueueHead(0),
      m_itemQueueCount(0),
      m_nextItemTick(0.0f),
      m_checkpointPending(false),
      m_checkpointPrepared(false),
      m_checkpointAt(0.0f),
      m_flGravityEnd(0.0f),
      m_flTargetGravity(800.0f),
      m_szGravityLabel(nullptr),
      m_flSpeedEnd(0.0f),
      m_flTargetSpeed(320.0f),
      m_szSpeedLabel(nullptr),
      m_flFrictionEnd(0.0f),
      m_flTargetFriction(4.0f),
      m_flTargetAirAccelerate(10.0f),
      m_flFovEnd(0.0f),
      m_iTargetFov(0),
      m_szFovLabel(nullptr),
      m_flGodmodeEnd(0.0f),
      m_flFallProtectEnd(0.0f),
      m_flSuperJumpEnd(0.0f),
      m_flPogoEnd(0.0f),
      m_flNextPogoTick(0.0f),
      m_flRegenEnd(0.0f),
      m_flNextRegenTick(0.0f),
      m_flDrunkEnd(0.0f),
      m_flQuakeEnd(0.0f),
      m_flNextQuakeTick(0.0f),
      m_flDiscoEnd(0.0f),
      m_flNextDiscoTick(0.0f),
      m_discoPhase(0),
      m_bAnomalyArmed(false),
      m_flAnomalyDetonateTime(0.0f),
      m_flNextAnomalyBeep(0.0f) {
    m_cvarEnabled  = { "chaos_enabled",  "1",  FCVAR_SERVER | FCVAR_ARCHIVE, 1.0f,  nullptr };
    m_cvarInterval = { "chaos_interval", "10", FCVAR_SERVER | FCVAR_ARCHIVE, 10.0f, nullptr };
    m_cvarSound    = { "chaos_sound",    "1",  FCVAR_SERVER | FCVAR_ARCHIVE, 1.0f,  nullptr };
    m_cvarRuChat   = { "chaos_ru_chat",  "1",  FCVAR_SERVER | FCVAR_ARCHIVE, 1.0f,  nullptr };
    m_cvarMonsters = { "chaos_monsters", "1", FCVAR_SERVER | FCVAR_ARCHIVE, 1.0f, nullptr };
    m_cvarSpawnLimit = { "chaos_spawn_limit", "8", FCVAR_SERVER | FCVAR_ARCHIVE, 8.0f, nullptr };
    m_cvarIntroTroll = { "chaos_intro_troll", "1", FCVAR_SERVER | FCVAR_ARCHIVE, 1.0f, nullptr };
    memset(m_classReady, 0, sizeof(m_classReady));
    memset(m_classStringIds, 0, sizeof(m_classStringIds));
    memset(m_soundReady, 0, sizeof(m_soundReady));
    m_portals[0].valid = m_portals[1].valid = false;
    m_gravityOverride = {"sv_gravity", 0, 0, false};
    m_speedOverride = {"sv_maxspeed", 0, 0, false};
    m_accelOverride = {"sv_accelerate", 0, 0, false};
    m_frictionOverride = {"sv_friction", 0, 0, false};
    m_airOverride = {"sv_airaccelerate", 0, 0, false};

    snprintf(m_lastEventName, sizeof(m_lastEventName), "READY");
    for (int i = 0; i < CHAOS_HISTORY_SIZE; ++i) {
        m_recentEvents[i] = -1;
    }
    for (int i = 0; i < CHAOS_BREADCRUMB_COUNT; ++i) {
        m_breadcrumbs[i].valid = false;
    }
}

const ChaosEventDef *ChaosManager::GetEventDef(int id) const {
    if (id < 0 || id >= CHAOS_EVENT_COUNT) {
        return nullptr;
    }
    return ChaosGetEventDef(id);
}

float ChaosManager::GetTimeRemaining() const {
    if (!gpGlobals) return 10.0f;
    float rem = m_flNextChaosTime - gpGlobals->time;
    return (rem > 0.0f) ? rem : 0.0f;
}

void ChaosManager::RegisterCvarsAndCommands() {
    if (m_cvarsRegistered) {
        return;
    }
    if (g_engfuncs.pfnCVarRegister) {
        g_engfuncs.pfnCVarRegister(&m_cvarEnabled);
        g_engfuncs.pfnCVarRegister(&m_cvarInterval);
        g_engfuncs.pfnCVarRegister(&m_cvarSound);
        g_engfuncs.pfnCVarRegister(&m_cvarRuChat);
        g_engfuncs.pfnCVarRegister(&m_cvarMonsters);
        g_engfuncs.pfnCVarRegister(&m_cvarSpawnLimit);
        g_engfuncs.pfnCVarRegister(&m_cvarIntroTroll);
    }
    if (g_engfuncs.pfnAddServerCommand) {
        g_engfuncs.pfnAddServerCommand((char *)"chaos_now", SrvCmd_ChaosNow);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_trigger", SrvCmd_ChaosTrigger);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_reset_deaths", SrvCmd_ChaosResetDeaths);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_toggle", SrvCmd_ChaosToggle);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_recover", SrvCmd_ChaosRecover);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_list", SrvCmd_ChaosList);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_status", SrvCmd_ChaosStatus);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_totem", SrvCmd_Totem);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_portal_gun", SrvCmd_PortalGun);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_portal_blue", SrvCmd_PortalBlue);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_portal_orange", SrvCmd_PortalOrange);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_portal_enter", SrvCmd_PortalEnter);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_speedrun", SrvCmd_Speedrun);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_c1a1", SrvCmd_C1A1);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_c1a1_go", SrvCmd_C1A1Go);
    }
    g_deathCounter.Init();
    m_cvarsRegistered = true;
    // On a listen server autoexec.cfg normally runs before the game DLL's
    // GameInit. Queue the configuration only after these cvars exist; GoldSrc
    // executes the command buffer after GameInit returns.
    if (g_engfuncs.pfnServerCommand) g_engfuncs.pfnServerCommand((char *)"exec chaos.cfg\n");
}

void ChaosManager::OnRegUserMsg(const char *pszName, int iMsgId) {
    if (!pszName || iMsgId <= 0) return;
    if (strcmp(pszName, "SetFOV") == 0)          m_msgSetFOV = iMsgId;
    else if (strcmp(pszName, "ScreenShake") == 0) m_msgScreenShake = iMsgId;
    else if (strcmp(pszName, "ScreenFade") == 0)  m_msgScreenFade = iMsgId;
    else if (strcmp(pszName, "SayText") == 0)     m_msgSayText = iMsgId;
    else if (strcmp(pszName, "TextMsg") == 0)     m_msgTextMsg = iMsgId;
}

void ChaosManager::OnMapPrecacheReset() {
    if (m_serverActive) ClearAllModifiers(FindActivePlayer(), true);
    memset(m_classReady, 0, sizeof(m_classReady));
    // Engine-allocated string IDs are only valid for the map that allocated them.
    memset(m_classStringIds, 0, sizeof(m_classStringIds));
    memset(m_soundReady, 0, sizeof(m_soundReady));
    m_inWorldPrecache = false;
    m_serverActive = false;
}
void ChaosManager::OnWorldPrecacheBegin() { m_inWorldPrecache = true; }
void ChaosManager::OnWorldPrecacheEnd() { m_inWorldPrecache = false; }

void ChaosManager::OnWorldEntityCreated(int classname, edict_t *entity) {
    if (!m_inWorldPrecache || !entity || entity->free || !entity->pvPrivateData ||
        !gpGlobals || !gpGlobals->pStringBase || classname <= 0) return;
    const char *name = STRING(classname);
    // Valve's W_Precache precaches these stock items/weapons and snarks.
    if (strncmp(name, "weapon_", 7) && strncmp(name, "ammo_", 5) &&
        strcmp(name, "item_suit") && strcmp(name, "item_battery") &&
        strcmp(name, "item_longjump") && strcmp(name, "monster_snark")) return;
    int index = ClassIndex(name);
    if (index >= 0) m_classReady[index] = true;
}
void ChaosManager::OnMapEntitySpawned(const char *classname, edict_t *entity, int result) {
    if (m_serverActive || result < 0 || !entity || entity->free || !entity->pvPrivateData) return;
    int index = ClassIndex(classname);
    if (index >= 0) m_classReady[index] = true;
}
void ChaosManager::OnTrackPrecacheSound(const char *soundName) {
    int index = SoundIndex(soundName);
    if (index >= 0) m_soundReady[index] = true;
}
bool ChaosManager::IsClassReady(const char *classname) const {
    int index = ClassIndex(classname);
    return index >= 0 && m_classReady[index];
}
int ChaosManager::ClassStringId(const char *classname) {
    int index = ClassIndex(classname);
    if (index < 0 || !g_engfuncs.pfnAllocString) return 0;
    // Classic GoldSrc's AllocString can consume permanent engine hunk space on
    // every call. Reuse one ID per known class, discarding it on each map load.
    if (!m_classStringIds[index]) m_classStringIds[index] = g_engfuncs.pfnAllocString(classname);
    return m_classStringIds[index];
}
bool ChaosManager::IsCueReady(const char *soundName) const {
    int index = SoundIndex(soundName);
    return index >= 0 && m_soundReady[index];
}

void ChaosManager::OnServerActivate(edict_t *pEdictList, int edictCount, int clientMax) {
    m_pEdictList = pEdictList;
    m_edictCount = edictCount;
    m_clientMax = (clientMax > 0) ? clientMax : 1;
    m_serverActive = true;

    const char *mapName = (gpGlobals && gpGlobals->mapname) ? STRING(gpGlobals->mapname) : "";
    ResetTransientState();
    if (strcmp(mapName, "c0a0") == 0 && !m_introSeen) {
        m_introSeen = true;
        m_introPending = true;
        m_introAt = (gpGlobals ? gpGlobals->time : 0.0f) + 1.5f;
    } else {
        m_introPending = false;
        if (strncmp(mapName, "c0a0", 4) != 0) m_introSeen = false;
    }
    if (m_checkpointPending) {
        if (strcmp(mapName, "c1a1") == 0) m_checkpointAt = (gpGlobals ? gpGlobals->time : 0.0f) + 3.0f;
        else m_checkpointPending = false;
    }
    g_deathCounter.OnMapStart(mapName);

    // Seed RNG with map name + current stats + engine random
    if (g_engfuncs.pfnRandomLong) {
        m_rngState ^= (uint32_t)g_engfuncs.pfnRandomLong(1, 0x7FFFFFFF);
    }
    m_rngState ^= (uint32_t)(g_deathCounter.GetTotalDeaths() * 2654435761u + g_deathCounter.GetTotalEvents() * 40503u);

    float interval = ChaosInterval();

    float now = gpGlobals ? gpGlobals->time : 0.0f;
    m_flLastFrameTime = now;
    m_flNextChaosTime = now + interval;
    m_flNextHudRefresh = now + 0.5f;
    m_lastBeepSecond = -1;
    m_nextManualEventTime = now;

    for (int i = 0; i < CHAOS_BREADCRUMB_COUNT; ++i) {
        m_breadcrumbs[i].valid = false;
    }
    m_flNextBreadcrumbTime = now + 0.5f;

    ClearAllModifiers(nullptr, true);
}

void ChaosManager::OnServerDeactivate() {
    ClearAllModifiers(FindActivePlayer(), true);
    ResetTransientState();
    g_deathCounter.FlushNow();
    m_serverActive = false;
}

void ChaosManager::OnClientPutInServer(edict_t *pPlayer) {
    g_deathCounter.OnClientPutInServer();
    ClearAllModifiers(pPlayer, true);
    m_restrictedContext = false;

    float interval = ChaosInterval();

    if (gpGlobals) {
        m_flNextChaosTime = gpGlobals->time + interval;
        m_flNextHudRefresh = gpGlobals->time + 0.25f;
    }
    m_lastBeepSecond = -1;
}

edict_t *ChaosManager::FindActivePlayer() const {
    if (!m_serverActive || !g_engfuncs.pfnPEntityOfEntIndex) {
        return nullptr;
    }
    int maxCli = (gpGlobals && gpGlobals->maxClients > 0) ? gpGlobals->maxClients : m_clientMax;
    if (maxCli < 1) maxCli = 1;
    for (int i = 1; i <= maxCli; ++i) {
        edict_t *pEnt = g_engfuncs.pfnPEntityOfEntIndex(i);
        if (pEnt && !pEnt->free && (pEnt->v.flags & FL_CLIENT)) {
            return pEnt;
        }
    }
    return nullptr;
}

bool ChaosManager::IsIntroTramMap() const {
    if (!gpGlobals || !gpGlobals->mapname) return false;
    const char *mapName = STRING(gpGlobals->mapname);
    if (!mapName) return false;
    return (strncmp(mapName, "c0a0", 4) == 0);
}

bool ChaosManager::PlayerHasSuit(const edict_t *pPlayer) const {
    if (!pPlayer) return false;
    return (pPlayer->v.weapons & (int)(1u << WEAPON_SUIT)) != 0;
}

bool ChaosManager::IsMovementSafe(const edict_t *player) const {
    if (!player || player->free || (player->v.flags & (FL_ONTRAIN | FL_FROZEN))) return false;
    // Avoid scripted cameras, vehicles and ladders; don't fight the engine.
    return player->v.movetype == MOVETYPE_WALK;
}

bool ChaosManager::IsChaosSessionAvailable(const edict_t *player) const {
    if (!m_serverActive || !gpGlobals || !gpGlobals->pStringBase ||
        gpGlobals->deathmatch != 0.0f || gpGlobals->coop != 0.0f ||
        !player || player->free || !(player->v.flags & FL_CLIENT) ||
        player->v.deadflag != DEAD_NO || player->v.health <= 0.0f) return false;

    // maxClients is a slot capacity, not a player count. A local game with
    // extra empty slots must not be stuck in safe mode. Still suspend effects
    // if another client joins, or the engine reports an inconsistent limit.
    int slots = gpGlobals->maxClients > 0 ? gpGlobals->maxClients : m_clientMax;
    if (slots < 1 || slots > 64 || (gpGlobals->maxEntities > 0 && slots >= gpGlobals->maxEntities)) return false;
    if (slots > 1) {
        if (!g_engfuncs.pfnPEntityOfEntIndex) return false;
        for (int i = 1; i <= slots; ++i) {
            const edict_t *other = g_engfuncs.pfnPEntityOfEntIndex(i);
            if (other && other != player && !other->free && (other->v.flags & FL_CLIENT)) return false;
        }
    }
    return true;
}

bool ChaosManager::IsRestrictedState(const edict_t *player) const {
    return IsIntroTramMap() || !IsMovementSafe(player);
}

bool ChaosManager::SyncSafetyContext(edict_t *player) {
    if (!IsChaosSessionAvailable(player)) {
        SuspendUnsafeEffects(player);
        m_restrictedContext = false;
        return false;
    }
    const bool restricted = IsRestrictedState(player);
    // Cancel pre-existing movement, inventory and camera effects once upon
    // entering a tram/cutscene. Newly selected visual events may then finish.
    if (restricted && !m_restrictedContext) SuspendUnsafeEffects(player);
    m_restrictedContext = restricted;
    return true;
}

bool ChaosManager::IsEventAllowed(const edict_t *player, int eventId) const {
    return IsChaosSessionAvailable(player) && IsEventAllowedInSession(player, eventId);
}

bool ChaosManager::HasReadyWeaponForEvent(int eventId) const {
    if (const ChaosWeaponGiftDef *gift = ChaosGetWeaponGift(eventId)) return IsClassReady(gift->weapon);
    if (eventId == 42) {
        return IsClassReady("weapon_9mmAR") || IsClassReady("weapon_shotgun") || IsClassReady("weapon_gauss");
    }
    if (eventId != 22 && eventId != 23) return false;
    for (int i = 0; i < CHAOS_WEAPON_GIFT_COUNT; ++i) {
        const ChaosWeaponGiftDef *gift = ChaosGetWeaponGift(CHAOS_WEAPON_GIFT_START + i);
        if (gift && IsClassReady(gift->weapon)) return true;
    }
    return false;
}

bool ChaosManager::IsEventAllowedInSession(const edict_t *player, int eventId) const {
    const ChaosEventDef *ev = ChaosGetEventDef(eventId);
    if (!ev) return false;
    if ((ev->requiresSuit && !PlayerHasSuit(player)) ||
        (ev->skipOnTram && IsRestrictedState(player))) return false;
    if (eventId < 12) {
        static const char *const classes[12] = {
            "monster_headcrab", "monster_babycrab", "monster_houndeye", "monster_bullchicken",
            "monster_zombie", "monster_alien_slave", "monster_human_grunt", "monster_alien_grunt",
            "monster_snark", "monster_cockroach", "monster_barney", "monster_scientist"
        };
        if (!g_engfuncs.pfnCVarGetFloat || !g_engfuncs.pfnAllocString || !g_engfuncs.pfnCreateNamedEntity ||
            !g_engfuncs.pfnRemoveEntity || !g_engfuncs.pfnTraceLine || !g_engfuncs.pfnTraceHull ||
            !g_engfuncs.pfnPointContents || !g_origDllFuncs.pfnSpawn || !IsClassReady(classes[eventId]) ||
            !HasEntityHeadroom()) return false;
        float monstersEnabled = g_engfuncs.pfnCVarGetFloat("chaos_monsters");
        if (!isfinite(monstersEnabled) || monstersEnabled <= 0.0f) return false;
        float limit = g_engfuncs.pfnCVarGetFloat("chaos_spawn_limit");
        if (!isfinite(limit) || limit <= 0 || CountChaosEntities() >= (int)(limit < 16 ? limit : 16)) return false;
    }
    if (eventId == 22 || eventId == 23 || eventId == 42 || ChaosGetWeaponGift(eventId)) {
        if (!HasReadyWeaponForEvent(eventId) || !HasEntityHeadroom() || !g_engfuncs.pfnAllocString ||
            !g_engfuncs.pfnCreateNamedEntity || !g_engfuncs.pfnRemoveEntity ||
            !g_origDllFuncs.pfnSpawn || !g_origDllFuncs.pfnTouch) return false;
    }
    return true;
}

bool ChaosManager::HasEntityHeadroom() const {
    if (!gpGlobals || gpGlobals->maxEntities <= 0 || !g_engfuncs.pfnNumberOfEntities) return false;
    int reserve = gpGlobals->maxEntities / 4;
    if (reserve > 128) reserve = 128;
    if (reserve < 16) reserve = 16;
    return g_engfuncs.pfnNumberOfEntities() < gpGlobals->maxEntities - reserve;
}
int ChaosManager::CountChaosEntities() const {
    if (!gpGlobals || !g_engfuncs.pfnPEntityOfEntIndex) return 16;
    int max = gpGlobals->maxEntities;
    if (max < 0) return 16;
    if (max > 4096) max = 4096;
    int count = 0;
    for (int i = 1; i < max; ++i) {
        edict_t *ent = g_engfuncs.pfnPEntityOfEntIndex(i);
        if (ent && !ent->free && ent->v.iuser4 == 0xC4A05) ++count;
    }
    return count;
}

void ChaosManager::BeginOverride(CvarOverride &state, float value) {
    if (!g_engfuncs.pfnCVarGetFloat || !g_engfuncs.pfnCVarSetFloat) return;
    if (g_engfuncs.pfnCVarGetPointer && !g_engfuncs.pfnCVarGetPointer(state.name)) return;
    if (!state.active) {
        state.original = g_engfuncs.pfnCVarGetFloat(state.name);
        if (!isfinite(state.original)) return;
        state.active = true;
    }
    state.applied = value;
    g_engfuncs.pfnCVarSetFloat(state.name, value);
}
void ChaosManager::EndOverride(CvarOverride &state) {
    if (!state.active) return;
    // Don't overwrite a value changed deliberately by the game/user meanwhile.
    if (g_engfuncs.pfnCVarSetFloat && g_engfuncs.pfnCVarGetFloat &&
        fabsf(g_engfuncs.pfnCVarGetFloat(state.name) - state.applied) < 0.01f)
        g_engfuncs.pfnCVarSetFloat(state.name, state.original);
    state.active = false;
}
void ChaosManager::BeginGravity(float value, float seconds, const char *label) {
    BeginOverride(m_gravityOverride, value);
    m_flGravityEnd = gpGlobals->time + seconds;
    m_flTargetGravity = value;
    m_szGravityLabel = label;
    if (value < 800.0f) ExtendFallProtection(seconds + 5.0f);
}
void ChaosManager::ExtendFallProtection(float seconds) {
    if (!gpGlobals) return;
    const float end = gpGlobals->time + seconds;
    if (end > m_flFallProtectEnd) m_flFallProtectEnd = end;
}
void ChaosManager::UpdateFallProtection(edict_t *player) {
    if (!player || !gpGlobals || m_flFallProtectEnd <= 0.0f) return;
    const float now = gpGlobals->time;
    // If an effect expires in midair, keep its protection until landing (with
    // a 10-second hard cap). Otherwise a high moon-gravity jump can kill the
    // player after the stated protection time has elapsed.
    if (now >= m_flFallProtectEnd &&
        ((player->v.flags & FL_ONGROUND) || now >= m_flFallProtectEnd + 10.0f)) {
        m_flFallProtectEnd = 0.0f;
        return;
    }
    // Valve CBasePlayer records its *private* m_flFallVelocity from velocity.z
    // in PreThink. Changing entvars.flFallVelocity alone does not prevent fall
    // damage. Limit the actual descent before the original PreThink runs.
    if (player->v.velocity.z < -400.0f) player->v.velocity.z = -400.0f;
    if (player->v.flFallVelocity > 400.0f) player->v.flFallVelocity = 400.0f;
}
void ChaosManager::BeginSpeed(edict_t *player, float value, float acceleration, float seconds, const char *label) {
    if (m_flSpeedEnd <= 0.0f) {
        // Zero is a valid engine/player setting; preserve it rather than
        // silently substituting 320 on the next map or scripted sequence.
        m_previousPlayerSpeed = isfinite(player->v.maxspeed) ? player->v.maxspeed : 0.0f;
    }
    BeginOverride(m_speedOverride, value);
    BeginOverride(m_accelOverride, acceleration);
    m_flSpeedEnd = gpGlobals->time + seconds;
    m_flTargetSpeed = value;
    m_szSpeedLabel = label;
    player->v.maxspeed = value;
    if (g_engfuncs.pfnSetClientMaxspeed) g_engfuncs.pfnSetClientMaxspeed(player, value);
}
void ChaosManager::BeginIce(float seconds) {
    BeginOverride(m_frictionOverride, 0.32f);
    BeginOverride(m_airOverride, 80.0f);
    m_flFrictionEnd = gpGlobals->time + seconds;
}
void ChaosManager::BeginFov(edict_t *player, int fov, float seconds, const char *label) {
    if (m_flFovEnd <= 0.0f)
        m_previousFov = (isfinite(player->v.fov) && player->v.fov >= 0.0f && player->v.fov <= 255.0f)
                            ? player->v.fov : 0.0f;
    m_flFovEnd = gpGlobals->time + seconds;
    m_iTargetFov = fov;
    m_szFovLabel = label;
    SendSetFOV(player, fov);
}
static bool IsChaosShieldGlow(const edict_t *player) {
    return player && player->v.renderfx == kRenderFxGlowShell &&
           fabsf(player->v.renderamt - 30.0f) < 0.01f &&
           fabsf(player->v.rendercolor.x - 255.0f) < 0.01f &&
           fabsf(player->v.rendercolor.y - 215.0f) < 0.01f &&
           fabsf(player->v.rendercolor.z - 40.0f) < 0.01f;
}
void ChaosManager::BeginShield(edict_t *player, float seconds) {
    if (m_flGodmodeEnd <= 0.0f) {
        m_previousGodmode = (player->v.flags & FL_GODMODE) != 0;
        m_previousRenderFx = player->v.renderfx;
        m_previousRenderAmt = player->v.renderamt;
        m_previousRenderColor = player->v.rendercolor;
    }
    m_flGodmodeEnd = gpGlobals->time + seconds;
    player->v.flags |= FL_GODMODE;
    player->v.renderfx = kRenderFxGlowShell;
    player->v.rendercolor = Vec3(255.0f, 215.0f, 40.0f);
    player->v.renderamt = 30.0f;
}
void ChaosManager::EndSpeed(edict_t *player) {
    // A train, cutscene or other game logic may have already changed the
    // player's speed. Only undo our own still-applied value.
    if (m_flSpeedEnd > 0.0f && player && !player->free &&
        isfinite(player->v.maxspeed) && fabsf(player->v.maxspeed - m_flTargetSpeed) < 0.01f) {
        player->v.maxspeed = m_previousPlayerSpeed;
        if (g_engfuncs.pfnSetClientMaxspeed) g_engfuncs.pfnSetClientMaxspeed(player, m_previousPlayerSpeed);
    }
    m_flSpeedEnd = 0.0f;
    m_szSpeedLabel = nullptr;
    EndOverride(m_speedOverride);
    EndOverride(m_accelOverride);
}
void ChaosManager::EndFov(edict_t *player) {
    if (m_flFovEnd > 0.0f && player && !player->free && player->v.deadflag == DEAD_NO &&
        isfinite(player->v.fov) && fabsf(player->v.fov - (float)m_iTargetFov) < 0.01f)
        SendSetFOV(player, (int)m_previousFov);
    m_flFovEnd = 0.0f;
    m_iTargetFov = 0;
    m_szFovLabel = nullptr;
}
void ChaosManager::EndShield(edict_t *player) {
    if (m_flGodmodeEnd > 0.0f && player && !player->free) {
        if (!m_previousGodmode) player->v.flags &= ~FL_GODMODE;
        if (IsChaosShieldGlow(player)) {
            player->v.renderfx = m_previousRenderFx;
            player->v.renderamt = m_previousRenderAmt;
            player->v.rendercolor = m_previousRenderColor;
        }
    }
    m_flGodmodeEnd = 0.0f;
}
void ChaosManager::ClearAllModifiers(edict_t *player, bool restoreEngineCvars) {
    (void)restoreEngineCvars;
    EndOverride(m_gravityOverride);
    m_flGravityEnd = 0.0f;
    m_szGravityLabel = nullptr;
    EndSpeed(player);
    EndOverride(m_frictionOverride);
    EndOverride(m_airOverride);
    m_flFrictionEnd = 0.0f;
    EndFov(player);
    EndShield(player);
    if (m_flDrunkEnd > 0.0f && player && !player->free) player->v.punchangle = m_previousPunchAngle;
    m_flFallProtectEnd = 0.0f;
    m_flSuperJumpEnd = 0.0f;
    m_flPogoEnd = 0.0f;
    m_flRegenEnd = 0.0f;
    m_flDrunkEnd = 0.0f;
    m_flQuakeEnd = 0.0f;
    m_flDiscoEnd = 0.0f;
    m_bAnomalyArmed = false;
}

void ChaosManager::SuspendUnsafeEffects(edict_t *player) {
    // A previously started event must not keep pushing the player, detonating
    // a bomb, issuing pickups or changing cvars on a train/ladder/cutscene.
    ClearAllModifiers(player, true);
    ClearItemQueue();
    for (int i = 0; i < CHAOS_BREADCRUMB_COUNT; ++i) m_breadcrumbs[i].valid = false;
    m_csAnimeFrame = -1;
    // Require a fresh press after leaving a vehicle/scripted state.
    m_portalUseHeld = player && (player->v.button & IN_USE) != 0;
}

uint32_t static NextXorShift(uint32_t &state) {
    uint32_t x = state ? state : 0xA341316Cu;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

enum AutomaticEventPool { AUTO_NPC, AUTO_WEAPON, AUTO_OTHER, AUTO_RECIPE, AUTO_POOL_COUNT };

static AutomaticEventPool AutomaticPoolFor(int id) {
    if (id >= 0 && id < 12) return AUTO_NPC;
    if (id == 22 || id == 23 || id == 42 || ChaosGetWeaponGift(id)) return AUTO_WEAPON;
    if ((id >= CHAOS_RECIPE_START && id < CHAOS_RECIPE_END) ||
        (id >= CHAOS_EXTENSION_START && id < CHAOS_EXTENSION_END)) return AUTO_RECIPE;
    return AUTO_OTHER;
}

int ChaosManager::PickNextEventId(const edict_t *pPlayer) {
    if (!IsChaosSessionAvailable(pPlayer)) return -1;
    struct Candidate { int id; AutomaticEventPool pool; bool recent; };
    Candidate candidates[CHAOS_EVENT_COUNT];
    int candidateCount = 0;
    int counts[AUTO_POOL_COUNT] = {};
    int freshCounts[AUTO_POOL_COUNT] = {};
    // Select a category first, then an ID. 432 recipes must not outweigh all
    // standalone NPCs simply because there are more combinations in the list.
    const int weights[AUTO_POOL_COUNT] = {50, 35, 10, 5};
    const bool restricted = IsRestrictedState(pPlayer);
    for (int i = 0; i < CHAOS_EVENT_COUNT; ++i) {
        if (!IsEventAllowedInSession(pPlayer, i)) continue;
        AutomaticEventPool pool = AutomaticPoolFor(i);
        if (pool == AUTO_WEAPON && m_itemQueueCount > 0) continue;
        if (pool == AUTO_RECIPE) {
            // Intro/cutscenes get standalone visual events only. The two AWP
            // combinations that immediately overwrite their own FOV are
            // manual-only; the remaining recipes stay eligible at low weight.
            if (!ChaosRecipeAllowedAutomatically(i, restricted)) continue;
        }
        bool recent = false;
        for (int h = 0; h < CHAOS_HISTORY_SIZE; ++h) {
            if (m_recentEvents[h] == i) { recent = true; break; }
        }
        candidates[candidateCount++] = {i, pool, recent};
        ++counts[pool];
        if (!recent) ++freshCounts[pool];
    }

    int totalWeight = 0;
    for (int p = 0; p < AUTO_POOL_COUNT; ++p) if (counts[p]) totalWeight += weights[p];
    if (!totalWeight) return -1;

    uint32_t r = NextXorShift(m_rngState);
    if (g_engfuncs.pfnRandomLong) r ^= (uint32_t)g_engfuncs.pfnRandomLong(0, 100000);
    int ticket = (int)(r % (uint32_t)totalWeight);
    int selectedPool = 0;
    for (; selectedPool < AUTO_POOL_COUNT; ++selectedPool) {
        if (!counts[selectedPool]) continue;
        if (ticket < weights[selectedPool]) break;
        ticket -= weights[selectedPool];
    }
    if (selectedPool == AUTO_POOL_COUNT) return -1;

    // History is relaxed independently for each category. If, for example,
    // snarks are the only prepared NPC, they must not disappear for eight
    // events merely because other categories have fresh IDs available.
    const bool requireFresh = freshCounts[selectedPool] > 0;
    const int poolCount = requireFresh ? freshCounts[selectedPool] : counts[selectedPool];
    r = NextXorShift(m_rngState);
    if (g_engfuncs.pfnRandomLong) r ^= (uint32_t)g_engfuncs.pfnRandomLong(0, 100000);
    int chosenIndex = (int)(r % (uint32_t)poolCount);
    int chosen = -1;
    for (int i = 0; i < candidateCount; ++i) {
        if ((int)candidates[i].pool != selectedPool || (requireFresh && candidates[i].recent)) continue;
        if (chosenIndex-- == 0) { chosen = candidates[i].id; break; }
    }
    if (chosen < 0) return -1;
    m_recentEvents[m_recentHead] = chosen;
    m_recentHead = (m_recentHead + 1) % CHAOS_HISTORY_SIZE;
    return chosen;
}

bool ChaosManager::FindSafeSpawnSpot(const edict_t *pPlayer, int hullType, float minDist, float maxDist, Vec3 &outPos) const {
    if (!pPlayer || !g_engfuncs.pfnTraceHull || !g_engfuncs.pfnTraceLine || !g_engfuncs.pfnPointContents) {
        return false;
    }

    const Vec3 playerOrigin = pPlayer->v.origin;
    const Vec3 eyePos = playerOrigin + pPlayer->v.view_ofs;
    const float baseYaw = pPlayer->v.v_angle.y;
    const float degToRad = 3.14159265f / 180.0f;

    uint32_t localSeed = m_rngState ^ (uint32_t)(gpGlobals ? (gpGlobals->time * 1000.0f) : 17.0f);

    for (int attempt = 0; attempt < 22; ++attempt) {
        uint32_t rv = NextXorShift(localSeed);
        // Bias first 10 attempts roughly ahead/around player so spawns feel immediate & visible
        float angleOffset = (attempt < 10)
            ? ((float)((int)(rv % 160) - 80))
            : ((float)(rv % 360));
        float angleRad = (baseYaw + angleOffset) * degToRad;

        uint32_t rv2 = NextXorShift(localSeed);
        float t = (float)(rv2 % 1000) / 1000.0f;
        float dist = minDist + (maxDist - minDist) * t;

        Vec3 candidate(
            playerOrigin.x + cosf(angleRad) * dist,
            playerOrigin.y + sinf(angleRad) * dist,
            playerOrigin.z + 24.0f
        );

        int contents = g_engfuncs.pfnPointContents(candidate);
        if (contents != CONTENTS_EMPTY) {
            continue;
        }

        // Line of sight from player eyes so monsters don't spawn in sealed rooms behind walls
        TraceResult los;
        memset((void *)&los, 0, sizeof(los));
        g_engfuncs.pfnTraceLine(eyePos, candidate, 1 /*ignore_monsters*/, (edict_t *)pPlayer, &los);
        if (los.flFraction < 0.82f) {
            continue;
        }

        // Settle slightly downward toward floor while respecting monster hull
        Vec3 downTarget(candidate.x, candidate.y, candidate.z - 96.0f);
        TraceResult floorTr;
        memset((void *)&floorTr, 0, sizeof(floorTr));
        g_engfuncs.pfnTraceHull(candidate, downTarget, 0 /*dont_ignore_monsters*/, hullType, (edict_t *)pPlayer, &floorTr);

        if (floorTr.fAllSolid || floorTr.fStartSolid || floorTr.flFraction >= 1.0f) continue;
        Vec3 hullCenter = floorTr.vecEndPos + Vec3(0, 0, 2.0f);

        TraceResult hullCheck;
        memset((void *)&hullCheck, 0, sizeof(hullCheck));
        g_engfuncs.pfnTraceHull(hullCenter, hullCenter, 0 /*dont_ignore_monsters*/, hullType, (edict_t *)pPlayer, &hullCheck);
        // GoldSrc TraceHull is centered vertically, while the stock monster
        // classes use a bottom-at-origin bounding box. Align the validated
        // hull to the actual NPC, rather than placing its feet at the center.
        const float halfHeight = hullType == large_hull ? 32.0f : (hullType == head_hull ? 18.0f : 36.0f);
        Vec3 monsterOrigin = hullCenter - Vec3(0, 0, halfHeight);
        if (!hullCheck.fAllSolid && !hullCheck.fStartSolid &&
            g_engfuncs.pfnPointContents(monsterOrigin + Vec3(0, 0, 1)) == CONTENTS_EMPTY) {
            outPos = monsterOrigin;
            return true;
        }
    }

    return false;
}

int ChaosManager::SpawnMonstersAroundPlayer(edict_t *pPlayer, const char *classname, int count, int hullType, bool friendly) {
    if (!pPlayer || !classname || !IsClassReady(classname) || !g_engfuncs.pfnAllocString ||
        !g_engfuncs.pfnCreateNamedEntity || !g_engfuncs.pfnRemoveEntity || !g_origDllFuncs.pfnSpawn ||
        !g_engfuncs.pfnCVarGetFloat) return 0;
    float monstersEnabled = g_engfuncs.pfnCVarGetFloat("chaos_monsters");
    if (!isfinite(monstersEnabled) || monstersEnabled <= 0.0f) return 0;
    float configured = g_engfuncs.pfnCVarGetFloat("chaos_spawn_limit");
    if (!isfinite(configured) || configured <= 0) return 0;
    int limit = (int)(configured < 16 ? configured : 16);
    if (count > 3) count = 3; // bounded CPU/network/entity cost per event

    int spawned = 0;
    for (int i = 0; i < count; ++i) {
        if (!HasEntityHeadroom() || CountChaosEntities() >= limit) break;
        m_rngState += (uint32_t)(i * 7919 + 13);
        Vec3 spawnPos;
        if (!FindSafeSpawnSpot(pPlayer, hullType, 96.0f, 260.0f, spawnPos)) {
            continue;
        }

        int classId = ClassStringId(classname);
        if (!classId) break;
        edict_t *pent = g_engfuncs.pfnCreateNamedEntity(classId);
        if (FNullEnt(pent)) continue;
        if (!pent->pvPrivateData) {
            if (g_engfuncs.pfnRemoveEntity) g_engfuncs.pfnRemoveEntity(pent);
            continue;
        }

        Vec3 toPlayer = pPlayer->v.origin - spawnPos;
        float yaw = atan2f(toPlayer.y, toPlayer.x) * (180.0f / 3.14159265f);

        pent->v.origin = spawnPos;
        pent->v.angles = Vec3(0.0f, yaw, 0.0f);
        pent->v.iuser4 = 0xC4A05; // Mark as Chaos-spawned entity
        // Let the original NPC AI fade its own corpse; otherwise a few dead
        // chaos NPCs could permanently exhaust the per-map spawn limit.
        pent->v.spawnflags |= SF_MONSTER_FADECORPSE;

        int spawnResult = g_origDllFuncs.pfnSpawn(pent);
        if (spawnResult < 0 && !pent->free && g_engfuncs.pfnRemoveEntity) g_engfuncs.pfnRemoveEntity(pent);

        if (spawnResult >= 0 && !pent->free) {
            if (strcmp(classname, "monster_snark") == 0) {
                float len = toPlayer.Length2D();
                if (len > 1.0f) {
                    pent->v.velocity = Vec3((toPlayer.x / len) * 180.0f, (toPlayer.y / len) * 180.0f, 140.0f);
                }
            }
            if (!friendly) {
                SendTeleportSplash(spawnPos);
            } else {
                SendDynamicLight(spawnPos, 14, 80, 255, 120, 8, 10);
            }
            spawned++;
        }
    }
    return spawned;
}

void ChaosManager::GiveItemToPlayer(edict_t *pPlayer, const char *itemClassname) {
    if (!pPlayer || !itemClassname || !IsClassReady(itemClassname) || !HasEntityHeadroom() ||
        !g_engfuncs.pfnAllocString || !g_engfuncs.pfnCreateNamedEntity || !g_engfuncs.pfnRemoveEntity ||
        !g_origDllFuncs.pfnSpawn || !g_origDllFuncs.pfnTouch) {
        return;
    }

    int classId = ClassStringId(itemClassname);
    if (!classId) return;
    edict_t *pent = g_engfuncs.pfnCreateNamedEntity(classId);
    if (FNullEnt(pent)) return;
    if (!pent->pvPrivateData) {
        if (g_engfuncs.pfnRemoveEntity) g_engfuncs.pfnRemoveEntity(pent);
        return;
    }

    pent->v.origin = pPlayer->v.origin;
    pent->v.spawnflags |= SF_NORESPAWN;
    int spawnResult = g_origDllFuncs.pfnSpawn(pent);
    if (spawnResult < 0) {
        if (!pent->free && g_engfuncs.pfnRemoveEntity) g_engfuncs.pfnRemoveEntity(pent);
        return;
    }
    if (!pent->free && pent->pvPrivateData) g_origDllFuncs.pfnTouch(pent, pPlayer);

    // Clean up uncollected item (e.g., ammo already at max capacity) so floor stays clean
    if (!pent->free && !(pent->v.flags & FL_KILLME) &&
        pent->v.owner != pPlayer && pent->v.aiment != pPlayer &&
        pent->v.solid != SOLID_NOT && !(pent->v.effects & EF_NODRAW) &&
        g_engfuncs.pfnRemoveEntity) {
        g_engfuncs.pfnRemoveEntity(pent);
    }
}

// Stagger pickups rather than emitting dozens of weapon/ammo messages in a
// single GoldSrc frame. The queue has a hard cap and is discarded on reload.
void ChaosManager::ClearItemQueue() {
    m_itemQueueHead = 0;
    m_itemQueueCount = 0;
    m_nextItemTick = 0.0f;
}
void ChaosManager::QueueItemToPlayer(edict_t *player, const char *classname) {
    if (!player || !classname || !IsClassReady(classname) || m_itemQueueCount >= 32) return;
    int tail = (m_itemQueueHead + m_itemQueueCount) % 32;
    snprintf(m_itemQueue[tail], sizeof(m_itemQueue[tail]), "%s", classname);
    ++m_itemQueueCount;
}
void ChaosManager::UpdateItemQueue(edict_t *player) {
    if (!gpGlobals || !player || !m_itemQueueCount || gpGlobals->time < m_nextItemTick) return;
    // A queue may contain supplies as well as weapons; do not depend on the
    // availability of the full-arsenal event to finish processing it.
    if (!IsChaosSessionAvailable(player) || IsRestrictedState(player) || !PlayerHasSuit(player)) {
        ClearItemQueue();
        return;
    }
    m_nextItemTick = gpGlobals->time + 0.25f;
    for (int i = 0; i < 2 && m_itemQueueCount > 0; ++i) {
        char item[32];
        snprintf(item, sizeof(item), "%s", m_itemQueue[m_itemQueueHead]);
        m_itemQueueHead = (m_itemQueueHead + 1) % 32;
        --m_itemQueueCount;
        GiveItemToPlayer(player, item);
    }
}

void ChaosManager::StripPlayerWeapons(edict_t *pPlayer) {
    if (!pPlayer || !IsClassReady("weapon_crowbar") || !HasEntityHeadroom() || !g_engfuncs.pfnAllocString ||
        !g_engfuncs.pfnCreateNamedEntity || !g_origDllFuncs.pfnSpawn || !g_origDllFuncs.pfnUse) {
        return;
    }
    const bool hadSuit = PlayerHasSuit(pPlayer);

    int classId = ClassStringId("player_weaponstrip");
    if (!classId) return;
    edict_t *strip = g_engfuncs.pfnCreateNamedEntity(classId);
    if (!FNullEnt(strip)) {
        strip->v.origin = pPlayer->v.origin;
        if (strip->pvPrivateData && g_origDllFuncs.pfnSpawn(strip) >= 0 && !strip->free)
            g_origDllFuncs.pfnUse(strip, pPlayer);
        if (!strip->free && g_engfuncs.pfnRemoveEntity) {
            g_engfuncs.pfnRemoveEntity(strip);
        }
    }

    // Preserve HEV suit flag (CStripWeapons::Use passes FALSE, double-ensure)
    if (hadSuit) {
        pPlayer->v.weapons |= (int)(1u << WEAPON_SUIT);
    }
}

int ChaosManager::LaunchNearbyEnemies(edict_t *pPlayer, const Vec3 &center, float radius, float upwardVel) {
    if (!pPlayer || !gpGlobals || gpGlobals->maxEntities <= 0 || !g_engfuncs.pfnFindEntityInSphere ||
        (!g_engfuncs.pfnEntOffsetOfPEntity && !g_engfuncs.pfnIndexOfEdict)) return 0;

    int count = 0;
    edict_t *ent = nullptr;
    int maxScans = gpGlobals->maxEntities;
    if (maxScans > 4096) maxScans = 4096;
    for (int scanned = 0; scanned < maxScans; ++scanned) {
        edict_t *next = g_engfuncs.pfnFindEntityInSphere(ent, center, radius);
        // GoldSrc signals end-of-search with edict 0 (worldspawn), not NULL.
        // Searching again from that sentinel restarts at the beginning forever.
        if (FNullEnt(next) || (g_engfuncs.pfnIndexOfEdict && g_engfuncs.pfnIndexOfEdict(next) == 0) ||
            next == ent) break;
        ent = next;
        if (ent == pPlayer) continue;
        if (!(ent->v.flags & FL_MONSTER) || ent->v.movetype != MOVETYPE_STEP) continue;
        if (ent->v.deadflag != DEAD_NO || ent->v.health <= 0.0f) continue;

        // Protect map-named scripted NPCs (Eli, Kleiner, retinal-scanner Barney/Scientists, bosses)
        const char *tname = ent->v.targetname ? STRING(ent->v.targetname) : "";
        if (tname && tname[0] != '\0') continue;

        const char *cname = ent->v.classname ? STRING(ent->v.classname) : "";
        if (strncmp(cname, "monster_scientist", 17) == 0 ||
            strncmp(cname, "monster_barney", 14) == 0 ||
            strncmp(cname, "monster_gman", 12) == 0 ||
            strncmp(cname, "monster_tentacle", 16) == 0 ||
            strncmp(cname, "monster_nihilanth", 17) == 0) {
            continue;
        }

        ent->v.flags &= ~FL_ONGROUND;
        ent->v.velocity.x += (float)((int)(NextXorShift(m_rngState) % 240) - 120);
        ent->v.velocity.y += (float)((int)(NextXorShift(m_rngState) % 240) - 120);
        ent->v.velocity.z = upwardVel;
        if (++count >= 8) break;
    }
    return count;
}

void ChaosManager::PlayCueSound(edict_t *pPlayer, const char *sample, int pitch) {
    if (!pPlayer || !sample || !IsCueReady(sample) || !g_engfuncs.pfnEmitSound) return;
    if (g_engfuncs.pfnCVarGetFloat && g_engfuncs.pfnCVarGetFloat("chaos_sound") <= 0.0f) {
        return;
    }
    g_engfuncs.pfnEmitSound(pPlayer, CHAN_AUTO, sample, 0.85f, ATTN_NONE, 0, pitch);
}

void ChaosManager::SendHudText(edict_t *pPlayer, const hudtextparms_t &parms, const char *message, bool reliable) {
    if (!pPlayer || !message || !g_engfuncs.pfnMessageBegin) return;

    const int dest = reliable ? MSG_ONE : MSG_ONE_UNRELIABLE;
    g_engfuncs.pfnMessageBegin(dest, SVC_TEMPENTITY, nullptr, pPlayer);
    g_engfuncs.pfnWriteByte(TE_TEXTMESSAGE);
    g_engfuncs.pfnWriteByte(parms.channel & 0xFF);
    g_engfuncs.pfnWriteShort(FixedSigned16(parms.x, (float)(1 << 13)));
    g_engfuncs.pfnWriteShort(FixedSigned16(parms.y, (float)(1 << 13)));
    g_engfuncs.pfnWriteByte(parms.effect);

    g_engfuncs.pfnWriteByte(parms.r1);
    g_engfuncs.pfnWriteByte(parms.g1);
    g_engfuncs.pfnWriteByte(parms.b1);
    g_engfuncs.pfnWriteByte(parms.a1);

    g_engfuncs.pfnWriteByte(parms.r2);
    g_engfuncs.pfnWriteByte(parms.g2);
    g_engfuncs.pfnWriteByte(parms.b2);
    g_engfuncs.pfnWriteByte(parms.a2);

    g_engfuncs.pfnWriteShort(FixedUnsigned16(parms.fadeinTime, (float)(1 << 8)));
    g_engfuncs.pfnWriteShort(FixedUnsigned16(parms.fadeoutTime, (float)(1 << 8)));
    g_engfuncs.pfnWriteShort(FixedUnsigned16(parms.holdTime, (float)(1 << 8)));

    if (parms.effect == 2) {
        g_engfuncs.pfnWriteShort(FixedUnsigned16(parms.fxTime, (float)(1 << 8)));
    }

    char safeBuf[230];
    snprintf(safeBuf, sizeof(safeBuf), "%s", message);
    g_engfuncs.pfnWriteString(safeBuf);
    g_engfuncs.pfnMessageEnd();
}

void ChaosManager::SendScreenShake(edict_t *pPlayer, float amplitude, float frequency, float duration) {
    if (!pPlayer || m_msgScreenShake <= 0 || !g_engfuncs.pfnMessageBegin) return;
    g_engfuncs.pfnMessageBegin(MSG_ONE_UNRELIABLE, m_msgScreenShake, nullptr, pPlayer);
    g_engfuncs.pfnWriteShort(FixedUnsigned16(amplitude, (float)(1 << 12)));
    g_engfuncs.pfnWriteShort(FixedUnsigned16(duration, (float)(1 << 12)));
    g_engfuncs.pfnWriteShort(FixedUnsigned16(frequency, (float)(1 << 8)));
    g_engfuncs.pfnMessageEnd();
}

void ChaosManager::SendScreenFade(edict_t *pPlayer, byte r, byte g, byte b, byte alpha, float fadeTime, float holdTime, int flags) {
    if (!pPlayer || m_msgScreenFade <= 0 || !g_engfuncs.pfnMessageBegin) return;
    g_engfuncs.pfnMessageBegin(MSG_ONE_UNRELIABLE, m_msgScreenFade, nullptr, pPlayer);
    g_engfuncs.pfnWriteShort(FixedUnsigned16(fadeTime, (float)(1 << 12)));
    g_engfuncs.pfnWriteShort(FixedUnsigned16(holdTime, (float)(1 << 12)));
    g_engfuncs.pfnWriteShort(flags);
    g_engfuncs.pfnWriteByte(r);
    g_engfuncs.pfnWriteByte(g);
    g_engfuncs.pfnWriteByte(b);
    g_engfuncs.pfnWriteByte(alpha);
    g_engfuncs.pfnMessageEnd();
}

void ChaosManager::SendDynamicLight(const Vec3 &pos, int radius10, byte r, byte g, byte b, int life10, int decay10) {
    if (!g_engfuncs.pfnMessageBegin) return;
    // Verified GoldSrc TE_DLIGHT format: coord*3 + radius + r + g + b + life + decay (no brightness byte!)
    g_engfuncs.pfnMessageBegin(MSG_BROADCAST, SVC_TEMPENTITY, nullptr, nullptr);
    g_engfuncs.pfnWriteByte(TE_DLIGHT);
    g_engfuncs.pfnWriteCoord(pos.x);
    g_engfuncs.pfnWriteCoord(pos.y);
    g_engfuncs.pfnWriteCoord(pos.z);
    g_engfuncs.pfnWriteByte(radius10 & 0xFF);
    g_engfuncs.pfnWriteByte(r);
    g_engfuncs.pfnWriteByte(g);
    g_engfuncs.pfnWriteByte(b);
    g_engfuncs.pfnWriteByte(life10 & 0xFF);
    g_engfuncs.pfnWriteByte(decay10 & 0xFF);
    g_engfuncs.pfnMessageEnd();
}

void ChaosManager::SendTeleportSplash(const Vec3 &pos) {
    if (!g_engfuncs.pfnMessageBegin) return;
    g_engfuncs.pfnMessageBegin(MSG_PVS, SVC_TEMPENTITY, pos, nullptr);
    g_engfuncs.pfnWriteByte(TE_TELEPORT);
    g_engfuncs.pfnWriteCoord(pos.x);
    g_engfuncs.pfnWriteCoord(pos.y);
    g_engfuncs.pfnWriteCoord(pos.z);
    g_engfuncs.pfnMessageEnd();
}

void ChaosManager::SendImplosion(const Vec3 &pos, int radius, int count, int life10) {
    if (!g_engfuncs.pfnMessageBegin) return;
    g_engfuncs.pfnMessageBegin(MSG_PVS, SVC_TEMPENTITY, pos, nullptr);
    g_engfuncs.pfnWriteByte(TE_IMPLOSION);
    g_engfuncs.pfnWriteCoord(pos.x);
    g_engfuncs.pfnWriteCoord(pos.y);
    g_engfuncs.pfnWriteCoord(pos.z);
    g_engfuncs.pfnWriteByte(radius & 0xFF);
    g_engfuncs.pfnWriteByte(count & 0xFF);
    g_engfuncs.pfnWriteByte(life10 & 0xFF);
    g_engfuncs.pfnMessageEnd();
}

void ChaosManager::SendSetFOV(edict_t *pPlayer, int fov) {
    if (!pPlayer || m_msgSetFOV <= 0 || !g_engfuncs.pfnMessageBegin) return;
    pPlayer->v.fov = (float)fov;
    g_engfuncs.pfnMessageBegin(MSG_ONE, m_msgSetFOV, nullptr, pPlayer);
    g_engfuncs.pfnWriteByte(fov & 0xFF);
    g_engfuncs.pfnMessageEnd();
}

void ChaosManager::SendChatAndConsole(edict_t *pPlayer, const char *msg) {
    if (!msg) return;
    if (pPlayer && g_engfuncs.pfnClientPrintf) {
        g_engfuncs.pfnClientPrintf(pPlayer, print_console, msg);
    } else if (g_engfuncs.pfnServerPrint) {
        g_engfuncs.pfnServerPrint(msg);
    }

    const bool ruChat = (!g_engfuncs.pfnCVarGetFloat || g_engfuncs.pfnCVarGetFloat("chaos_ru_chat") > 0.0f);
    if (ruChat && pPlayer && m_msgSayText > 0 && g_engfuncs.pfnMessageBegin) {
        int entIdx = g_engfuncs.pfnIndexOfEdict ? g_engfuncs.pfnIndexOfEdict(pPlayer) : 1;
        // SayText is a variable-length user message with a 255-byte wire cap.
        // Stop at a UTF-8 boundary and leave ample room for the entity byte.
        char safe[190];
        size_t len = strlen(msg);
        if (len >= sizeof(safe)) {
            len = sizeof(safe) - 1;
            while (len > 0 && (((unsigned char)msg[len] & 0xc0) == 0x80)) --len;
        }
        memcpy(safe, msg, len);
        safe[len] = '\0';
        g_engfuncs.pfnMessageBegin(MSG_ONE, m_msgSayText, nullptr, pPlayer);
        g_engfuncs.pfnWriteByte(entIdx & 0xFF);
        g_engfuncs.pfnWriteString(safe);
        g_engfuncs.pfnMessageEnd();
    }
}

void ChaosManager::BuildActiveModifierSummary(char *buf, size_t bufSize) const {
    if (!buf || bufSize == 0) return;
    buf[0] = '\0';
    if (!gpGlobals) return;

    const float now = gpGlobals->time;
    size_t used = 0;

    auto appendMod = [&](const char *label, float endTime) {
        if (!label || endTime <= now) return;
        int secLeft = (int)ceilf(endTime - now);
        int written = snprintf(buf + used, bufSize - used, "%s%s (%ds)",
                               (used > 0) ? ", " : "ACTIVE: ", label, secLeft);
        if (written > 0 && used + (size_t)written < bufSize) {
            used += (size_t)written;
        }
    };

    appendMod(m_szGravityLabel, m_flGravityEnd);
    appendMod(m_szSpeedLabel, m_flSpeedEnd);
    if (m_flFrictionEnd > now)  appendMod("ICE RINK", m_flFrictionEnd);
    appendMod(m_szFovLabel, m_flFovEnd);
    if (m_flGodmodeEnd > now)   appendMod("GODMODE", m_flGodmodeEnd);
    if (m_flSuperJumpEnd > now) appendMod("SUPER JUMP", m_flSuperJumpEnd);
    if (m_flPogoEnd > now)      appendMod("POGO", m_flPogoEnd);
    if (m_flRegenEnd > now)     appendMod("REGEN", m_flRegenEnd);
    if (m_flDrunkEnd > now)     appendMod("DRUNK", m_flDrunkEnd);
    if (m_flQuakeEnd > now)     appendMod("QUAKE", m_flQuakeEnd);
    if (m_flDiscoEnd > now)     appendMod("DISCO", m_flDiscoEnd);
    if (m_bAnomalyArmed && m_flAnomalyDetonateTime > now) appendMod("BOMB", m_flAnomalyDetonateTime);
    auto appendTag = [&](const char *label) {
        if (used >= bufSize - 1) return;
        int written = snprintf(buf + used, bufSize - used, "%s%s", used ? ", " : "ACTIVE: ", label);
        if (written > 0) used = (size_t)written >= bufSize - used ? bufSize - 1 : used + (size_t)written;
    };
    if (m_totemArmed) appendTag("TOTEM READY");
    if (m_portalEnabled) appendTag("PORTAL DEVICE");
}

void ChaosManager::RefreshStatusHud(edict_t *pPlayer) {
    if (!pPlayer || !gpGlobals) return;

    const bool enabled = (!g_engfuncs.pfnCVarGetFloat || g_engfuncs.pfnCVarGetFloat("chaos_enabled") > 0.0f);
    float interval = ChaosInterval();

    float rem = m_flNextChaosTime - gpGlobals->time;
    if (rem < 0.0f) rem = 0.0f;
    if (rem > interval) rem = interval;

    int secLeft = (int)ceilf(rem);
    int filledBars = (int)roundf(((interval - rem) / interval) * 10.0f);
    if (filledBars < 0) filledBars = 0;
    if (filledBars > 10) filledBars = 10;

    char bar[12];
    for (int i = 0; i < 10; ++i) {
        bar[i] = (i < filledBars) ? '#' : '-';
    }
    bar[10] = '\0';

    char activeLine[128];
    BuildActiveModifierSummary(activeLine, sizeof(activeLine));

    char runLine[40] = {0};
    if (m_speedrunActive) {
        float elapsed = gpGlobals->time - m_speedrunStart;
        if (!isfinite(elapsed) || elapsed < 0) elapsed = 0;
        if (elapsed > 359999) elapsed = 359999;
        int whole = (int)elapsed;
        snprintf(runLine, sizeof(runLine), "\nRUN: %02d:%02d.%d", whole / 60, whole % 60, (int)(elapsed * 10) % 10);
    }
    char hudBuf[384];
    if (enabled && !IsChaosSessionAvailable(pPlayer)) {
        snprintf(hudBuf, sizeof(hudBuf),
                 "=== HL1 CHAOS v1.2 ===\nDEATHS: %d  (MAP: %d)\nWAITING FOR SINGLE PLAYER%s",
                 g_deathCounter.GetTotalDeaths(), g_deathCounter.GetMapDeaths(), runLine);
    } else if (enabled) {
        snprintf(hudBuf, sizeof(hudBuf),
                 "=== HL1 CHAOS v1.2 ===\n"
                 "DEATHS: %d  (MAP: %d)\n"
                 "NEXT: %2ds [%s]\n"
                 "LAST: %s%s%s%s%s",
                 g_deathCounter.GetTotalDeaths(),
                 g_deathCounter.GetMapDeaths(),
                 secLeft,
                 bar,
                 m_lastEventName,
                 IsRestrictedState(pPlayer) ? "\nINTRO/VEHICLE: VISUAL EVENTS" : "",
                 (activeLine[0] != '\0') ? "\n" : "",
                 activeLine,
                 runLine);
    } else {
        snprintf(hudBuf, sizeof(hudBuf),
                 "=== HL1 CHAOS v1.2 [PAUSED] ===\n"
                 "DEATHS: %d  (MAP: %d)\n"
                 "Type chaos_toggle to resume%s",
                 g_deathCounter.GetTotalDeaths(),
                 g_deathCounter.GetMapDeaths(),
                 runLine);
    }

    hudtextparms_t parms;
    memset(&parms, 0, sizeof(parms));
    parms.channel = 1;
    parms.x = 0.015f;
    parms.y = 0.03f;
    parms.effect = 0;
    if (enabled && secLeft <= 3 && secLeft > 0) {
        parms.r1 = 255; parms.g1 = 75;  parms.b1 = 35;  parms.a1 = 255;
    } else {
        parms.r1 = 255; parms.g1 = 180; parms.b1 = 25;  parms.a1 = 255;
    }
    parms.r2 = 255; parms.g2 = 255; parms.b2 = 255; parms.a2 = 255;
    parms.fadeinTime = 0.0f;
    parms.fadeoutTime = 0.0f;
    parms.holdTime = 1.1f;
    parms.fxTime = 0.0f;

    SendHudText(pPlayer, parms, hudBuf, false /*unreliable*/);
}

void ChaosManager::ShowEventBanner(edict_t *pPlayer, const ChaosEventDef &ev, int eventNum) {
    hudtextparms_t parms;
    memset(&parms, 0, sizeof(parms));
    parms.channel = 2;
    parms.x = -1.0f; // Centered horizontally
    parms.y = 0.14f;
    parms.effect = 0;

    switch (ev.tone) {
        case CHAOS_TONE_GOOD:
            parms.r1 = 75;  parms.g1 = 255; parms.b1 = 110; break;
        case CHAOS_TONE_NEUTRAL:
            parms.r1 = 80;  parms.g1 = 225; parms.b1 = 255; break;
        case CHAOS_TONE_WEIRD:
            parms.r1 = 235; parms.g1 = 115; parms.b1 = 255; break;
        case CHAOS_TONE_DANGER:
        default:
            parms.r1 = 255; parms.g1 = 90;  parms.b1 = 45;  break;
    }
    parms.a1 = 255;
    parms.r2 = 255; parms.g2 = 255; parms.b2 = 255; parms.a2 = 255;
    parms.fadeinTime = 0.08f;
    parms.holdTime = 3.8f;
    parms.fadeoutTime = 0.5f;

    char banner[200];
    snprintf(banner, sizeof(banner),
             ">>> RANDOM EVENT #%d <<<\n[ %s ]",
             eventNum, ev.titleEn);
    SendHudText(pPlayer, parms, banner, true /*reliable*/);
}

void ChaosManager::ShowDeathBanner(edict_t *pPlayer, int totalDeaths, int mapDeaths) {
    hudtextparms_t parms;
    memset(&parms, 0, sizeof(parms));
    parms.channel = 3;
    parms.x = -1.0f;
    parms.y = 0.34f;
    parms.effect = 0;
    parms.r1 = 255; parms.g1 = 40; parms.b1 = 40; parms.a1 = 255;
    parms.r2 = 255; parms.g2 = 200; parms.b2 = 200; parms.a2 = 255;
    parms.fadeinTime = 0.05f;
    parms.holdTime = 6.0f;
    parms.fadeoutTime = 0.5f;

    char banner[160];
    snprintf(banner, sizeof(banner),
             "*** YOU DIED! ***\nTOTAL DEATHS: %d  |  MAP DEATHS: %d",
             totalDeaths, mapDeaths);
    SendHudText(pPlayer, parms, banner, true /*reliable*/);

    char chatMsg[220];
    snprintf(chatMsg, sizeof(chatMsg),
             "[ХАОС] Вы погибли! Всего смертей: %d (на этой карте: %d)\n",
             totalDeaths, mapDeaths);
    SendChatAndConsole(pPlayer, chatMsg);
}

void ChaosManager::ExecuteEvent(edict_t *pPlayer, int eventId) {
    if (!pPlayer || pPlayer->free || eventId < 0 || eventId >= CHAOS_EVENT_COUNT || !gpGlobals) {
        return;
    }

    if (!IsEventAllowed(pPlayer, eventId)) return;
    const ChaosEventDef &ev = *ChaosGetEventDef(eventId);
    const float now = gpGlobals->time;

    m_lastEventId = eventId;
    snprintf(m_lastEventName, sizeof(m_lastEventName), "%s", ev.titleEn);
    g_deathCounter.IncrementEvents();
    const int eventNum = g_deathCounter.GetTotalEvents();

    ShowEventBanner(pPlayer, ev, eventNum);

    char chatBuf[256];
    snprintf(chatBuf, sizeof(chatBuf),
             "[ХАОС #%d] %s — %s (Смертей: %d)\n",
             eventNum, ev.titleRu, ev.descRu, g_deathCounter.GetTotalDeaths());
    SendChatAndConsole(pPlayer, chatBuf);

    if (const ChaosWeaponGiftDef *gift = ChaosGetWeaponGift(eventId)) {
        QueueItemToPlayer(pPlayer, gift->weapon);
        if (gift->ammo) QueueItemToPlayer(pPlayer, gift->ammo);
        if (gift->bonusAmmo) QueueItemToPlayer(pPlayer, gift->bonusAmmo);
        PlayCueSound(pPlayer, "items/gunpickup2.wav", PITCH_NORM);
        RefreshStatusHud(pPlayer);
        return;
    }
    if ((eventId >= CHAOS_RECIPE_START && eventId < CHAOS_RECIPE_END) ||
        (eventId >= CHAOS_EXTENSION_START && eventId < CHAOS_EXTENSION_END)) {
        RunTheme(pPlayer, ChaosRecipeTheme(eventId));
        RunVariant(pPlayer, ChaosRecipeVariant(eventId));
        RefreshStatusHud(pPlayer);
        return;
    }
    int spawnedNpcs = -1;
    switch (eventId) {
        case 0: // HEADCRAB AMBUSH
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_headcrab", 3, head_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_NORM);
            break;

        case 1: // BABY CRAB SWARM
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_babycrab", 5, head_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_HIGH);
            break;

        case 2: // HOUNDEYE PACK
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_houndeye", 2, human_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_NORM);
            break;

        case 3: // BULLSQUID SURPRISE
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_bullchicken", 1, large_hull, false);
            if (!spawnedNpcs) spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_headcrab", 2, head_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_LOW);
            break;

        case 4: // ZOMBIE OUTBREAK
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_zombie", 2, human_hull, false);
            if (!spawnedNpcs) spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_headcrab", 3, head_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_LOW);
            break;

        case 5: // VORTIGAUNT RAID
            SendScreenFade(pPlayer, 40, 255, 60, 75, 0.35f, 0.1f, FFADE_IN);
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_alien_slave", 1, human_hull, false);
            if (!spawnedNpcs) spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_houndeye", 1, human_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_NORM);
            break;

        case 6: // HECU MARINE DROP
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_human_grunt", 1, human_hull, false);
            if (!spawnedNpcs) spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_snark", 3, head_hull, false);
            PlayCueSound(pPlayer, "buttons/button10.wav", PITCH_NORM);
            break;

        case 7: // ALIEN GRUNT HORNETS
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_alien_grunt", 1, large_hull, false);
            if (!spawnedNpcs) spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_alien_slave", 1, human_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_LOW);
            break;

        case 8: // SNARK PARTY
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_snark", 4, head_hull, false);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;

        case 9: // ROACH INFESTATION
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_cockroach", 10, head_hull, true);
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
            break;

        case 10: // BARNEY BACKUP
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_barney", 1, human_hull, true);
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_NORM);
            break;

        case 11: // PANICKED SCIENTISTS
            spawnedNpcs = SpawnMonstersAroundPlayer(pPlayer, "monster_scientist", 3, human_hull, true);
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_NORM);
            break;

        case 12: // MOON GRAVITY
            BeginGravity(160.0f, ev.durationSec, "MOON GRAV");
            pPlayer->v.velocity.z += 180.0f;
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
            break;

        case 13: // JUPITER GRAVITY
            BeginGravity(1600.0f, ev.durationSec, "HEAVY GRAV");
            SendScreenShake(pPlayer, 6.0f, 40.0f, 1.0f);
            PlayCueSound(pPlayer, "items/suitchargeno1.wav", PITCH_LOW);
            break;

        case 14: // TURBO FREEMAN
            BeginSpeed(pPlayer, 720.0f, 22.0f, ev.durationSec, "TURBO SPEED");
            SendScreenFade(pPlayer, 60, 220, 255, 50, 0.3f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_HIGH);
            break;

        case 15: // SNAIL PACE
            BeginSpeed(pPlayer, 105.0f, 10.0f, ev.durationSec, "SNAIL PACE");
            PlayCueSound(pPlayer, "items/suitchargeno1.wav", PITCH_LOW);
            break;

        case 16: // ICE SKATING RINK
            BeginIce(ev.durationSec);
            SendScreenFade(pPlayer, 140, 220, 255, 55, 0.4f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
            break;

        case 17: // SUPER JUMP BOOTS
            m_flSuperJumpEnd = now + ev.durationSec;
            ExtendFallProtection(ev.durationSec + 4.5f);
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_NORM);
            break;

        case 18: // POGO STICK MODE
            m_flPogoEnd = now + ev.durationSec;
            m_flNextPogoTick = now + 0.1f;
            ExtendFallProtection(ev.durationSec + 4.0f);
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
            break;

        case 19: // SKY LAUNCHPAD
        {
            ExtendFallProtection(6.5f);
            pPlayer->v.flags &= ~FL_ONGROUND;
            pPlayer->v.velocity.z = 580.0f;
            SendScreenShake(pPlayer, 5.0f, 30.0f, 0.8f);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;
        }

        case 20: // DEJA VU (REWIND 3.5S)
        {
            int bestIdx = -1;
            float bestDiff = 999.0f;
            for (int i = 0; i < CHAOS_BREADCRUMB_COUNT; ++i) {
                if (!m_breadcrumbs[i].valid) continue;
                float age = now - m_breadcrumbs[i].timestamp;
                if (age >= 1.5f && age <= 7.0f) {
                    float diff = fabsf(age - 3.5f);
                    if (diff < bestDiff) {
                        bestDiff = diff;
                        bestIdx = i;
                    }
                }
            }
            bool rewound = false;
            if (bestIdx >= 0 && g_engfuncs.pfnTraceHull && g_engfuncs.pfnSetOrigin && g_engfuncs.pfnPointContents) {
                Vec3 dest = m_breadcrumbs[bestIdx].origin;
                int hull = (pPlayer->v.flags & FL_DUCKING) ? head_hull : human_hull;
                TraceResult path = {}, check = {}, floor = {};
                g_engfuncs.pfnTraceHull(pPlayer->v.origin, dest, 0, hull, pPlayer, &path);
                g_engfuncs.pfnTraceHull(dest, dest, 0, hull, pPlayer, &check);
                Vec3 below = dest - Vec3(0, 0, 72);
                g_engfuncs.pfnTraceHull(dest, below, 0, hull, pPlayer, &floor);
                Vec3 nearFeet = dest - Vec3(0, 0, hull == head_hull ? 14.0f : 30.0f);
                // Don't rewind through a door that has closed since the
                // breadcrumb, into a blocked hull, liquid or an open drop.
                if (!path.fAllSolid && !path.fStartSolid && path.flFraction >= 0.99f &&
                    !check.fAllSolid && !check.fStartSolid && !floor.fAllSolid && !floor.fStartSolid &&
                    floor.flFraction < 1.0f && g_engfuncs.pfnPointContents(dest) == CONTENTS_EMPTY &&
                    g_engfuncs.pfnPointContents(nearFeet) == CONTENTS_EMPTY) {
                    SendTeleportSplash(pPlayer->v.origin);
                    g_engfuncs.pfnSetOrigin(pPlayer, dest);
                    pPlayer->v.velocity = Vec3(0.0f, 0.0f, 0.0f);
                    SendTeleportSplash(dest);
                    SendScreenFade(pPlayer, 180, 80, 255, 95, 0.4f, 0.1f, FFADE_IN);
                    rewound = true;
                }
            }
            if (!rewound) {
                // Safe fallback nudge if no valid 3.5s breadcrumb yet
                pPlayer->v.velocity.z += 260.0f;
                SendScreenFade(pPlayer, 180, 80, 255, 80, 0.4f, 0.1f, FFADE_IN);
            }
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_HIGH);
            break;
        }

        case 21: // BLINK DASH FORWARD
            BlinkForward(pPlayer);
            SendScreenFade(pPlayer, 80, 220, 255, 80, 0.3f, 0.05f, FFADE_IN);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;

        case 22: // RANDOM WEAPON GIFT
        {
            const ChaosWeaponGiftDef *ready[CHAOS_WEAPON_GIFT_COUNT];
            int count = 0;
            for (int i = 0; i < CHAOS_WEAPON_GIFT_COUNT; ++i) {
                const ChaosWeaponGiftDef *gift = ChaosGetWeaponGift(CHAOS_WEAPON_GIFT_START + i);
                if (gift && IsClassReady(gift->weapon)) ready[count++] = gift;
            }
            if (count > 0) {
                const ChaosWeaponGiftDef &gift = *ready[NextXorShift(m_rngState) % (uint32_t)count];
                QueueItemToPlayer(pPlayer, gift.weapon);
                if (gift.ammo) QueueItemToPlayer(pPlayer, gift.ammo);
                if (gift.bonusAmmo) QueueItemToPlayer(pPlayer, gift.bonusAmmo);
                PlayCueSound(pPlayer, "items/gunpickup2.wav", PITCH_NORM);
            }
            break;
        }

        case 23: // FULL ARSENAL JACKPOT
        {
            static const char *const kAllGear[] = {
                "weapon_crowbar", "weapon_9mmhandgun", "ammo_9mmclip",
                "weapon_357", "ammo_357",
                "weapon_shotgun", "ammo_buckshot",
                "weapon_9mmAR", "ammo_9mmAR", "ammo_ARgrenades",
                "weapon_crossbow", "ammo_crossbow",
                "weapon_rpg", "ammo_rpgclip",
                "weapon_gauss", "weapon_egon", "ammo_gaussclip",
                "weapon_hornetgun", "weapon_handgrenade",
                "weapon_satchel", "weapon_tripmine", "weapon_snark",
                "item_longjump"
            };
            for (size_t i = 0; i < sizeof(kAllGear) / sizeof(kAllGear[0]); ++i) {
                QueueItemToPlayer(pPlayer, kAllGear[i]);
            }
            SendScreenFade(pPlayer, 80, 255, 100, 65, 0.4f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "items/gunpickup2.wav", PITCH_HIGH);
            break;
        }

        case 24: // CROWBAR ONLY
            ClearItemQueue();
            StripPlayerWeapons(pPlayer);
            QueueItemToPlayer(pPlayer, "weapon_crowbar");
            PlayCueSound(pPlayer, "items/weapondrop1.wav", PITCH_NORM);
            break;

        case 25: // DEMOMAN LOADOUT
            ClearItemQueue();
            if (!IsClassReady("weapon_crowbar") || !IsClassReady("weapon_rpg")) break;
            StripPlayerWeapons(pPlayer);
            QueueItemToPlayer(pPlayer, "weapon_crowbar");
            QueueItemToPlayer(pPlayer, "weapon_rpg");
            QueueItemToPlayer(pPlayer, "ammo_rpgclip");
            QueueItemToPlayer(pPlayer, "ammo_rpgclip");
            QueueItemToPlayer(pPlayer, "weapon_handgrenade");
            QueueItemToPlayer(pPlayer, "weapon_handgrenade");
            QueueItemToPlayer(pPlayer, "weapon_satchel");
            QueueItemToPlayer(pPlayer, "weapon_tripmine");
            QueueItemToPlayer(pPlayer, "weapon_snark");
            PlayCueSound(pPlayer, "items/gunpickup2.wav", PITCH_LOW);
            break;

        case 26: // SUPPLY DROP SHOWER
            if (!IsClassReady("item_healthkit") && pPlayer->v.health < 100.0f) {
                pPlayer->v.health += 25.0f;
                if (pPlayer->v.health > 100.0f) pPlayer->v.health = 100.0f;
            }
            QueueItemToPlayer(pPlayer, "item_healthkit");
            QueueItemToPlayer(pPlayer, "item_healthkit");
            QueueItemToPlayer(pPlayer, "item_battery");
            QueueItemToPlayer(pPlayer, "item_battery");
            QueueItemToPlayer(pPlayer, "ammo_9mmAR");
            QueueItemToPlayer(pPlayer, "ammo_buckshot");
            QueueItemToPlayer(pPlayer, "ammo_357");
            QueueItemToPlayer(pPlayer, "ammo_gaussclip");
            PlayCueSound(pPlayer, "items/medshot4.wav", PITCH_NORM);
            break;

        case 27: // ANOMALY BOMB
            ArmBomb(pPlayer);
            break;

        case 28: // HEV OVERCHARGE
            if (pPlayer->v.health < 100.0f) pPlayer->v.health = 100.0f;
            pPlayer->v.armorvalue = 150.0f;
            SendScreenFade(pPlayer, 255, 190, 40, 65, 0.4f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_NORM);
            break;

        case 29: // GLASS CANNON (15 HP / 100 AP) - never lethal!
            if (pPlayer->v.health > 15.0f) {
                pPlayer->v.health = 15.0f;
            }
            if (pPlayer->v.armorvalue < 100.0f) {
                pPlayer->v.armorvalue = 100.0f;
            }
            SendScreenFade(pPlayer, 255, 60, 40, 75, 0.35f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "items/suitchargeno1.wav", PITCH_NORM);
            break;

        case 30: // NANO REGENERATION
            m_flRegenEnd = now + ev.durationSec;
            m_flNextRegenTick = now + 0.2f;
            PlayCueSound(pPlayer, "items/medshot4.wav", PITCH_HIGH);
            break;

        case 31: // HP <-> ARMOR SWAP (guaranteed >= 15 HP)
        {
            float oldHp = pPlayer->v.health;
            float oldAp = pPlayer->v.armorvalue;
            float newHp = (oldAp >= 15.0f) ? oldAp : 15.0f;
            float newAp = (oldHp > 0.0f) ? oldHp : 0.0f;
            if (newHp > 150.0f) newHp = 150.0f;
            if (newAp > 150.0f) newAp = 150.0f;
            pPlayer->v.health = newHp;
            pPlayer->v.armorvalue = newAp;
            SendScreenFade(pPlayer, 200, 90, 255, 65, 0.35f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_NORM);
            break;
        }

        case 32: // GODMODE SHIELD (8S)
            BeginShield(pPlayer, ev.durationSec);
            SendScreenFade(pPlayer, 255, 220, 60, 70, 0.4f, 0.15f, FFADE_IN);
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_HIGH);
            break;

        case 33: // QUAKE PRO FOV (130)
            BeginFov(pPlayer, 130, ev.durationSec, "FOV 130");
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;

        case 34: // SNIPER ZOOM (FOV 38)
            BeginFov(pPlayer, 38, ev.durationSec, "ZOOM FOV");
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_LOW);
            break;

        case 35: // BARNEY'S BEER (DRUNK CAM)
            if (m_flDrunkEnd <= 0.0f) m_previousPunchAngle = pPlayer->v.punchangle;
            m_flDrunkEnd = now + ev.durationSec;
            SendScreenFade(pPlayer, 255, 180, 60, 55, 0.6f, 0.2f, FFADE_IN);
            PlayCueSound(pPlayer, "items/medshot4.wav", PITCH_LOW);
            break;

        case 36: // RESONANCE EARTHQUAKE
            m_flQuakeEnd = now + ev.durationSec;
            m_flNextQuakeTick = now + 0.05f;
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_LOW);
            break;

        case 37: // BLACK MESA DISCO RAVE + ANTIGRAV ENEMIES
            m_flDiscoEnd = now + ev.durationSec;
            m_flNextDiscoTick = now + 0.05f;
            m_discoPhase = 0;
            LaunchNearbyEnemies(pPlayer, pPlayer->v.origin, 900.0f, 460.0f);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;

        case 38: m_totemArmed = true; ActivateTotemIfNeeded(pPlayer); break;
        case 39:
            m_portalEnabled = true;
            SendChatAndConsole(pPlayer, "[ХАОС] Порталы: chaos_portal_blue / chaos_portal_orange, переход: E.\n");
            break;
        case 40: RunTheme(pPlayer, 17); break;
        case 41: ShowNewGameTroll(pPlayer); break;
        case 42: GiveOpforInspiredLoadout(pPlayer); break;
        case 43: ArmBomb(pPlayer); break;
        case CHAOS_RECIPE_END: StartCsAnimeMeme(pPlayer); break;
        default: break;
    }

    if (spawnedNpcs == 0) {
        SendChatAndConsole(pPlayer, "[ХАОС] НПС не создан: нет безопасного места, слота или необходимых ресурсов.\n");
    }
    RefreshStatusHud(pPlayer);
}

void ChaosManager::RecordBreadcrumb(const edict_t *pPlayer) {
    if (!pPlayer || !gpGlobals) return;
    if (gpGlobals->time < m_flNextBreadcrumbTime) return;

    m_flNextBreadcrumbTime = gpGlobals->time + 0.7f;
    if (pPlayer->v.deadflag != DEAD_NO || pPlayer->v.health <= 0.0f) return;

    m_breadcrumbs[m_breadcrumbHead].origin = pPlayer->v.origin;
    m_breadcrumbs[m_breadcrumbHead].angles = pPlayer->v.v_angle;
    m_breadcrumbs[m_breadcrumbHead].timestamp = gpGlobals->time;
    m_breadcrumbs[m_breadcrumbHead].valid = true;
    m_breadcrumbHead = (m_breadcrumbHead + 1) % CHAOS_BREADCRUMB_COUNT;
}

void ChaosManager::UpdateTimedModifiers(edict_t *pPlayer) {
    if (!pPlayer || pPlayer->free || !gpGlobals) return;
    const float now = gpGlobals->time;

    // Restore the values that existed before each effect, not hard-coded defaults.
    if (m_flGravityEnd > 0.0f && now >= m_flGravityEnd) {
        EndOverride(m_gravityOverride);
        m_flGravityEnd = 0.0f;
        m_szGravityLabel = nullptr;
    }
    if (m_flSpeedEnd > 0.0f) {
        if (now >= m_flSpeedEnd) EndSpeed(pPlayer);
        else {
            pPlayer->v.maxspeed = m_flTargetSpeed;
            if (g_engfuncs.pfnSetClientMaxspeed) g_engfuncs.pfnSetClientMaxspeed(pPlayer, m_flTargetSpeed);
        }
    }
    if (m_flFrictionEnd > 0.0f && now >= m_flFrictionEnd) {
        EndOverride(m_frictionOverride);
        EndOverride(m_airOverride);
        m_flFrictionEnd = 0.0f;
    }
    if (m_flFovEnd > 0.0f) {
        if (now >= m_flFovEnd) EndFov(pPlayer);
        else if ((int)pPlayer->v.fov != m_iTargetFov) SendSetFOV(pPlayer, m_iTargetFov);
    }
    if (m_flGodmodeEnd > 0.0f) {
        if (now >= m_flGodmodeEnd) EndShield(pPlayer);
        else pPlayer->v.flags |= FL_GODMODE;
    }

    // 6. Cap descent before the original player code records fall speed.
    UpdateFallProtection(pPlayer);

    // 7. Super Jump Boots
    if (m_flSuperJumpEnd > 0.0f) {
        if (now >= m_flSuperJumpEnd) {
            m_flSuperJumpEnd = 0.0f;
        } else if ((pPlayer->v.button & IN_JUMP) && !(pPlayer->v.oldbuttons & IN_JUMP) &&
                   (pPlayer->v.flags & FL_ONGROUND)) {
            pPlayer->v.velocity.z = 560.0f;
            float yawRad = pPlayer->v.v_angle.y * (3.14159265f / 180.0f);
            pPlayer->v.velocity.x += cosf(yawRad) * 140.0f;
            pPlayer->v.velocity.y += sinf(yawRad) * 140.0f;
        }
    }

    // 8. Pogo Stick auto-bounce
    if (m_flPogoEnd > 0.0f) {
        if (now >= m_flPogoEnd) {
            m_flPogoEnd = 0.0f;
        } else if (now >= m_flNextPogoTick && (pPlayer->v.flags & FL_ONGROUND)) {
            m_flNextPogoTick = now + 1.05f;
            pPlayer->v.flags &= ~FL_ONGROUND;
            pPlayer->v.velocity.z = 390.0f;
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
        }
    }

    // 9. Nano Regeneration
    if (m_flRegenEnd > 0.0f) {
        if (now >= m_flRegenEnd) {
            m_flRegenEnd = 0.0f;
        } else if (now >= m_flNextRegenTick) {
            m_flNextRegenTick = now + 1.0f;
            if (pPlayer->v.health > 0.0f && pPlayer->v.health < 125.0f) {
                pPlayer->v.health += 7.0f;
                if (pPlayer->v.health > 125.0f) pPlayer->v.health = 125.0f;
            }
            if (PlayerHasSuit(pPlayer) && pPlayer->v.armorvalue < 125.0f) {
                pPlayer->v.armorvalue += 7.0f;
                if (pPlayer->v.armorvalue > 125.0f) pPlayer->v.armorvalue = 125.0f;
            }
        }
    }

    // 10. Barney's Beer (Drunk Camera sway)
    if (m_flDrunkEnd > 0.0f) {
        if (now >= m_flDrunkEnd) {
            m_flDrunkEnd = 0.0f;
            pPlayer->v.punchangle = m_previousPunchAngle;
        } else {
            pPlayer->v.punchangle.x = sinf(now * 2.7f) * 7.5f;
            pPlayer->v.punchangle.y = cosf(now * 2.1f) * 9.0f;
            pPlayer->v.punchangle.z = sinf(now * 3.4f) * 14.0f;
        }
    }

    // 11. Resonance Earthquake
    if (m_flQuakeEnd > 0.0f) {
        if (now >= m_flQuakeEnd) {
            m_flQuakeEnd = 0.0f;
        } else if (now >= m_flNextQuakeTick) {
            m_flNextQuakeTick = now + 0.65f;
            SendScreenShake(pPlayer, 10.0f, 65.0f, 0.8f);
            if (pPlayer->v.flags & FL_ONGROUND) {
                pPlayer->v.velocity.x += (float)((int)(NextXorShift(m_rngState) % 180) - 90);
                pPlayer->v.velocity.y += (float)((int)(NextXorShift(m_rngState) % 180) - 90);
            }
        }
    }

    // 12. Black Mesa Disco Rave
    if (m_flDiscoEnd > 0.0f) {
        if (now >= m_flDiscoEnd) {
            m_flDiscoEnd = 0.0f;
        } else if (now >= m_flNextDiscoTick) {
            m_flNextDiscoTick = now + 0.45f;
            static const byte kColors[6][3] = {
                { 255,  40, 180 },
                {  40, 240, 255 },
                {  80, 255,  70 },
                { 255, 220,  40 },
                { 180,  60, 255 },
                { 255,  80,  40 }
            };
            const byte *c = kColors[m_discoPhase % 6];
            m_discoPhase++;
            SendDynamicLight(pPlayer->v.origin + Vec3(0.0f, 0.0f, 32.0f), 28, c[0], c[1], c[2], 6, 15);
            SendScreenFade(pPlayer, c[0], c[1], c[2], 45, 0.25f, 0.05f, FFADE_IN);
        }
    }

    // 13. Armed Anomaly Shockwave Bomb (non-lethal knockback!)
    if (m_bAnomalyArmed) {
        if (now < m_flAnomalyDetonateTime) {
            if (now >= m_flNextAnomalyBeep) {
                m_flNextAnomalyBeep = now + 0.55f;
                SendDynamicLight(m_vecAnomalyOrigin + Vec3(0.0f, 0.0f, 16.0f), 22, 255, 50, 30, 5, 20);
                PlayCueSound(pPlayer, "fvox/beep.wav", PITCH_HIGH);
            }
        } else {
            m_bAnomalyArmed = false;
            SendTeleportSplash(m_vecAnomalyOrigin);
            SendDynamicLight(m_vecAnomalyOrigin + Vec3(0.0f, 0.0f, 24.0f), 38, 255, 140, 40, 12, 30);
            SendScreenShake(pPlayer, 14.0f, 80.0f, 1.4f);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_LOW);

            // The blast stays at the armed position even if the player moved.
            LaunchNearbyEnemies(pPlayer, m_vecAnomalyOrigin, 450.0f, 480.0f);

            // Push player away if still standing close (never lethal!)
            Vec3 delta = pPlayer->v.origin - m_vecAnomalyOrigin;
            float dist = delta.Length();
            if (dist < 320.0f) {
                ExtendFallProtection(4.5f);
                float scale = (320.0f - dist) / 320.0f;
                if (dist > 1.0f) {
                    pPlayer->v.velocity.x += (delta.x / dist) * (520.0f * scale);
                    pPlayer->v.velocity.y += (delta.y / dist) * (520.0f * scale);
                }
                pPlayer->v.flags &= ~FL_ONGROUND;
                pPlayer->v.velocity.z += 380.0f * scale + 120.0f;
                // Light non-lethal singe (never drops below 15 HP)
                if (!(pPlayer->v.flags & FL_GODMODE) && dist < 160.0f && pPlayer->v.health > 15.0f) {
                    float newHp = pPlayer->v.health - 15.0f;
                    pPlayer->v.health = (newHp < 15.0f) ? 15.0f : newHp;
                }
                SendScreenFade(pPlayer, 255, 120, 30, 95, 0.45f, 0.1f, FFADE_IN);
            }
        }
    }
}

void ChaosManager::OnStartFrame() {
    if (!m_serverActive || !gpGlobals) return;

    const float now = gpGlobals->time;
    float interval = ChaosInterval();

    // Detect save-load time jumps or interval reductions so timer never freezes
    if (now < m_flLastFrameTime - 0.1f) {
        ClearAllModifiers(FindActivePlayer(), true);
        ResetTransientState();
        m_checkpointPending = false;
        m_nextManualEventTime = now;
        m_flNextChaosTime = now + interval;
        m_flNextHudRefresh = now + 0.1f;
        m_lastBeepSecond = -1;
    } else if ((m_flNextChaosTime - now) > interval + 1.5f) {
        m_flNextChaosTime = now + interval;
        m_lastBeepSecond = -1;
    }
    m_flLastFrameTime = now;
    g_deathCounter.FlushIfDue(now);

    edict_t *pPlayer = FindActivePlayer();
    if (!pPlayer) return;

    // Death check (persists immediately before player clicks to reload autosave!)
    if (g_deathCounter.CheckPlayerState(pPlayer)) {
        ClearAllModifiers(pPlayer, true);
        ResetTransientState();
        ShowDeathBanner(pPlayer, g_deathCounter.GetTotalDeaths(), g_deathCounter.GetMapDeaths());
        return;
    }

    const bool isAlive = (pPlayer->v.deadflag == DEAD_NO && pPlayer->v.health > 0.0f);
    if (!isAlive) {
        // ClientKill may already have counted this death before the first
        // frame; transient abilities still need to be cleared in that case.
        ClearAllModifiers(pPlayer, true);
        ResetTransientState();
        // Hold chaos timer steady while player is on death screen.
        m_flNextChaosTime = now + interval;
        return;
    }

    if (m_introPending && now >= m_introAt) {
        m_introPending = false;
        if (!g_engfuncs.pfnCVarGetFloat || g_engfuncs.pfnCVarGetFloat("chaos_intro_troll") > 0.0f)
            ShowNewGameTroll(pPlayer);
    }
    UpdateCheckpoint(pPlayer);
    const bool sessionAvailable = SyncSafetyContext(pPlayer);
    if (sessionAvailable) {
        if (!m_restrictedContext) {
            UpdateItemQueue(pPlayer);
            ActivateTotemIfNeeded(pPlayer);
            UpdatePortalUse(pPlayer);
            RecordBreadcrumb(pPlayer);
        }
        // In a tram or cutscene, pre-existing disruptive effects were cleared
        // on entry and only visual events can be selected. Their animations
        // still need normal updates instead of being cancelled every frame.
        UpdateTimedModifiers(pPlayer);
        UpdateCsAnimeMeme(pPlayer);
    }

    const bool enabled = (!g_engfuncs.pfnCVarGetFloat || g_engfuncs.pfnCVarGetFloat("chaos_enabled") > 0.0f);
    if (enabled && sessionAvailable) {
        float rem = m_flNextChaosTime - now;
        int secLeft = (int)ceilf(rem);
        if (secLeft >= 1 && secLeft <= 3 && secLeft != m_lastBeepSecond) {
            m_lastBeepSecond = secLeft;
            PlayCueSound(pPlayer, "fvox/beep.wav", (secLeft == 1) ? PITCH_HIGH : PITCH_NORM);
        }

        if (now >= m_flNextChaosTime) {
            m_flNextChaosTime = now + interval;
            m_lastBeepSecond = -1;
            int nextEvent = PickNextEventId(pPlayer);
            ExecuteEvent(pPlayer, nextEvent);
        }
    } else {
        m_flNextChaosTime = now + interval;
        m_lastBeepSecond = -1;
    }

    if (now >= m_flNextHudRefresh) {
        m_flNextHudRefresh = now + 0.5f;
        RefreshStatusHud(pPlayer);
    }
}

enum SaveFieldMask {
    SAVE_SPEED = 1, SAVE_FOV = 2, SAVE_GODMODE = 4, SAVE_GLOW = 8, SAVE_PUNCH = 16
};
bool ChaosManager::BeforePlayerSave(edict_t *player, PlayerSaveSnapshot &snapshot) const {
    if (!player || player->free || !(player->v.flags & FL_CLIENT)) return false;
    if (m_flSpeedEnd <= 0.0f && m_flFovEnd <= 0.0f && m_flGodmodeEnd <= 0.0f && m_flDrunkEnd <= 0.0f) return false;
    if (player != FindActivePlayer()) return false;
    snapshot.changed = 0;
    snapshot.flags = player->v.flags;
    snapshot.maxspeed = player->v.maxspeed;
    snapshot.fov = player->v.fov;
    snapshot.renderfx = player->v.renderfx;
    snapshot.renderamt = player->v.renderamt;
    snapshot.rendercolor = player->v.rendercolor;
    snapshot.punchangle = player->v.punchangle;
    if (m_flSpeedEnd > 0.0f && isfinite(player->v.maxspeed) &&
        fabsf(player->v.maxspeed - m_flTargetSpeed) < 0.01f) {
        player->v.maxspeed = m_previousPlayerSpeed;
        snapshot.changed |= SAVE_SPEED;
    }
    if (m_flFovEnd > 0.0f && isfinite(player->v.fov) &&
        fabsf(player->v.fov - (float)m_iTargetFov) < 0.01f) {
        player->v.fov = m_previousFov;
        snapshot.changed |= SAVE_FOV;
    }
    if (m_flGodmodeEnd > 0.0f) {
        if (!m_previousGodmode && (player->v.flags & FL_GODMODE)) {
            player->v.flags &= ~FL_GODMODE;
            snapshot.changed |= SAVE_GODMODE;
        }
        if (IsChaosShieldGlow(player)) {
            player->v.renderfx = m_previousRenderFx;
            player->v.renderamt = m_previousRenderAmt;
            player->v.rendercolor = m_previousRenderColor;
            snapshot.changed |= SAVE_GLOW;
        }
    }
    if (m_flDrunkEnd > 0.0f) {
        player->v.punchangle = m_previousPunchAngle;
        snapshot.changed |= SAVE_PUNCH;
    }
    return snapshot.changed != 0;
}
void ChaosManager::AfterPlayerSave(edict_t *player, const PlayerSaveSnapshot &snapshot) const {
    if (!player || player->free) return;
    // Restore only fields temporarily changed above. The original DLL may
    // legitimately change other flags while it serializes the player.
    if (snapshot.changed & SAVE_GODMODE)
        player->v.flags = (player->v.flags & ~FL_GODMODE) | (snapshot.flags & FL_GODMODE);
    if (snapshot.changed & SAVE_SPEED) player->v.maxspeed = snapshot.maxspeed;
    if (snapshot.changed & SAVE_FOV) player->v.fov = snapshot.fov;
    if (snapshot.changed & SAVE_GLOW) {
        player->v.renderfx = snapshot.renderfx;
        player->v.renderamt = snapshot.renderamt;
        player->v.rendercolor = snapshot.rendercolor;
    }
    if (snapshot.changed & SAVE_PUNCH) player->v.punchangle = snapshot.punchangle;
}

void ChaosManager::OnPlayerPreThink(edict_t *pPlayer) {
    if (!pPlayer || pPlayer->free || !m_serverActive || pPlayer != FindActivePlayer()) return;
    if (g_deathCounter.CheckPlayerState(pPlayer)) {
        ClearAllModifiers(pPlayer, true);
        ResetTransientState();
        ShowDeathBanner(pPlayer, g_deathCounter.GetTotalDeaths(), g_deathCounter.GetMapDeaths());
        return;
    }
    if (!SyncSafetyContext(pPlayer)) return;
    if (!m_restrictedContext) {
        ActivateTotemIfNeeded(pPlayer);
        UpdateFallProtection(pPlayer);
    }
}

void ChaosManager::OnPlayerPostThink(edict_t *pPlayer) {
    if (!pPlayer || pPlayer->free || !m_serverActive || pPlayer != FindActivePlayer()) return;
    if (g_deathCounter.CheckPlayerState(pPlayer)) {
        ClearAllModifiers(pPlayer, true);
        ResetTransientState();
        ShowDeathBanner(pPlayer, g_deathCounter.GetTotalDeaths(), g_deathCounter.GetMapDeaths());
        return;
    }
    // The original PreThink can itself enter a train, ladder or scripted mode.
    if (!SyncSafetyContext(pPlayer)) return;
    if (!m_restrictedContext) ActivateTotemIfNeeded(pPlayer);
    if (m_flFovEnd > 0.0f && gpGlobals && gpGlobals->time < m_flFovEnd) {
        pPlayer->v.fov = (float)m_iTargetFov;
    }
}

bool ChaosManager::OnClientCommand(edict_t *pPlayer) {
    if (!g_engfuncs.pfnCmd_Argc || !g_engfuncs.pfnCmd_Argv) return false;
    if (g_engfuncs.pfnCmd_Argc() < 1) return false;

    const char *cmd = g_engfuncs.pfnCmd_Argv(0);
    if (!cmd) return false;

    if (StrCaseEq(cmd, "chaos_now")) {
        CmdTriggerRandom();
        return true;
    }
    if (StrCaseEq(cmd, "chaos_trigger")) {
        if (g_engfuncs.pfnCmd_Argc() >= 2) {
            CmdTriggerById(CmdArgInt(1, -1));
        } else {
            CmdTriggerRandom();
        }
        return true;
    }
    if (StrCaseEq(cmd, "chaos_reset_deaths")) {
        CmdResetDeaths();
        return true;
    }
    if (StrCaseEq(cmd, "chaos_toggle")) {
        CmdToggle();
        return true;
    }
    if (StrCaseEq(cmd, "chaos_list")) {
        CmdPrintList(pPlayer, CmdArgInt(1, 1));
        return true;
    }
    if (StrCaseEq(cmd, "chaos_status")) { CmdPrintStatus(pPlayer); return true; }
    if (StrCaseEq(cmd, "chaos_recover")) { CmdRecover(); return true; }
    if (StrCaseEq(cmd, "chaos_totem")) { CmdTotem(); return true; }
    if (StrCaseEq(cmd, "chaos_portal_gun")) { CmdPortalGun(); return true; }
    if (StrCaseEq(cmd, "chaos_portal_blue")) { CmdPortalPlace(0); return true; }
    if (StrCaseEq(cmd, "chaos_portal_orange")) { CmdPortalPlace(1); return true; }
    if (StrCaseEq(cmd, "chaos_portal_enter")) { CmdPortalEnter(); return true; }
    if (StrCaseEq(cmd, "chaos_speedrun")) { CmdSpeedrun(); return true; }
    if (StrCaseEq(cmd, "chaos_c1a1")) { CmdC1A1(false); return true; }
    if (StrCaseEq(cmd, "chaos_c1a1_go")) { CmdC1A1(true); return true; }
    return false;
}

void ChaosManager::CmdTriggerRandom() {
    edict_t *pPlayer = FindActivePlayer();
    if (!pPlayer || pPlayer->v.deadflag != DEAD_NO || pPlayer->v.health <= 0.0f) return;
    if (gpGlobals && gpGlobals->time < m_nextManualEventTime) return;
    if (!SyncSafetyContext(pPlayer)) {
        SendChatAndConsole(pPlayer, "[ХАОС] События доступны в одиночной игре.\n");
        return;
    }

    float interval = ChaosInterval();
    if (gpGlobals) m_flNextChaosTime = gpGlobals->time + interval;
    m_lastBeepSecond = -1;

    int evId = PickNextEventId(pPlayer);
    if (evId < 0) {
        SendChatAndConsole(pPlayer, "[ХАОС] На этой карте/в этом состоянии события временно недоступны.\n");
        return;
    }
    if (gpGlobals) m_nextManualEventTime = gpGlobals->time + 0.3f;
    ExecuteEvent(pPlayer, evId);
}

void ChaosManager::CmdTriggerById(int eventId) {
    edict_t *pPlayer = FindActivePlayer();
    if (!pPlayer || pPlayer->v.deadflag != DEAD_NO || pPlayer->v.health <= 0.0f) return;

    if (eventId < 0 || eventId >= CHAOS_EVENT_COUNT) {
        char message[128];
        snprintf(message, sizeof(message), "[ХАОС] Укажите ID от 0 до %d (список: chaos_list).\n", CHAOS_EVENT_COUNT - 1);
        SendChatAndConsole(pPlayer, message);
        return;
    }

    if (!SyncSafetyContext(pPlayer)) {
        SendChatAndConsole(pPlayer, "[ХАОС] События доступны в одиночной игре.\n");
        return;
    }
    if (!IsEventAllowed(pPlayer, eventId)) {
        SendChatAndConsole(pPlayer, "[ХАОС] Это событие недоступно: транспорт, HEV или ресурсы. Другие события продолжаются.\n");
        return;
    }
    if (gpGlobals && gpGlobals->time < m_nextManualEventTime) return;
    if (gpGlobals) m_nextManualEventTime = gpGlobals->time + 0.3f;
    float interval = ChaosInterval();
    if (gpGlobals) m_flNextChaosTime = gpGlobals->time + interval;
    m_lastBeepSecond = -1;
    ExecuteEvent(pPlayer, eventId);
}

void ChaosManager::CmdResetDeaths() {
    g_deathCounter.ResetDeaths();
    edict_t *pPlayer = FindActivePlayer();
    SendChatAndConsole(pPlayer, "[ХАОС] Счётчик смертей сброшен до 0!\n");
    if (pPlayer) RefreshStatusHud(pPlayer);
}

void ChaosManager::CmdToggle() {
    if (!g_engfuncs.pfnCVarGetFloat || !g_engfuncs.pfnCVarSetFloat) return;
    float cur = g_engfuncs.pfnCVarGetFloat("chaos_enabled");
    float next = (cur > 0.0f) ? 0.0f : 1.0f;
    g_engfuncs.pfnCVarSetFloat("chaos_enabled", next);

    edict_t *pPlayer = FindActivePlayer();
    if (next <= 0.0f) {
        ClearAllModifiers(pPlayer, true);
        SendChatAndConsole(pPlayer, "[ХАОС] Рандомные события поставлены на паузу (chaos_toggle для продолжения).\n");
    } else {
        SendChatAndConsole(pPlayer, "[ХАОС] Рандомные события снова АКТИВНЫ!\n");
    }
    if (pPlayer) RefreshStatusHud(pPlayer);
}

void ChaosManager::CmdRecover() {
    edict_t *player = FindActivePlayer();
    ClearAllModifiers(player, true);
    ResetTransientState();
    // Explicit repair for saves made by older mod versions. Unlike automatic
    // cleanup, this deliberately restores stock defaults at the user's request.
    if (g_engfuncs.pfnCVarSetFloat) {
        g_engfuncs.pfnCVarSetFloat("sv_gravity", 800.0f);
        g_engfuncs.pfnCVarSetFloat("sv_maxspeed", 320.0f);
        g_engfuncs.pfnCVarSetFloat("sv_accelerate", 10.0f);
        g_engfuncs.pfnCVarSetFloat("sv_friction", 4.0f);
        g_engfuncs.pfnCVarSetFloat("sv_airaccelerate", 10.0f);
    }
    if (player && !player->free) {
        player->v.flags &= ~FL_GODMODE;
        if (player->v.renderfx == kRenderFxGlowShell) {
            player->v.renderfx = kRenderFxNone;
            player->v.renderamt = 0.0f;
        }
        player->v.maxspeed = 320.0f;
        if (g_engfuncs.pfnSetClientMaxspeed) g_engfuncs.pfnSetClientMaxspeed(player, 320.0f);
        player->v.fov = 0.0f;
        SendSetFOV(player, 0);
    }
    SendChatAndConsole(player, "[ХАОС] Аварийное восстановление: физика и камера возвращены к настройкам HL1.\n");
}

void ChaosManager::CmdPrintList(edict_t *pPlayer, int page) {
    if (!pPlayer) pPlayer = FindActivePlayer();
    auto printLine = [&](const char *line) {
        if (pPlayer && g_engfuncs.pfnClientPrintf) g_engfuncs.pfnClientPrintf(pPlayer, print_console, line);
        else if (g_engfuncs.pfnServerPrint) g_engfuncs.pfnServerPrint(line);
    };
    const int perPage = 25;
    const int pages = (CHAOS_EVENT_COUNT + perPage - 1) / perPage;
    if (page < 1) page = 1;
    if (page > pages) page = pages;
    char line[256];
    snprintf(line, sizeof(line), "=== ХАОС: страница %d/%d, всего %d (chaos_list <страница>) ===\n", page, pages, CHAOS_EVENT_COUNT);
    printLine(line);
    int end = page * perPage;
    if (end > CHAOS_EVENT_COUNT) end = CHAOS_EVENT_COUNT;
    for (int i = (page - 1) * perPage; i < end; ++i) {
        const ChaosEventDef *ev = ChaosGetEventDef(i);
        snprintf(line, sizeof(line), " [%3d] %-32s | %s\n", ev->id, ev->titleEn, ev->titleRu);
        printLine(line);
    }
    printLine("Вызов: chaos_trigger <ID> (опасные/недоступные события блокируются).\n");
}

void ChaosManager::CmdPrintStatus(edict_t *pPlayer) {
    if (!pPlayer) pPlayer = FindActivePlayer();
    char buf[256];
    snprintf(buf, sizeof(buf),
             "[ХАОС v1.2] Смертей всего: %d | На этой карте: %d | Пережито событий: %d | До след. события: %.1f сек\n",
             g_deathCounter.GetTotalDeaths(),
             g_deathCounter.GetMapDeaths(),
             g_deathCounter.GetTotalEvents(),
             GetTimeRemaining());
    SendChatAndConsole(pPlayer, buf);
    const bool enabled = !g_engfuncs.pfnCVarGetFloat || g_engfuncs.pfnCVarGetFloat("chaos_enabled") > 0.0f;
    if (!enabled)
        SendChatAndConsole(pPlayer, "[ХАОС] Автособытия на паузе: chaos_enabled 1 или chaos_toggle.\n");
    else if (!IsChaosSessionAvailable(pPlayer))
        SendChatAndConsole(pPlayer, "[ХАОС] Ожидание живого игрока в одиночной игре.\n");
    else if (IsRestrictedState(pPlayer))
        SendChatAndConsole(pPlayer, "[ХАОС] События работают: во вступлении и транспорте доступны визуальные эффекты.\n");
    else
        SendChatAndConsole(pPlayer, "[ХАОС] Автособытия работают; ограничения действуют только для отдельных событий.\n");
}
