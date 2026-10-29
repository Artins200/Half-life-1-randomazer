#include "chaos_events.h"
#include "death_counter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

ChaosManager g_chaos;

static const ChaosEventDef g_eventDefs[CHAOS_EVENT_COUNT] = {
    // === MONSTERS & NPCS (0..11) ===
    {  0, "HEADCRAB AMBUSH!",        "Засада хедкрабов!",         "Вокруг появились 3 прыгучих хедкраба!",              CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true  },
    {  1, "BABY CRAB SWARM!",        "Рой мини-хедкрабов!",       "Пять быстрых детёнышей хедкраба под ногами!",        CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true  },
    {  2, "HOUNDEYE SONIC PACK!",    "Стая хаундаев!",            "Два звуковых пса Xen готовят ультразвуковую волну!", CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true  },
    {  3, "BULLSQUID SURPRISE!",     "Кислотный буллсквид!",      "Рядом телепортировался голодный буллсквид!",         CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true  },
    {  4, "ZOMBIE OUTBREAK!",        "Нашествие зомби!",          "Два зомбированных учёных вышли на охоту!",           CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true  },
    {  5, "XEN VORTIGAUNT RAID!",    "Вторжение вортигонта!",     "Портал из Xen перенёс боевого вортигонта!",          CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true  },
    {  6, "HECU MARINE DROP!",       "Десант спецназа HECU!",     "Пехотинец HECU высадился прямо поблизости!",         CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, true,  true  },
    {  7, "ALIEN GRUNT HORNETS!",    "Пехотинец пришельцев!",     "Мощный Alien Grunt с мухомётом вышел из портала!",   CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, true,  true  },
    {  8, "SNARK PARTY!",            "Вечеринка снарков!",        "Четыре кусачих жука-снарка бегут грызть всех!",      CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, false },
    {  9, "ROACH INFESTATION!",      "Нашествие тараканов!",      "Десять шуршащих тараканов Чёрной Мезы!",             CHAOS_CAT_MONSTERS, CHAOS_TONE_WEIRD,    0.0f, false, false },
    { 10, "BARNEY REINFORCEMENT!",   "Подмога от Барни!",         "Дружелюбный охранник Барни пришёл прикрыть спину!",  CHAOS_CAT_MONSTERS, CHAOS_TONE_GOOD,     0.0f, false, true  },
    { 11, "PANICKED SCIENTISTS!",    "Паника научного отдела!",   "Трое учёных появились рядом в полной панике!",       CHAOS_CAT_MONSTERS, CHAOS_TONE_WEIRD,    0.0f, false, true  },

    // === PHYSICS & MOVEMENT (12..21) ===
    { 12, "MOON GRAVITY (10S)",      "Лунная гравитация!",        "Притяжение снижено в 5 раз на 10 секунд!",           CHAOS_CAT_PHYSICS,  CHAOS_TONE_NEUTRAL, 10.0f, false, false },
    { 13, "JUPITER GRAVITY (8S)",    "Гравитация Юпитера!",       "Тяжёлое притяжение x2 на 8 секунд!",                 CHAOS_CAT_PHYSICS,  CHAOS_TONE_DANGER,   8.0f, false, false },
    { 14, "TURBO FREEMAN (10S)",     "Турбо-ускорение Гордона!",  "Сверхзвуковой бег Гордона Фримена на 10 секунд!",    CHAOS_CAT_PHYSICS,  CHAOS_TONE_GOOD,    10.0f, false, false },
    { 15, "SNAIL PACE (7S)",         "Черепаший шаг!",            "Ноги налились свинцом — замедление на 7 секунд!",    CHAOS_CAT_PHYSICS,  CHAOS_TONE_DANGER,   7.0f, false, false },
    { 16, "ICE SKATING RINK (10S)",  "Гололёд в Чёрной Мезе!",    "Пол стал ледяным: скольжение как на коньках!",       CHAOS_CAT_PHYSICS,  CHAOS_TONE_NEUTRAL, 10.0f, false, false },
    { 17, "SUPER JUMP BOOTS (10S)",  "Пружинные ботинки!",        "Каждый прыжок подбрасывает к потолку (10 сек)!",     CHAOS_CAT_PHYSICS,  CHAOS_TONE_GOOD,    10.0f, false, true  },
    { 18, "POGO STICK MODE (9S)",    "Режим кузнечика!",          "Гордон неудержимо прыгает каждые 1.1 секунды!",      CHAOS_CAT_PHYSICS,  CHAOS_TONE_WEIRD,    9.0f, false, true  },
    { 19, "SKY LAUNCHPAD!",          "Катапульта в воздух!",      "Резкий подброс вверх с защитой от урона падения!",   CHAOS_CAT_PHYSICS,  CHAOS_TONE_NEUTRAL,  0.0f, false, true  },
    { 20, "DEJA VU (REWIND 3.5S)",   "Дежавю!",                   "Возврат Гордона на позицию 3.5 секунды назад!",      CHAOS_CAT_PHYSICS,  CHAOS_TONE_WEIRD,    0.0f, false, true  },
    { 21, "BLINK DASH FORWARD!",     "Телепорт-рывок вперёд!",    "Мгновенный блинк вперёд по направлению взгляда!",    CHAOS_CAT_PHYSICS,  CHAOS_TONE_NEUTRAL,  0.0f, false, true  },

    // === WEAPONS & INVENTORY (22..27) ===
    { 22, "RANDOM WEAPON GIFT!",     "Случайная пушка!",          "Выдано случайное мощное оружие с боезапасом!",       CHAOS_CAT_WEAPONS,  CHAOS_TONE_GOOD,     0.0f, true,  true  },
    { 23, "FULL ARSENAL JACKPOT!",   "ДЖЕКПОТ: Полный арсенал!",  "Выданы все пушки и запас патронов!",                 CHAOS_CAT_WEAPONS,  CHAOS_TONE_GOOD,     0.0f, true,  true  },
    { 24, "CROWBAR ONLY!",           "Только монтировка!",        "Пушки отобраны — в руках только верная монтировка!", CHAOS_CAT_WEAPONS,  CHAOS_TONE_DANGER,   0.0f, true,  true  },
    { 25, "DEMOMAN LOADOUT!",        "Набор подрывника!",         "Только взрывчатка: RPG, гранаты, ранцы и мины!",     CHAOS_CAT_WEAPONS,  CHAOS_TONE_WEIRD,    0.0f, true,  true  },
    { 26, "SUPPLY DROP SHOWER!",     "Дождь припасов!",           "Аптечки, батареи HEV и патроны прямо в руки!",       CHAOS_CAT_WEAPONS,  CHAOS_TONE_GOOD,     0.0f, true,  false },
    { 27, "ANOMALY BOMB (3S FUSE)!", "Аномальная бомба (3 сек)!", "Отойди с точки! Через 3 сек ударная волна!",         CHAOS_CAT_WEAPONS,  CHAOS_TONE_DANGER,   3.0f, false, true  },

    // === HEALTH, ARMOR & SURVIVAL (28..32) ===
    { 28, "HEV OVERCHARGE!",         "Перегрузка костюма HEV!",   "100 HP и 150 единиц заряда брони костюма!",          CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD,     0.0f, false, false },
    { 29, "GLASS CANNON (15/100)!",  "Хрустальная пушка!",        "15 здоровья, но 100 брони HEV — береги себя!",       CHAOS_CAT_SURVIVAL, CHAOS_TONE_DANGER,   0.0f, false, false },
    { 30, "NANO REGENERATION (10S)", "Нано-регенерация!",         "+7 HP и +7 брони каждую секунду на 10 секунд!",      CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD,    10.0f, false, false },
    { 31, "HP <-> ARMOR SWAP!",      "Обмен ХП и брони местами!", "Здоровье и броня поменялись значениями!",            CHAOS_CAT_SURVIVAL, CHAOS_TONE_WEIRD,    0.0f, false, false },
    { 32, "GODMODE SHIELD (8S)!",    "Золотой щит неуязвимости!", "Полное бессмертие на 8 секунд — врывайся в бой!",    CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD,     8.0f, false, false },

    // === VISUAL, CAMERA & WORLD CHAOS (33..37) ===
    { 33, "QUAKE PRO FOV (130)",     "Рыбий глаз Quake Pro!",     "Сверхширокий угол обзора FOV 130 на 10 секунд!",     CHAOS_CAT_VISUAL,   CHAOS_TONE_WEIRD,   10.0f, false, false },
    { 34, "SNIPER ZOOM (FOV 38)",    "Туннельное зрение!",        "Экстремальный зум камеры FOV 38 на 8 секунд!",       CHAOS_CAT_VISUAL,   CHAOS_TONE_WEIRD,    8.0f, false, false },
    { 35, "BARNEY'S BEER (DRUNK)",   "Пиво от Барни!",            "Камера пьяно раскачивается 10 секунд!",              CHAOS_CAT_VISUAL,   CHAOS_TONE_WEIRD,   10.0f, false, false },
    { 36, "RESONANCE EARTHQUAKE!",   "Каскадный резонанс!",       "Мощное землетрясение и подземные толчки 8 сек!",     CHAOS_CAT_VISUAL,   CHAOS_TONE_DANGER,   8.0f, false, false },
    { 37, "BLACK MESA DISCO RAVE!",  "Дискотека в Чёрной Мезе!",  "Неоновый рейв + все враги вокруг взлетают вверх!",   CHAOS_CAT_VISUAL,   CHAOS_TONE_WEIRD,   10.0f, false, false }
};

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

// Static server command callbacks
static void SrvCmd_ChaosNow(void) { g_chaos.CmdTriggerRandom(); }
static void SrvCmd_ChaosResetDeaths(void) { g_chaos.CmdResetDeaths(); }
static void SrvCmd_ChaosToggle(void) { g_chaos.CmdToggle(); }
static void SrvCmd_ChaosList(void) { g_chaos.CmdPrintList(nullptr); }
static void SrvCmd_ChaosStatus(void) { g_chaos.CmdPrintStatus(nullptr); }
static void SrvCmd_ChaosTrigger(void) {
    if (g_engfuncs.pfnCmd_Argc && g_engfuncs.pfnCmd_Argc() >= 2) {
        int id = atoi(g_engfuncs.pfnCmd_Argv(1));
        g_chaos.CmdTriggerById(id);
    } else {
        g_chaos.CmdTriggerRandom();
    }
}

ChaosManager::ChaosManager()
    : m_cvarsRegistered(false),
      m_msgSetFOV(0),
      m_msgScreenShake(0),
      m_msgScreenFade(0),
      m_msgSayText(0),
      m_msgTextMsg(0),
      m_precachedModelCount(0),
      m_precachedSoundCount(0),
      m_bMonstersPrecached(false),
      m_pEdictList(nullptr),
      m_edictCount(0),
      m_clientMax(1),
      m_serverActive(false),
      m_flLastFrameTime(0.0f),
      m_flNextChaosTime(10.0f),
      m_flNextHudRefresh(0.0f),
      m_lastBeepSecond(-1),
      m_lastEventId(-1),
      m_recentHead(0),
      m_rngState(0x9E3779B9u),
      m_breadcrumbHead(0),
      m_flNextBreadcrumbTime(0.0f),
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
    return &g_eventDefs[id];
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
    }
    if (g_engfuncs.pfnAddServerCommand) {
        g_engfuncs.pfnAddServerCommand((char *)"chaos_now", SrvCmd_ChaosNow);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_trigger", SrvCmd_ChaosTrigger);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_reset_deaths", SrvCmd_ChaosResetDeaths);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_toggle", SrvCmd_ChaosToggle);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_list", SrvCmd_ChaosList);
        g_engfuncs.pfnAddServerCommand((char *)"chaos_status", SrvCmd_ChaosStatus);
    }
    g_deathCounter.Init();
    m_cvarsRegistered = true;
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
    m_precachedModelCount = 0;
    m_precachedSoundCount = 0;
    m_bMonstersPrecached = false;
}

void ChaosManager::OnTrackPrecacheModel(const char *modelName) {
    if (modelName && modelName[0] != '\0') {
        m_precachedModelCount++;
    }
}

void ChaosManager::OnTrackPrecacheSound(const char *soundName) {
    if (soundName && soundName[0] != '\0') {
        m_precachedSoundCount++;
    }
}

bool ChaosManager::CanPrecacheMore() const {
    return (m_precachedModelCount < 475 && m_precachedSoundCount < 465);
}

bool ChaosManager::PrecacheMonsterViaMaker(const char *monsterClassname) {
    if (!CanPrecacheMore()) {
        return false;
    }
    if (!g_engfuncs.pfnCreateNamedEntity || !g_engfuncs.pfnAllocString ||
        !g_origDllFuncs.pfnKeyValue || !g_origDllFuncs.pfnSpawn || !g_engfuncs.pfnRemoveEntity) {
        return false;
    }

    edict_t *maker = g_engfuncs.pfnCreateNamedEntity(g_engfuncs.pfnAllocString("monstermaker"));
    if (FNullEnt(maker)) {
        return false;
    }

    KeyValueData kvd1;
    kvd1.szClassName = (char *)"monstermaker";
    kvd1.szKeyName   = (char *)"monstertype";
    kvd1.szValue     = (char *)monsterClassname;
    kvd1.fHandled    = 0;
    g_origDllFuncs.pfnKeyValue(maker, &kvd1);

    KeyValueData kvd2;
    kvd2.szClassName = (char *)"monstermaker";
    kvd2.szKeyName   = (char *)"targetname";
    kvd2.szValue     = (char *)"__chaos_precache_only__";
    kvd2.fHandled    = 0;
    g_origDllFuncs.pfnKeyValue(maker, &kvd2);

    g_origDllFuncs.pfnSpawn(maker);
    g_engfuncs.pfnRemoveEntity(maker);
    return true;
}

void ChaosManager::OnWorldSpawnPost() {
    if (m_bMonstersPrecached) {
        return;
    }
    m_bMonstersPrecached = true;

    // Precache Chaos sound cues
    if (g_engfuncs.pfnPrecacheSound && CanPrecacheMore()) {
        g_engfuncs.pfnPrecacheSound((char *)"fvox/beep.wav");
        g_engfuncs.pfnPrecacheSound((char *)"debris/beamstart7.wav");
        g_engfuncs.pfnPrecacheSound((char *)"items/suitchargeok1.wav");
        g_engfuncs.pfnPrecacheSound((char *)"items/medshot4.wav");
        g_engfuncs.pfnPrecacheSound((char *)"items/gunpickup2.wav");
        g_engfuncs.pfnPrecacheSound((char *)"items/weapondrop1.wav");
        g_engfuncs.pfnPrecacheSound((char *)"items/suitchargeno1.wav");
        g_engfuncs.pfnPrecacheSound((char *)"debris/zap4.wav");
        g_engfuncs.pfnPrecacheSound((char *)"buttons/button3.wav");
        g_engfuncs.pfnPrecacheSound((char *)"buttons/button10.wav");
    }

    // Precache item_healthkit & Chaos monster pool safely via Valve's monstermaker
    static const char *const kPrecacheClasses[] = {
        "item_healthkit",
        "monster_headcrab",
        "monster_babycrab",
        "monster_houndeye",
        "monster_bullchicken",
        "monster_zombie",
        "monster_alien_slave",
        "monster_cockroach",
        "monster_barney",
        "monster_scientist",
        "monster_human_grunt",
        "monster_alien_grunt"
    };
    for (size_t i = 0; i < sizeof(kPrecacheClasses) / sizeof(kPrecacheClasses[0]); ++i) {
        if (!CanPrecacheMore()) break;
        PrecacheMonsterViaMaker(kPrecacheClasses[i]);
    }
}

void ChaosManager::OnServerActivate(edict_t *pEdictList, int edictCount, int clientMax) {
    m_pEdictList = pEdictList;
    m_edictCount = edictCount;
    m_clientMax = (clientMax > 0) ? clientMax : 1;
    m_serverActive = true;

    const char *mapName = (gpGlobals && gpGlobals->mapname) ? STRING(gpGlobals->mapname) : "";
    g_deathCounter.OnMapStart(mapName);

    // Seed RNG with map name + current stats + engine random
    if (g_engfuncs.pfnRandomLong) {
        m_rngState ^= (uint32_t)g_engfuncs.pfnRandomLong(1, 0x7FFFFFFF);
    }
    m_rngState ^= (uint32_t)(g_deathCounter.GetTotalDeaths() * 2654435761u + g_deathCounter.GetTotalEvents() * 40503u);

    float interval = (g_engfuncs.pfnCVarGetFloat) ? g_engfuncs.pfnCVarGetFloat("chaos_interval") : 10.0f;
    if (interval < 2.0f) interval = 10.0f;

    float now = gpGlobals ? gpGlobals->time : 0.0f;
    m_flLastFrameTime = now;
    m_flNextChaosTime = now + interval;
    m_flNextHudRefresh = now + 0.5f;
    m_lastBeepSecond = -1;

    for (int i = 0; i < CHAOS_BREADCRUMB_COUNT; ++i) {
        m_breadcrumbs[i].valid = false;
    }
    m_flNextBreadcrumbTime = now + 0.5f;

    ClearAllModifiers(nullptr, true);
}

void ChaosManager::OnServerDeactivate() {
    ClearAllModifiers(FindActivePlayer(), true);
    m_serverActive = false;
}

void ChaosManager::OnClientPutInServer(edict_t *pPlayer) {
    g_deathCounter.OnClientPutInServer();
    ClearAllModifiers(pPlayer, true);

    float interval = (g_engfuncs.pfnCVarGetFloat) ? g_engfuncs.pfnCVarGetFloat("chaos_interval") : 10.0f;
    if (interval < 2.0f) interval = 10.0f;

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
    return (pPlayer->v.weapons & (1 << WEAPON_SUIT)) != 0;
}

void ChaosManager::ClearAllModifiers(edict_t *pPlayer, bool restoreEngineCvars) {
    m_flGravityEnd = 0.0f;
    m_flTargetGravity = 800.0f;
    m_szGravityLabel = nullptr;

    m_flSpeedEnd = 0.0f;
    m_flTargetSpeed = 320.0f;
    m_szSpeedLabel = nullptr;

    m_flFrictionEnd = 0.0f;
    m_flTargetFriction = 4.0f;
    m_flTargetAirAccelerate = 10.0f;

    m_flFovEnd = 0.0f;
    m_iTargetFov = 0;
    m_szFovLabel = nullptr;

    m_flGodmodeEnd = 0.0f;
    m_flFallProtectEnd = 0.0f;
    m_flSuperJumpEnd = 0.0f;
    m_flPogoEnd = 0.0f;
    m_flRegenEnd = 0.0f;
    m_flDrunkEnd = 0.0f;
    m_flQuakeEnd = 0.0f;
    m_flDiscoEnd = 0.0f;
    m_bAnomalyArmed = false;

    if (restoreEngineCvars && g_engfuncs.pfnCVarSetFloat) {
        g_engfuncs.pfnCVarSetFloat("sv_gravity", 800.0f);
        g_engfuncs.pfnCVarSetFloat("sv_friction", 4.0f);
        g_engfuncs.pfnCVarSetFloat("sv_maxspeed", 320.0f);
        g_engfuncs.pfnCVarSetFloat("sv_airaccelerate", 10.0f);
        g_engfuncs.pfnCVarSetFloat("sv_accelerate", 10.0f);
    }

    if (pPlayer && !pPlayer->free) {
        pPlayer->v.gravity = 1.0f;
        pPlayer->v.friction = 1.0f;
        pPlayer->v.flags &= ~FL_GODMODE;
        if (pPlayer->v.renderfx == kRenderFxGlowShell) {
            pPlayer->v.renderfx = kRenderFxNone;
            pPlayer->v.renderamt = 0.0f;
        }
        if (g_engfuncs.pfnSetClientMaxspeed) {
            g_engfuncs.pfnSetClientMaxspeed(pPlayer, 320.0f);
        }
        if (pPlayer->v.deadflag == DEAD_NO) {
            SendSetFOV(pPlayer, 0);
        }
    }
}

uint32_t static NextXorShift(uint32_t &state) {
    uint32_t x = state ? state : 0xA341316Cu;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

int ChaosManager::PickNextEventId(const edict_t *pPlayer) {
    const bool hasSuit = PlayerHasSuit(pPlayer);
    const bool onTram  = IsIntroTramMap() || (pPlayer && (pPlayer->v.flags & FL_ONTRAIN));

    int candidates[CHAOS_EVENT_COUNT];
    int candidateCount = 0;

    for (int pass = 0; pass < 2 && candidateCount == 0; ++pass) {
        for (int i = 0; i < CHAOS_EVENT_COUNT; ++i) {
            const ChaosEventDef &ev = g_eventDefs[i];
            if (ev.requiresSuit && !hasSuit) continue;
            if (ev.skipOnTram && onTram) continue;

            if (pass == 0) {
                bool recentlyUsed = false;
                for (int h = 0; h < CHAOS_HISTORY_SIZE; ++h) {
                    if (m_recentEvents[h] == i) {
                        recentlyUsed = true;
                        break;
                    }
                }
                if (recentlyUsed) continue;
            }
            candidates[candidateCount++] = i;
        }
    }

    if (candidateCount <= 0) {
        return 12; // Moon gravity fallback
    }

    uint32_t r = NextXorShift(m_rngState);
    if (g_engfuncs.pfnRandomLong) {
        r ^= (uint32_t)g_engfuncs.pfnRandomLong(0, 100000);
    }
    int chosen = candidates[r % (uint32_t)candidateCount];

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
        if (contents == CONTENTS_SOLID || contents == CONTENTS_SKY || contents == CONTENTS_LAVA) {
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

        Vec3 finalSpot = (floorTr.flFraction < 1.0f)
            ? Vec3(floorTr.vecEndPos.x, floorTr.vecEndPos.y, floorTr.vecEndPos.z + 2.0f)
            : candidate;

        TraceResult hullCheck;
        memset((void *)&hullCheck, 0, sizeof(hullCheck));
        g_engfuncs.pfnTraceHull(finalSpot, finalSpot, 0 /*dont_ignore_monsters*/, hullType, (edict_t *)pPlayer, &hullCheck);
        if (!hullCheck.fAllSolid && !hullCheck.fStartSolid) {
            outPos = finalSpot;
            return true;
        }
    }

    return false;
}

int ChaosManager::SpawnMonstersAroundPlayer(edict_t *pPlayer, const char *classname, int count, int hullType, bool friendly) {
    if (!pPlayer || !classname || !g_engfuncs.pfnCreateNamedEntity || !g_origDllFuncs.pfnSpawn) {
        return 0;
    }

    // Entity cap guard so long play sessions never exhaust edicts
    if (g_engfuncs.pfnNumberOfEntities && gpGlobals && gpGlobals->maxEntities > 128) {
        if (g_engfuncs.pfnNumberOfEntities() > gpGlobals->maxEntities - 64) {
            return 0;
        }
    }

    int spawned = 0;
    for (int i = 0; i < count; ++i) {
        m_rngState += (uint32_t)(i * 7919 + 13);
        Vec3 spawnPos;
        if (!FindSafeSpawnSpot(pPlayer, hullType, 96.0f, 260.0f, spawnPos)) {
            continue;
        }

        edict_t *pent = g_engfuncs.pfnCreateNamedEntity(g_engfuncs.pfnAllocString(classname));
        if (FNullEnt(pent)) {
            continue;
        }

        Vec3 toPlayer = pPlayer->v.origin - spawnPos;
        float yaw = atan2f(toPlayer.y, toPlayer.x) * (180.0f / 3.14159265f);

        pent->v.origin = spawnPos;
        pent->v.angles = Vec3(0.0f, yaw, 0.0f);
        pent->v.iuser4 = 0xC4A05; // Mark as Chaos-spawned entity

        g_origDllFuncs.pfnSpawn(pent);

        if (!pent->free) {
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
    if (!pPlayer || !itemClassname || !g_engfuncs.pfnCreateNamedEntity ||
        !g_origDllFuncs.pfnSpawn || !g_origDllFuncs.pfnTouch) {
        return;
    }

    edict_t *pent = g_engfuncs.pfnCreateNamedEntity(g_engfuncs.pfnAllocString(itemClassname));
    if (FNullEnt(pent)) {
        return;
    }

    pent->v.origin = pPlayer->v.origin;
    pent->v.spawnflags |= SF_NORESPAWN;
    g_origDllFuncs.pfnSpawn(pent);
    if (!pent->free) {
        g_origDllFuncs.pfnTouch(pent, pPlayer);
    }

    // Clean up uncollected item (e.g., ammo already at max capacity) so floor stays clean
    if (!pent->free && !(pent->v.flags & FL_KILLME) &&
        pent->v.owner != pPlayer && pent->v.aiment != pPlayer &&
        pent->v.solid != SOLID_NOT && !(pent->v.effects & EF_NODRAW) &&
        g_engfuncs.pfnRemoveEntity) {
        g_engfuncs.pfnRemoveEntity(pent);
    }
}

void ChaosManager::StripPlayerWeapons(edict_t *pPlayer) {
    if (!pPlayer || !g_engfuncs.pfnCreateNamedEntity || !g_origDllFuncs.pfnSpawn || !g_origDllFuncs.pfnUse) {
        return;
    }
    const bool hadSuit = PlayerHasSuit(pPlayer);

    edict_t *strip = g_engfuncs.pfnCreateNamedEntity(g_engfuncs.pfnAllocString("player_weaponstrip"));
    if (!FNullEnt(strip)) {
        strip->v.origin = pPlayer->v.origin;
        g_origDllFuncs.pfnSpawn(strip);
        g_origDllFuncs.pfnUse(strip, pPlayer);
        if (!strip->free && g_engfuncs.pfnRemoveEntity) {
            g_engfuncs.pfnRemoveEntity(strip);
        }
    }

    // Preserve HEV suit flag (CStripWeapons::Use passes FALSE, double-ensure)
    if (hadSuit) {
        pPlayer->v.weapons |= (1 << WEAPON_SUIT);
    }
}

int ChaosManager::LaunchNearbyEnemies(edict_t *pPlayer, float radius, float upwardVel) {
    if (!pPlayer || !g_engfuncs.pfnFindEntityInSphere) {
        return 0;
    }

    int count = 0;
    edict_t *ent = nullptr;
    while ((ent = g_engfuncs.pfnFindEntityInSphere(ent, pPlayer->v.origin, radius)) != nullptr) {
        if (ent == pPlayer || ent->free) continue;
        if (!(ent->v.flags & FL_MONSTER)) continue;
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
        ent->v.origin.z += 4.0f;
        ent->v.velocity.x += (float)((int)(NextXorShift(m_rngState) % 240) - 120);
        ent->v.velocity.y += (float)((int)(NextXorShift(m_rngState) % 240) - 120);
        ent->v.velocity.z = upwardVel;
        ent->v.avelocity.y = 360.0f;
        count++;
    }
    return count;
}

void ChaosManager::PlayCueSound(edict_t *pPlayer, const char *sample, int pitch) {
    if (!pPlayer || !sample || !g_engfuncs.pfnEmitSound) return;
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

    char safeBuf[500];
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
        g_engfuncs.pfnMessageBegin(MSG_ONE, m_msgSayText, nullptr, pPlayer);
        g_engfuncs.pfnWriteByte(entIdx & 0xFF);
        g_engfuncs.pfnWriteString(msg);
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
    if (m_bAnomalyArmed && m_flAnomalyDetonateTime > now) {
        appendMod("BOMB", m_flAnomalyDetonateTime);
    }
}

void ChaosManager::RefreshStatusHud(edict_t *pPlayer) {
    if (!pPlayer || !gpGlobals) return;

    const bool enabled = (!g_engfuncs.pfnCVarGetFloat || g_engfuncs.pfnCVarGetFloat("chaos_enabled") > 0.0f);
    float interval = (g_engfuncs.pfnCVarGetFloat) ? g_engfuncs.pfnCVarGetFloat("chaos_interval") : 10.0f;
    if (interval < 2.0f) interval = 10.0f;

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

    char hudBuf[384];
    if (enabled) {
        snprintf(hudBuf, sizeof(hudBuf),
                 "=== HL1 CHAOS MOD ===\n"
                 "DEATHS: %d  (MAP: %d)\n"
                 "NEXT: %2ds [%s]\n"
                 "LAST: %s%s%s",
                 g_deathCounter.GetTotalDeaths(),
                 g_deathCounter.GetMapDeaths(),
                 secLeft,
                 bar,
                 m_lastEventName,
                 (activeLine[0] != '\0') ? "\n" : "",
                 activeLine);
    } else {
        snprintf(hudBuf, sizeof(hudBuf),
                 "=== HL1 CHAOS [PAUSED] ===\n"
                 "DEATHS: %d  (MAP: %d)\n"
                 "Type chaos_toggle to resume",
                 g_deathCounter.GetTotalDeaths(),
                 g_deathCounter.GetMapDeaths());
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
    parms.holdTime = 0.65f;
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

    const ChaosEventDef &ev = g_eventDefs[eventId];
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

    switch (eventId) {
        case 0: // HEADCRAB AMBUSH
            SpawnMonstersAroundPlayer(pPlayer, "monster_headcrab", 3, head_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_NORM);
            break;

        case 1: // BABY CRAB SWARM
            SpawnMonstersAroundPlayer(pPlayer, "monster_babycrab", 5, head_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_HIGH);
            break;

        case 2: // HOUNDEYE PACK
            SpawnMonstersAroundPlayer(pPlayer, "monster_houndeye", 2, human_hull, false);
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_NORM);
            break;

        case 3: // BULLSQUID SURPRISE
            if (SpawnMonstersAroundPlayer(pPlayer, "monster_bullchicken", 1, human_hull, false) == 0) {
                SpawnMonstersAroundPlayer(pPlayer, "monster_headcrab", 2, head_hull, false);
            }
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_LOW);
            break;

        case 4: // ZOMBIE OUTBREAK
            if (SpawnMonstersAroundPlayer(pPlayer, "monster_zombie", 2, human_hull, false) == 0) {
                SpawnMonstersAroundPlayer(pPlayer, "monster_headcrab", 3, head_hull, false);
            }
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_LOW);
            break;

        case 5: // VORTIGAUNT RAID
            SendScreenFade(pPlayer, 40, 255, 60, 75, 0.35f, 0.1f, FFADE_IN);
            if (SpawnMonstersAroundPlayer(pPlayer, "monster_alien_slave", 1, human_hull, false) == 0) {
                SpawnMonstersAroundPlayer(pPlayer, "monster_houndeye", 1, human_hull, false);
            }
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_NORM);
            break;

        case 6: // HECU MARINE DROP
            if (SpawnMonstersAroundPlayer(pPlayer, "monster_human_grunt", 1, human_hull, false) == 0) {
                SpawnMonstersAroundPlayer(pPlayer, "monster_snark", 3, head_hull, false);
            }
            PlayCueSound(pPlayer, "buttons/button10.wav", PITCH_NORM);
            break;

        case 7: // ALIEN GRUNT HORNETS
            if (SpawnMonstersAroundPlayer(pPlayer, "monster_alien_grunt", 1, human_hull, false) == 0) {
                SpawnMonstersAroundPlayer(pPlayer, "monster_alien_slave", 1, human_hull, false);
            }
            PlayCueSound(pPlayer, "debris/beamstart7.wav", PITCH_LOW);
            break;

        case 8: // SNARK PARTY
            SpawnMonstersAroundPlayer(pPlayer, "monster_snark", 4, head_hull, false);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;

        case 9: // ROACH INFESTATION
            SpawnMonstersAroundPlayer(pPlayer, "monster_cockroach", 10, head_hull, true);
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
            break;

        case 10: // BARNEY BACKUP
            SpawnMonstersAroundPlayer(pPlayer, "monster_barney", 1, human_hull, true);
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_NORM);
            break;

        case 11: // PANICKED SCIENTISTS
            SpawnMonstersAroundPlayer(pPlayer, "monster_scientist", 3, human_hull, true);
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_NORM);
            break;

        case 12: // MOON GRAVITY
            m_flGravityEnd = now + ev.durationSec;
            m_flTargetGravity = 160.0f;
            m_szGravityLabel = "MOON GRAV";
            m_flFallProtectEnd = now + ev.durationSec + 4.5f;
            if (g_engfuncs.pfnCVarSetFloat) g_engfuncs.pfnCVarSetFloat("sv_gravity", 160.0f);
            pPlayer->v.velocity.z += 180.0f;
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
            break;

        case 13: // JUPITER GRAVITY
            m_flGravityEnd = now + ev.durationSec;
            m_flTargetGravity = 1600.0f;
            m_szGravityLabel = "HEAVY GRAV";
            if (g_engfuncs.pfnCVarSetFloat) g_engfuncs.pfnCVarSetFloat("sv_gravity", 1600.0f);
            SendScreenShake(pPlayer, 6.0f, 40.0f, 1.0f);
            PlayCueSound(pPlayer, "items/suitchargeno1.wav", PITCH_LOW);
            break;

        case 14: // TURBO FREEMAN
            m_flSpeedEnd = now + ev.durationSec;
            m_flTargetSpeed = 720.0f;
            m_szSpeedLabel = "TURBO SPEED";
            if (g_engfuncs.pfnCVarSetFloat) {
                g_engfuncs.pfnCVarSetFloat("sv_maxspeed", 720.0f);
                g_engfuncs.pfnCVarSetFloat("sv_accelerate", 22.0f);
            }
            if (g_engfuncs.pfnSetClientMaxspeed) {
                g_engfuncs.pfnSetClientMaxspeed(pPlayer, 720.0f);
            }
            SendScreenFade(pPlayer, 60, 220, 255, 50, 0.3f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_HIGH);
            break;

        case 15: // SNAIL PACE
            m_flSpeedEnd = now + ev.durationSec;
            m_flTargetSpeed = 105.0f;
            m_szSpeedLabel = "SNAIL PACE";
            if (g_engfuncs.pfnCVarSetFloat) g_engfuncs.pfnCVarSetFloat("sv_maxspeed", 105.0f);
            if (g_engfuncs.pfnSetClientMaxspeed) g_engfuncs.pfnSetClientMaxspeed(pPlayer, 105.0f);
            PlayCueSound(pPlayer, "items/suitchargeno1.wav", PITCH_LOW);
            break;

        case 16: // ICE SKATING RINK
            m_flFrictionEnd = now + ev.durationSec;
            m_flTargetFriction = 0.32f;
            m_flTargetAirAccelerate = 80.0f;
            if (g_engfuncs.pfnCVarSetFloat) {
                g_engfuncs.pfnCVarSetFloat("sv_friction", 0.32f);
                g_engfuncs.pfnCVarSetFloat("sv_airaccelerate", 80.0f);
            }
            SendScreenFade(pPlayer, 140, 220, 255, 55, 0.4f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
            break;

        case 17: // SUPER JUMP BOOTS
            m_flSuperJumpEnd = now + ev.durationSec;
            m_flFallProtectEnd = now + ev.durationSec + 4.5f;
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_NORM);
            break;

        case 18: // POGO STICK MODE
            m_flPogoEnd = now + ev.durationSec;
            m_flNextPogoTick = now + 0.1f;
            m_flFallProtectEnd = now + ev.durationSec + 4.0f;
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_HIGH);
            break;

        case 19: // SKY LAUNCHPAD
        {
            m_flFallProtectEnd = now + 6.5f;
            pPlayer->v.flags &= ~FL_ONGROUND;
            pPlayer->v.origin.z += 2.0f;
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
            if (bestIdx >= 0 && g_engfuncs.pfnTraceHull && g_engfuncs.pfnSetOrigin) {
                Vec3 dest = m_breadcrumbs[bestIdx].origin;
                int hull = (pPlayer->v.flags & FL_DUCKING) ? head_hull : human_hull;
                TraceResult tr;
                memset((void *)&tr, 0, sizeof(tr));
                g_engfuncs.pfnTraceHull(dest, dest, 0, hull, pPlayer, &tr);
                if (!tr.fAllSolid && !tr.fStartSolid) {
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
        {
            if (g_engfuncs.pfnTraceHull && g_engfuncs.pfnSetOrigin) {
                float yawRad = pPlayer->v.v_angle.y * (3.14159265f / 180.0f);
                Vec3 start = pPlayer->v.origin;
                Vec3 target(
                    start.x + cosf(yawRad) * 240.0f,
                    start.y + sinf(yawRad) * 240.0f,
                    start.z + 12.0f
                );
                int hull = (pPlayer->v.flags & FL_DUCKING) ? head_hull : human_hull;
                TraceResult tr;
                memset((void *)&tr, 0, sizeof(tr));
                g_engfuncs.pfnTraceHull(start, target, 0, hull, pPlayer, &tr);
                Vec3 safeEnd = start + (target - start) * (tr.flFraction * 0.85f);

                TraceResult check;
                memset((void *)&check, 0, sizeof(check));
                g_engfuncs.pfnTraceHull(safeEnd, safeEnd, 0, hull, pPlayer, &check);
                if (!check.fAllSolid && !check.fStartSolid) {
                    SendTeleportSplash(start);
                    g_engfuncs.pfnSetOrigin(pPlayer, safeEnd);
                    SendTeleportSplash(safeEnd);
                }
            }
            SendScreenFade(pPlayer, 80, 220, 255, 80, 0.3f, 0.05f, FFADE_IN);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;
        }

        case 22: // RANDOM WEAPON GIFT
        {
            struct WeaponBundle { const char *wpn; const char *ammo1; const char *ammo2; };
            static const WeaponBundle kBundles[] = {
                { "weapon_357",       "ammo_357",       "ammo_357" },
                { "weapon_shotgun",   "ammo_buckshot",  "ammo_buckshot" },
                { "weapon_9mmAR",     "ammo_9mmAR",     "ammo_ARgrenades" },
                { "weapon_crossbow",  "ammo_crossbow",  "ammo_crossbow" },
                { "weapon_rpg",       "ammo_rpgclip",   "ammo_rpgclip" },
                { "weapon_gauss",     "ammo_gaussclip", "ammo_gaussclip" },
                { "weapon_egon",      "ammo_gaussclip", "ammo_gaussclip" },
                { "weapon_hornetgun", nullptr,          nullptr }
            };
            const WeaponBundle &b = kBundles[NextXorShift(m_rngState) % (sizeof(kBundles) / sizeof(kBundles[0]))];
            GiveItemToPlayer(pPlayer, b.wpn);
            if (b.ammo1) GiveItemToPlayer(pPlayer, b.ammo1);
            if (b.ammo2) GiveItemToPlayer(pPlayer, b.ammo2);
            PlayCueSound(pPlayer, "items/gunpickup2.wav", PITCH_NORM);
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
                GiveItemToPlayer(pPlayer, kAllGear[i]);
            }
            SendScreenFade(pPlayer, 80, 255, 100, 65, 0.4f, 0.1f, FFADE_IN);
            PlayCueSound(pPlayer, "items/gunpickup2.wav", PITCH_HIGH);
            break;
        }

        case 24: // CROWBAR ONLY
            StripPlayerWeapons(pPlayer);
            GiveItemToPlayer(pPlayer, "weapon_crowbar");
            PlayCueSound(pPlayer, "items/weapondrop1.wav", PITCH_NORM);
            break;

        case 25: // DEMOMAN LOADOUT
            StripPlayerWeapons(pPlayer);
            GiveItemToPlayer(pPlayer, "weapon_crowbar");
            GiveItemToPlayer(pPlayer, "weapon_rpg");
            GiveItemToPlayer(pPlayer, "ammo_rpgclip");
            GiveItemToPlayer(pPlayer, "ammo_rpgclip");
            GiveItemToPlayer(pPlayer, "weapon_handgrenade");
            GiveItemToPlayer(pPlayer, "weapon_handgrenade");
            GiveItemToPlayer(pPlayer, "weapon_satchel");
            GiveItemToPlayer(pPlayer, "weapon_tripmine");
            GiveItemToPlayer(pPlayer, "weapon_snark");
            PlayCueSound(pPlayer, "items/gunpickup2.wav", PITCH_LOW);
            break;

        case 26: // SUPPLY DROP SHOWER
            GiveItemToPlayer(pPlayer, "item_healthkit");
            GiveItemToPlayer(pPlayer, "item_healthkit");
            GiveItemToPlayer(pPlayer, "item_battery");
            GiveItemToPlayer(pPlayer, "item_battery");
            GiveItemToPlayer(pPlayer, "ammo_9mmAR");
            GiveItemToPlayer(pPlayer, "ammo_buckshot");
            GiveItemToPlayer(pPlayer, "ammo_357");
            GiveItemToPlayer(pPlayer, "ammo_gaussclip");
            PlayCueSound(pPlayer, "items/medshot4.wav", PITCH_NORM);
            break;

        case 27: // ANOMALY BOMB (3S FUSE) - non-lethal knockback shockwave
            m_bAnomalyArmed = true;
            m_vecAnomalyOrigin = pPlayer->v.origin;
            m_flAnomalyDetonateTime = now + 3.0f;
            m_flNextAnomalyBeep = now + 0.1f;
            SendImplosion(m_vecAnomalyOrigin, 140, 32, 25);
            PlayCueSound(pPlayer, "buttons/button10.wav", PITCH_HIGH);
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

        case 31: // HP <-> ARMOR SWAP (guaranteed >= 10 HP)
        {
            float oldHp = pPlayer->v.health;
            float oldAp = pPlayer->v.armorvalue;
            float newHp = (oldAp >= 10.0f) ? oldAp : 10.0f;
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
            m_flGodmodeEnd = now + ev.durationSec;
            pPlayer->v.flags |= FL_GODMODE;
            pPlayer->v.renderfx = kRenderFxGlowShell;
            pPlayer->v.rendercolor = Vec3(255.0f, 215.0f, 40.0f);
            pPlayer->v.renderamt = 30.0f;
            SendScreenFade(pPlayer, 255, 220, 60, 70, 0.4f, 0.15f, FFADE_IN);
            PlayCueSound(pPlayer, "items/suitchargeok1.wav", PITCH_HIGH);
            break;

        case 33: // QUAKE PRO FOV (130)
            m_flFovEnd = now + ev.durationSec;
            m_iTargetFov = 130;
            m_szFovLabel = "FOV 130";
            SendSetFOV(pPlayer, 130);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;

        case 34: // SNIPER ZOOM (FOV 38)
            m_flFovEnd = now + ev.durationSec;
            m_iTargetFov = 38;
            m_szFovLabel = "ZOOM FOV";
            SendSetFOV(pPlayer, 38);
            PlayCueSound(pPlayer, "buttons/button3.wav", PITCH_LOW);
            break;

        case 35: // BARNEY'S BEER (DRUNK CAM)
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
        default:
            m_flDiscoEnd = now + ev.durationSec;
            m_flNextDiscoTick = now + 0.05f;
            m_discoPhase = 0;
            LaunchNearbyEnemies(pPlayer, 900.0f, 460.0f);
            PlayCueSound(pPlayer, "debris/zap4.wav", PITCH_HIGH);
            break;
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

    // 1. Gravity modifier
    if (m_flGravityEnd > 0.0f) {
        if (now >= m_flGravityEnd) {
            m_flGravityEnd = 0.0f;
            m_flTargetGravity = 800.0f;
            m_szGravityLabel = nullptr;
            if (g_engfuncs.pfnCVarSetFloat) g_engfuncs.pfnCVarSetFloat("sv_gravity", 800.0f);
        } else if (g_engfuncs.pfnCVarGetFloat && fabsf(g_engfuncs.pfnCVarGetFloat("sv_gravity") - m_flTargetGravity) > 1.0f) {
            g_engfuncs.pfnCVarSetFloat("sv_gravity", m_flTargetGravity);
        }
    }

    // 2. Speed modifier (weapon switches in HL1 reset client maxspeed, so keep synced!)
    if (m_flSpeedEnd > 0.0f) {
        if (now >= m_flSpeedEnd) {
            m_flSpeedEnd = 0.0f;
            m_flTargetSpeed = 320.0f;
            m_szSpeedLabel = nullptr;
            if (g_engfuncs.pfnCVarSetFloat) {
                g_engfuncs.pfnCVarSetFloat("sv_maxspeed", 320.0f);
                g_engfuncs.pfnCVarSetFloat("sv_accelerate", 10.0f);
            }
            if (g_engfuncs.pfnSetClientMaxspeed) {
                g_engfuncs.pfnSetClientMaxspeed(pPlayer, 320.0f);
            }
            pPlayer->v.maxspeed = 320.0f;
        } else {
            pPlayer->v.maxspeed = m_flTargetSpeed;
            if (g_engfuncs.pfnSetClientMaxspeed) {
                g_engfuncs.pfnSetClientMaxspeed(pPlayer, m_flTargetSpeed);
            }
            // Extra horizontal stride boost for Turbo Freeman when moving on ground
            if (m_flTargetSpeed > 500.0f && (pPlayer->v.flags & FL_ONGROUND)) {
                float speed2D = pPlayer->v.velocity.Length2D();
                if (speed2D > 160.0f && speed2D < 640.0f) {
                    pPlayer->v.velocity.x *= 1.06f;
                    pPlayer->v.velocity.y *= 1.06f;
                }
            }
        }
    }

    // 3. Ice friction modifier
    if (m_flFrictionEnd > 0.0f && now >= m_flFrictionEnd) {
        m_flFrictionEnd = 0.0f;
        if (g_engfuncs.pfnCVarSetFloat) {
            g_engfuncs.pfnCVarSetFloat("sv_friction", 4.0f);
            g_engfuncs.pfnCVarSetFloat("sv_airaccelerate", 10.0f);
        }
    }

    // 4. FOV modifier
    if (m_flFovEnd > 0.0f) {
        if (now >= m_flFovEnd) {
            m_flFovEnd = 0.0f;
            m_iTargetFov = 0;
            m_szFovLabel = nullptr;
            SendSetFOV(pPlayer, 0);
        } else if ((int)pPlayer->v.fov != m_iTargetFov) {
            SendSetFOV(pPlayer, m_iTargetFov);
        }
    }

    // 5. Godmode modifier
    if (m_flGodmodeEnd > 0.0f) {
        if (now >= m_flGodmodeEnd) {
            m_flGodmodeEnd = 0.0f;
            pPlayer->v.flags &= ~FL_GODMODE;
            pPlayer->v.renderfx = kRenderFxNone;
            pPlayer->v.renderamt = 0.0f;
        } else {
            pPlayer->v.flags |= FL_GODMODE;
        }
    }

    // 6. Fall-damage protection window
    if (m_flFallProtectEnd > 0.0f) {
        if (now >= m_flFallProtectEnd) {
            m_flFallProtectEnd = 0.0f;
        } else if (pPlayer->v.flFallVelocity > 440.0f) {
            pPlayer->v.flFallVelocity = 440.0f;
        }
    }

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
            pPlayer->v.origin.z += 2.0f;
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
            pPlayer->v.punchangle = Vec3(0.0f, 0.0f, 0.0f);
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

            // Launch nearby enemies
            LaunchNearbyEnemies(pPlayer, 450.0f, 480.0f);

            // Push player away if still standing close (never lethal!)
            Vec3 delta = pPlayer->v.origin - m_vecAnomalyOrigin;
            float dist = delta.Length();
            if (dist < 320.0f) {
                m_flFallProtectEnd = now + 4.5f;
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
    float interval = (g_engfuncs.pfnCVarGetFloat) ? g_engfuncs.pfnCVarGetFloat("chaos_interval") : 10.0f;
    if (interval < 2.0f) interval = 2.0f;
    if (interval > 300.0f) interval = 300.0f;

    // Detect save-load time jumps or interval reductions so timer never freezes
    if (now < m_flLastFrameTime - 0.1f || (m_flNextChaosTime - now) > interval + 1.5f) {
        ClearAllModifiers(FindActivePlayer(), true);
        m_flNextChaosTime = now + interval;
        m_flNextHudRefresh = now + 0.1f;
        m_lastBeepSecond = -1;
    }
    m_flLastFrameTime = now;

    edict_t *pPlayer = FindActivePlayer();
    if (!pPlayer) return;

    // Death check (persists immediately before player clicks to reload autosave!)
    if (g_deathCounter.CheckPlayerState(pPlayer)) {
        ClearAllModifiers(pPlayer, true);
        ShowDeathBanner(pPlayer, g_deathCounter.GetTotalDeaths(), g_deathCounter.GetMapDeaths());
        return;
    }

    const bool isAlive = (pPlayer->v.deadflag == DEAD_NO && pPlayer->v.health > 0.0f);
    if (!isAlive) {
        // Hold chaos timer steady while player is on death screen
        m_flNextChaosTime = now + interval;
        return;
    }

    RecordBreadcrumb(pPlayer);
    UpdateTimedModifiers(pPlayer);

    const bool enabled = (!g_engfuncs.pfnCVarGetFloat || g_engfuncs.pfnCVarGetFloat("chaos_enabled") > 0.0f);
    if (enabled) {
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
    }

    if (now >= m_flNextHudRefresh) {
        m_flNextHudRefresh = now + 0.25f;
        RefreshStatusHud(pPlayer);
    }
}

void ChaosManager::OnPlayerPreThink(edict_t *pPlayer) {
    if (!pPlayer || pPlayer->free) return;
    if (g_deathCounter.CheckPlayerState(pPlayer)) {
        ClearAllModifiers(pPlayer, true);
        ShowDeathBanner(pPlayer, g_deathCounter.GetTotalDeaths(), g_deathCounter.GetMapDeaths());
        return;
    }
    if (m_flFallProtectEnd > 0.0f && gpGlobals && gpGlobals->time < m_flFallProtectEnd) {
        if (pPlayer->v.flFallVelocity > 440.0f) {
            pPlayer->v.flFallVelocity = 440.0f;
        }
    }
}

void ChaosManager::OnPlayerPostThink(edict_t *pPlayer) {
    if (!pPlayer || pPlayer->free) return;
    if (m_flFovEnd > 0.0f && gpGlobals && gpGlobals->time < m_flFovEnd && pPlayer->v.deadflag == DEAD_NO) {
        pPlayer->v.fov = (float)m_iTargetFov;
    }
    if (g_deathCounter.CheckPlayerState(pPlayer)) {
        ClearAllModifiers(pPlayer, true);
        ShowDeathBanner(pPlayer, g_deathCounter.GetTotalDeaths(), g_deathCounter.GetMapDeaths());
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
            CmdTriggerById(atoi(g_engfuncs.pfnCmd_Argv(1)));
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
        CmdPrintList(pPlayer);
        return true;
    }
    if (StrCaseEq(cmd, "chaos_status")) {
        CmdPrintStatus(pPlayer);
        return true;
    }
    return false;
}

void ChaosManager::CmdTriggerRandom() {
    edict_t *pPlayer = FindActivePlayer();
    if (!pPlayer || pPlayer->v.deadflag != DEAD_NO || pPlayer->v.health <= 0.0f) return;

    float interval = (g_engfuncs.pfnCVarGetFloat) ? g_engfuncs.pfnCVarGetFloat("chaos_interval") : 10.0f;
    if (interval < 2.0f) interval = 10.0f;
    if (gpGlobals) m_flNextChaosTime = gpGlobals->time + interval;
    m_lastBeepSecond = -1;

    int evId = PickNextEventId(pPlayer);
    ExecuteEvent(pPlayer, evId);
}

void ChaosManager::CmdTriggerById(int eventId) {
    edict_t *pPlayer = FindActivePlayer();
    if (!pPlayer || pPlayer->v.deadflag != DEAD_NO || pPlayer->v.health <= 0.0f) return;

    if (eventId < 0 || eventId >= CHAOS_EVENT_COUNT) {
        SendChatAndConsole(pPlayer, "[ХАОС] Укажите ID события от 0 до 37 (список: chaos_list)\n");
        return;
    }

    float interval = (g_engfuncs.pfnCVarGetFloat) ? g_engfuncs.pfnCVarGetFloat("chaos_interval") : 10.0f;
    if (interval < 2.0f) interval = 10.0f;
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

void ChaosManager::CmdPrintList(edict_t *pPlayer) {
    if (!pPlayer) pPlayer = FindActivePlayer();
    auto printLine = [&](const char *line) {
        if (pPlayer && g_engfuncs.pfnClientPrintf) {
            g_engfuncs.pfnClientPrintf(pPlayer, print_console, line);
        } else if (g_engfuncs.pfnServerPrint) {
            g_engfuncs.pfnServerPrint(line);
        }
    };

    printLine("=== СПИСОК СОБЫТИЙ HL1 RANDOMIZER CHAOS (0..37) ===\n");
    char line[256];
    for (int i = 0; i < CHAOS_EVENT_COUNT; ++i) {
        snprintf(line, sizeof(line), " [%2d] %-26s | %s\n",
                 g_eventDefs[i].id, g_eventDefs[i].titleEn, g_eventDefs[i].titleRu);
        printLine(line);
    }
    printLine("Вызов любого события вручную: chaos_trigger <ID>\n");
}

void ChaosManager::CmdPrintStatus(edict_t *pPlayer) {
    if (!pPlayer) pPlayer = FindActivePlayer();
    char buf[256];
    snprintf(buf, sizeof(buf),
             "[ХАОС СТАТИСТИКА] Смертей всего: %d | На этой карте: %d | Пережито событий: %d | До след. события: %.1f сек\n",
             g_deathCounter.GetTotalDeaths(),
             g_deathCounter.GetMapDeaths(),
             g_deathCounter.GetTotalEvents(),
             GetTimeRemaining());
    SendChatAndConsole(pPlayer, buf);
}
