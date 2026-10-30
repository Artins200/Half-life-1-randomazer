#include "chaos_events.h"
#include "chaos_features.h"
#include "death_counter.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

// Abilities use the active original game DLL and its already-prepared effects.
// Optional Gearbox weapons require the user's installed Opposing Force files.
static bool SafePlayerContents(const Vec3 &origin, int hull) {
    if (!g_engfuncs.pfnPointContents || g_engfuncs.pfnPointContents(origin) != CONTENTS_EMPTY) return false;
    // The hull centre can be above lava/slime/water while the player's feet
    // are already inside it. Sample near the bottom as well.
    Vec3 nearFeet = origin - Vec3(0, 0, hull == head_hull ? 14.0f : 30.0f);
    return g_engfuncs.pfnPointContents(nearFeet) == CONTENTS_EMPTY;
}

void ChaosManager::ResetTransientState() {
    m_restrictedContext = false;
    m_totemArmed = false;
    ClearItemQueue();
    m_portalEnabled = false;
    m_portals[0].valid = m_portals[1].valid = false;
    m_nextPortalUse = 0.0f;
    m_nextPortalVisual = 0.0f;
    m_portalUseHeld = false;
    m_speedrunActive = false;
    m_csAnimeFrame = -1;
    m_nextCsAnimeFrame = 0.0f;
    for (int i = 0; i < CHAOS_BREADCRUMB_COUNT; ++i) m_breadcrumbs[i].valid = false;
}

void ChaosManager::ActivateTotemIfNeeded(edict_t *player) {
    // The proxy cannot intercept CBasePlayer::TakeDamage. This is intentionally
    // a pre-death emergency shield, not an unsafe resurrection after PlayerKilled.
    if (!m_totemArmed || !gpGlobals || !IsEventAllowed(player, 38) || player->v.health > 25.0f) return;
    m_totemArmed = false;
    player->v.health = 60.0f;
    if (PlayerHasSuit(player) && player->v.armorvalue < 40.0f) player->v.armorvalue = 40.0f;
    BeginShield(player, 3.0f);
    m_flRegenEnd = gpGlobals->time + 6.0f;
    m_flNextRegenTick = gpGlobals->time + 1.0f;
    SendScreenFade(player, 255, 215, 60, 95, 0.5f, 0.3f, FFADE_IN);
    SendDynamicLight(player->v.origin, 20, 255, 210, 40, 8, 20);
    PlayCueSound(player, "items/suitchargeok1.wav", PITCH_HIGH);
    SendChatAndConsole(player, "[ХАОС] Тотем сработал до гибели: здоровье, броня и щит!\n");
}

void ChaosManager::CmdTotem() {
    edict_t *player = FindActivePlayer();
    if (!IsEventAllowed(player, 38)) {
        SendChatAndConsole(player, "[ХАОС] Тотем доступен в одиночной игре вне транспорта и вступительного трамвая.\n");
        return;
    }
    m_totemArmed = true;
    SendChatAndConsole(player, "[ХАОС] Тотем готов: срабатывает при 25 HP или ниже, пока игрок жив.\n");
    ActivateTotemIfNeeded(player);
}

void ChaosManager::CmdPortalGun() {
    edict_t *player = FindActivePlayer();
    if (!IsEventAllowed(player, 39)) {
        SendChatAndConsole(player, "[ХАОС] Порталы доступны в одиночной игре вне транспорта и вступительного трамвая.\n");
        return;
    }
    m_portalEnabled = true;
    SendChatAndConsole(player, "[ХАОС] Портальное устройство: chaos_portal_blue / chaos_portal_orange, затем E у точки.\n");
}

void ChaosManager::CmdPortalPlace(int slot) {
    edict_t *player = FindActivePlayer();
    if (!m_portalEnabled || !IsEventAllowed(player, 39)) {
        SendChatAndConsole(player, "[ХАОС] Сначала включите устройство: chaos_portal_gun.\n");
        return;
    }
    PlacePortal(player, slot);
}

void ChaosManager::PlacePortal(edict_t *player, int slot) {
    if (!player || slot < 0 || slot > 1 || !g_engfuncs.pfnMakeVectors ||
        !g_engfuncs.pfnTraceLine || !g_engfuncs.pfnTraceHull || !g_engfuncs.pfnPointContents) return;
    // Place only on nearby static world walls. Dynamic doors, monsters, sky,
    // dangerous liquids, and positions without a walkable hull are rejected.
    g_engfuncs.pfnMakeVectors(player->v.v_angle);
    const Vec3 start = player->v.origin + player->v.view_ofs;
    const Vec3 end = start + gpGlobals->v_forward * 768.0f;
    TraceResult wall = {};
    g_engfuncs.pfnTraceLine(start, end, 1, player, &wall);
    if (wall.fStartSolid || wall.fAllSolid || wall.flFraction <= 0.0f || wall.flFraction >= 1.0f ||
        fabsf(wall.vecPlaneNormal.z) > 0.3f ||
        (wall.pHit && (!g_engfuncs.pfnIndexOfEdict || g_engfuncs.pfnIndexOfEdict(wall.pHit) != 0))) {
        SendChatAndConsole(player, "[ХАОС] Нужна близкая неподвижная стена.\n");
        return;
    }
    Vec3 spot = wall.vecEndPos + wall.vecPlaneNormal * 38.0f;
    spot.z = player->v.origin.z;
    const int hull = (player->v.flags & FL_DUCKING) ? head_hull : human_hull;
    TraceResult path = {};
    g_engfuncs.pfnTraceHull(player->v.origin, spot, 0, hull, player, &path);
    if (path.fStartSolid || path.fAllSolid || path.flFraction < 0.99f) {
        SendChatAndConsole(player, "[ХАОС] К точке нет безопасного пути.\n");
        return;
    }
    TraceResult floor = {};
    Vec3 below = spot - Vec3(0, 0, 72);
    g_engfuncs.pfnTraceHull(spot, below, 0, hull, player, &floor);
    if (floor.fStartSolid || floor.fAllSolid || floor.flFraction >= 1.0f) {
        SendChatAndConsole(player, "[ХАОС] У стены нет безопасного пола.\n");
        return;
    }
    spot = floor.vecEndPos + Vec3(0, 0, 1);
    TraceResult check = {};
    g_engfuncs.pfnTraceHull(spot, spot, 0, hull, player, &check);
    if (check.fStartSolid || check.fAllSolid || !SafePlayerContents(spot, hull)) {
        SendChatAndConsole(player, "[ХАОС] Недостаточно места для портала.\n");
        return;
    }
    m_portals[slot].origin = spot;
    m_portals[slot].valid = true;
    SendDynamicLight(spot + Vec3(0, 0, 24), 16, slot ? 255 : 40, 130, slot ? 30 : 255, 10, 15);
    SendChatAndConsole(player, slot ? "[ХАОС] Оранжевая точка установлена.\n" : "[ХАОС] Синяя точка установлена.\n");
}

void ChaosManager::CmdPortalEnter() {
    edict_t *player = FindActivePlayer();
    if (player) EnterPortal(player);
}
void ChaosManager::EnterPortal(edict_t *player) {
    if (!gpGlobals || !m_portalEnabled || !m_portals[0].valid || !m_portals[1].valid ||
        !IsEventAllowed(player, 39) || gpGlobals->time < m_nextPortalUse ||
        !g_engfuncs.pfnTraceHull || !g_engfuncs.pfnSetOrigin || !g_engfuncs.pfnPointContents) return;
    float d0 = (player->v.origin - m_portals[0].origin).Length();
    float d1 = (player->v.origin - m_portals[1].origin).Length();
    if (d0 > 64.0f && d1 > 64.0f) return;
    int destination = (d0 <= d1) ? 1 : 0;
    Vec3 spot = m_portals[destination].origin;
    int hull = (player->v.flags & FL_DUCKING) ? head_hull : human_hull;
    TraceResult check = {};
    g_engfuncs.pfnTraceHull(spot, spot, 0, hull, player, &check);
    TraceResult floor = {};
    Vec3 below = spot - Vec3(0, 0, 72);
    g_engfuncs.pfnTraceHull(spot, below, 0, hull, player, &floor);
    if (check.fStartSolid || check.fAllSolid || floor.fStartSolid || floor.fAllSolid ||
        floor.flFraction >= 1.0f || !SafePlayerContents(spot, hull)) {
        SendChatAndConsole(player, "[ХАОС] Выход занят или опасен, телепортация отменена.\n");
        return;
    }
    Vec3 source = player->v.origin;
    m_nextPortalUse = gpGlobals->time + 1.0f;
    SendTeleportSplash(source);
    g_engfuncs.pfnSetOrigin(player, spot);
    player->v.velocity = Vec3(0, 0, 0);
    player->v.flags &= ~FL_ONGROUND;
    SendTeleportSplash(spot);
}
void ChaosManager::UpdatePortalUse(edict_t *player) {
    if (!player || !gpGlobals || !m_portalEnabled) return;
    bool held = (player->v.button & IN_USE) != 0;
    if (held && !m_portalUseHeld) EnterPortal(player);
    m_portalUseHeld = held;
    if (gpGlobals->time >= m_nextPortalVisual) {
        m_nextPortalVisual = gpGlobals->time + 1.0f;
        for (int i = 0; i < 2; ++i) if (m_portals[i].valid)
            SendDynamicLight(m_portals[i].origin + Vec3(0, 0, 24), 12, i ? 255 : 40, 130, i ? 30 : 255, 11, 10);
    }
}

void ChaosManager::CmdSpeedrun() {
    edict_t *player = FindActivePlayer();
    if (!player || !gpGlobals) return;
    m_speedrunActive = !m_speedrunActive;
    if (m_speedrunActive) {
        m_speedrunStart = gpGlobals->time;
        SendChatAndConsole(player, "[ХАОС] Таймер спидрана запущен на текущей карте.\n");
    } else {
        float elapsed = gpGlobals->time - m_speedrunStart;
        if (elapsed < 0) elapsed = 0;
        char msg[128];
        snprintf(msg, sizeof(msg), "[ХАОС] Спидран остановлен: %.1f сек.\n", elapsed);
        SendChatAndConsole(player, msg);
    }
}

void ChaosManager::ShowNewGameTroll(edict_t *player) {
    if (!player) return;
    hudtextparms_t parms = {};
    parms.channel = 2;
    parms.x = -1.0f; parms.y = 0.20f;
    parms.r1 = 255; parms.g1 = 140; parms.b1 = 40; parms.a1 = 255;
    parms.r2 = 255; parms.g2 = 255; parms.b2 = 255; parms.a2 = 255;
    parms.fadeinTime = 0.1f; parms.holdTime = 5.0f; parms.fadeoutTime = 0.5f;
    SendHudText(player, parms, "NEW GAME...\nDELETING SAVES?\nJUST KIDDING! SAVES ARE SAFE.", true);
    SendChatAndConsole(player, "[ХАОС] Это шутка: мод не удаляет ваши сохранения.\n");
}

void ChaosManager::StartCsAnimeMeme(edict_t *player) {
    if (!player || !gpGlobals) return;
    m_csAnimeFrame = 0;
    m_nextCsAnimeFrame = gpGlobals->time;
    UpdateCsAnimeMeme(player);
}

void ChaosManager::UpdateCsAnimeMeme(edict_t *player) {
    if (!player || !gpGlobals || m_csAnimeFrame < 0 || gpGlobals->time < m_nextCsAnimeFrame) return;
    // Four lightweight, ASCII HUD frames. No downloaded models, sounds or
    // changes to the player's CS installation are involved.
    static const char *const frames[] = {
        "CS 1.6 UPDATE...\n[ DOWNLOADING ]",
        "ANIME IN CS 1.6?!\n( o_o )",
        "( ^_^ )\nRUSH B!",
        "JUST A MEME!\nNO FILES CHANGED."
    };
    const int count = (int)(sizeof(frames) / sizeof(frames[0]));
    if (m_csAnimeFrame >= count) { m_csAnimeFrame = -1; return; }
    hudtextparms_t parms = {};
    parms.channel = 4;
    parms.x = -1.0f; parms.y = 0.57f;
    parms.r1 = 255; parms.g1 = 150; parms.b1 = 210; parms.a1 = 255;
    parms.r2 = 255; parms.g2 = 255; parms.b2 = 255; parms.a2 = 255;
    parms.fadeinTime = 0.05f; parms.holdTime = 0.8f; parms.fadeoutTime = 0.1f;
    SendHudText(player, parms, frames[m_csAnimeFrame], false);
    m_nextCsAnimeFrame = gpGlobals->time + 0.65f;
    if (++m_csAnimeFrame >= count) m_csAnimeFrame = -1;
}

static bool IsDirectory(const struct stat &info) {
#ifdef _WIN32
    return (info.st_mode & _S_IFMT) == _S_IFDIR;
#else
    return S_ISDIR(info.st_mode);
#endif
}

ChaosSaveSlotResult ChaosFindUnusedSaveSlot(const char *gameDir, unsigned long timestamp,
                                           char *name, size_t nameSize) {
    if (!gameDir || !gameDir[0] || !name || nameSize == 0) return CHAOS_SAVE_SLOT_CHECK_FAILED;
    name[0] = '\0';
    // Windows reports ENOENT for a file used as a parent directory. Validate
    // both parents explicitly instead of treating that as a free save slot.
    struct stat gameInfo;
    if (stat(gameDir, &gameInfo) != 0 || !IsDirectory(gameInfo)) return CHAOS_SAVE_SLOT_CHECK_FAILED;
    const char *directories[] = {"SAVE", "save"};
    for (const char *folder : directories) {
        char path[400];
        int len = snprintf(path, sizeof(path), "%s/%s", gameDir, folder);
        if (len < 0 || (size_t)len >= sizeof(path)) return CHAOS_SAVE_SLOT_CHECK_FAILED;
        struct stat info;
        errno = 0;
        if (stat(path, &info) == 0) {
            if (!IsDirectory(info)) return CHAOS_SAVE_SLOT_CHECK_FAILED;
        } else {
            if (errno != ENOENT) return CHAOS_SAVE_SLOT_CHECK_FAILED;
#ifndef _WIN32
            // A dangling SAVE symlink is not an absent, safely creatable folder.
            if (lstat(path, &info) == 0 || errno != ENOENT) return CHAOS_SAVE_SLOT_CHECK_FAILED;
#endif
        }
    }
    const char *extensions[] = {"sav", "SAV"};
    // GoldSrc's SaveGameSlot can delete/age the <slot>01 backup, too.
    const char *suffixes[] = {"", "01"};
    for (int i = 0; i < 100; ++i) {
        int len = snprintf(name, nameSize, "chaos_c1a1_%lu_%02d", timestamp, i);
        if (len < 0 || (size_t)len >= nameSize) { name[0] = '\0'; return CHAOS_SAVE_SLOT_CHECK_FAILED; }
        bool used = false;
        for (const char *folder : directories) for (const char *extension : extensions) for (const char *suffix : suffixes) {
            char path[400];
            int pathLen = snprintf(path, sizeof(path), "%s/%s/%s%s.%s", gameDir, folder, name, suffix, extension);
            if (pathLen < 0 || (size_t)pathLen >= sizeof(path)) {
                name[0] = '\0';
                return CHAOS_SAVE_SLOT_CHECK_FAILED;
            }
            struct stat info;
            errno = 0;
#ifdef _WIN32
            int found = stat(path, &info);
#else
            // A dangling symlink is still an existing path and must not be
            // mistaken for an available save slot.
            int found = lstat(path, &info);
#endif
            if (found == 0) used = true;
            else if (errno != ENOENT) { name[0] = '\0'; return CHAOS_SAVE_SLOT_CHECK_FAILED; }
        }
        if (!used) return CHAOS_SAVE_SLOT_OK;
    }
    name[0] = '\0';
    return CHAOS_SAVE_SLOT_UNAVAILABLE;
}

void ChaosManager::UpdateCheckpoint(edict_t *player) {
    if (!m_checkpointPending || !player || !gpGlobals || m_checkpointAt <= 0 || gpGlobals->time < m_checkpointAt ||
        player->v.deadflag != DEAD_NO || player->v.health <= 0) return;
    if (!gpGlobals->pStringBase || gpGlobals->maxClients != 1 ||
        gpGlobals->deathmatch != 0.0f || gpGlobals->coop != 0.0f) {
        m_checkpointPending = false;
        return;
    }
    const char *map = gpGlobals->mapname ? STRING(gpGlobals->mapname) : "";
    if (strcmp(map, "c1a1") != 0) { m_checkpointPending = false; return; }
    if (!m_checkpointPrepared) {
        if (!PlayerHasSuit(player)) GiveItemToPlayer(player, "item_suit");
        if (PlayerHasSuit(player)) {
            GiveItemToPlayer(player, "weapon_crowbar");
            GiveItemToPlayer(player, "weapon_9mmhandgun");
            GiveItemToPlayer(player, "ammo_9mmclip");
        }
        m_checkpointPrepared = true;
        m_checkpointAt = gpGlobals->time + 1.0f;
        return;
    }
    m_checkpointPending = false;
    if (!g_engfuncs.pfnGetGameDir || !g_engfuncs.pfnServerCommand) {
        SendChatAndConsole(player, "[ХАОС] Не удалось создать сейв: сохраните вручную.\n");
        return;
    }
    char dir[260] = {0};
    g_engfuncs.pfnGetGameDir(dir);
    if (!dir[0]) { SendChatAndConsole(player, "[ХАОС] Не найден каталог сейвов. Сохраните вручную.\n"); return; }
    char name[64];
    ChaosSaveSlotResult result = ChaosFindUnusedSaveSlot(dir, (unsigned long)time(nullptr), name, sizeof(name));
    if (result == CHAOS_SAVE_SLOT_CHECK_FAILED) {
        SendChatAndConsole(player, "[ХАОС] Не удалось проверить существующие сейвы. Сохраните вручную.\n");
        return;
    }
    if (result != CHAOS_SAVE_SLOT_OK) {
        SendChatAndConsole(player, "[ХАОС] Свободное имя сейва не найдено. Сохраните вручную.\n");
        return;
    }
    char cmd[96], message[160];
    snprintf(cmd, sizeof(cmd), "save %s\n", name);
    snprintf(message, sizeof(message), "[ХАОС] Создаём сейв %s; проверьте список сохранений.\n", name);
    SendChatAndConsole(player, message);
    g_engfuncs.pfnServerCommand(cmd);
}

void ChaosManager::CmdC1A1(bool confirm) {
    edict_t *player = FindActivePlayer();
    if (!confirm) {
        SendChatAndConsole(player, "[ХАОС] Сначала сделайте отдельное ручное сохранение. Затем: chaos_c1a1_go\n");
        return;
    }
    if (!m_serverActive || !player || player->free || player->v.deadflag != DEAD_NO || player->v.health <= 0.0f ||
        !gpGlobals || gpGlobals->maxClients != 1 || gpGlobals->deathmatch != 0.0f || gpGlobals->coop != 0.0f ||
        !g_engfuncs.pfnIsMapValid || !g_engfuncs.pfnServerCommand || !g_engfuncs.pfnIsMapValid((char *)"c1a1")) {
        SendChatAndConsole(player, "[ХАОС] Карта c1a1 недоступна или команда запрещена в сетевой игре.\n");
        return;
    }
    m_checkpointPending = true;
    m_checkpointPrepared = false;
    m_checkpointAt = 0.0f;
    SendChatAndConsole(player, "[ХАОС] Загружаем c1a1; затем создадим новый именованный сейв.\n");
    g_engfuncs.pfnServerCommand((char *)"map c1a1\n");
}

void ChaosManager::ArmBomb(edict_t *player) {
    m_bAnomalyArmed = true;
    m_vecAnomalyOrigin = player->v.origin;
    m_flAnomalyDetonateTime = gpGlobals->time + 3.0f;
    m_flNextAnomalyBeep = gpGlobals->time + 0.1f;
    SendImplosion(m_vecAnomalyOrigin, 140, 32, 25);
    PlayCueSound(player, "buttons/button10.wav", PITCH_HIGH);
}

void ChaosManager::BlinkForward(edict_t *player) {
    if (!g_engfuncs.pfnTraceHull || !g_engfuncs.pfnSetOrigin || !g_engfuncs.pfnPointContents) return;
    float yaw = player->v.v_angle.y * (3.14159265f / 180.0f);
    Vec3 start = player->v.origin;
    Vec3 target = start + Vec3(cosf(yaw) * 240.0f, sinf(yaw) * 240.0f, 0.0f);
    int hull = (player->v.flags & FL_DUCKING) ? head_hull : human_hull;
    TraceResult path = {};
    g_engfuncs.pfnTraceHull(start, target, 0, hull, player, &path);
    if (path.fStartSolid || path.fAllSolid || path.flFraction <= 0.05f || path.flFraction > 1.0f) return;
    Vec3 spot = start + (target - start) * (path.flFraction * 0.85f);
    TraceResult check = {}, floor = {};
    g_engfuncs.pfnTraceHull(spot, spot, 0, hull, player, &check);
    Vec3 below = spot - Vec3(0, 0, 72);
    g_engfuncs.pfnTraceHull(spot, below, 0, hull, player, &floor);
    if (check.fAllSolid || check.fStartSolid || floor.fAllSolid || floor.fStartSolid || floor.flFraction >= 1.0f ||
        !SafePlayerContents(spot, hull)) return;
    SendTeleportSplash(start);
    g_engfuncs.pfnSetOrigin(player, spot);
    SendTeleportSplash(spot);
}

void ChaosManager::GiveOpforInspiredLoadout(edict_t *player) {
    // Prefer actual Gearbox classes only when the original DLL/map precached
    // them. Valve mode has no such classes, so it keeps the stock fallback.
    bool hasOpforWeapon = false;
    for (int i = 0; i < CHAOS_OPFOR_WEAPON_GIFT_COUNT; ++i) {
        const ChaosWeaponGiftDef *gift = ChaosGetWeaponGift(CHAOS_OPFOR_WEAPON_GIFT_START + i);
        if (!gift || !IsClassReady(gift->weapon)) continue;
        hasOpforWeapon = true;
        QueueItemToPlayer(player, gift->weapon);
        if (gift->ammo) QueueItemToPlayer(player, gift->ammo);
        if (gift->bonusAmmo) QueueItemToPlayer(player, gift->bonusAmmo);
    }
    if (hasOpforWeapon) return;
    QueueItemToPlayer(player, "weapon_9mmAR");
    QueueItemToPlayer(player, "ammo_9mmAR");
    QueueItemToPlayer(player, "weapon_shotgun");
    QueueItemToPlayer(player, "ammo_buckshot");
    QueueItemToPlayer(player, "weapon_gauss");
    QueueItemToPlayer(player, "ammo_gaussclip");
}

void ChaosManager::RunTheme(edict_t *player, int theme) {
    const float now = gpGlobals->time;
    switch (theme) {
        case 0: m_totemArmed = true; ActivateTotemIfNeeded(player); break;
        case 1:
            QueueItemToPlayer(player, "weapon_crowbar");
            if (player->v.armorvalue < 25) player->v.armorvalue = 25;
            SendDynamicLight(player->v.origin, 12, 50, 210, 255, 6, 12);
            break;
        case 2:
            ArmBomb(player);
            SendScreenFade(player, 50, 255, 70, 45, 0.25f, 0.1f, FFADE_IN);
            break;
        case 3: BlinkForward(player); break;
        case 4: BeginShield(player, 3); SendScreenFade(player, 180, 40, 220, 80, 0.4f, 0.2f, FFADE_IN); break;
        case 5:
            if (player->v.health < 100) player->v.health = 100;
            if (player->v.armorvalue < 80) player->v.armorvalue = 80;
            break;
        case 6:
            BeginSpeed(player, 580, 18, 7, "REDSTONE");
            SendDynamicLight(player->v.origin, 12, 255, 40, 30, 6, 12);
            break;
        case 10:
            BeginSpeed(player, 620, 18, 7, "RUSH B");
            SendScreenFade(player, 255, 255, 255, 55, 0.2f, 0.1f, FFADE_IN);
            break;
        case 7: BeginGravity(1200, 7, "BEDROCK"); break;
        case 8: QueueItemToPlayer(player, "weapon_crossbow"); QueueItemToPlayer(player, "ammo_crossbow"); break;
        case 9: if (player->v.health < 100) player->v.health = 100;
                SendScreenFade(player, 255, 220, 90, 55, 0.4f, 0.1f, FFADE_IN); break;
        case 11:
            BeginSpeed(player, 440, 14, 7, "CS B-HOP");
            m_flSuperJumpEnd = now + 7; ExtendFallProtection(12); break;
        case 12:
            QueueItemToPlayer(player, "weapon_crowbar");
            if (player->v.health < 80) player->v.health = 80;
            break;
        case 13: SendScreenFade(player, 255, 255, 255, 125, 0.35f, 0.15f, FFADE_IN); break;
        case 14: ArmBomb(player); break;
        case 15: BeginFov(player, 45, 7, "AWP ZOOM"); break;
        case 16:
            QueueItemToPlayer(player, "weapon_crowbar");
            BeginSpeed(player, 460, 12, 5, "KNIFE ROUND");
            break;
        case 17: BeginSpeed(player, 650, 20, 8, "SPEEDRUN");
                 m_flSuperJumpEnd = now + 8; ExtendFallProtection(13); break;
        case 18: m_flSuperJumpEnd = now + 7; ExtendFallProtection(12); break;
        case 19: BeginGravity(220, 8, "XEN HOP"); break;
        case 20: m_flRegenEnd = now + 8; m_flNextRegenTick = now + 0.2f; break;
        case 21: case 22:
            m_portalEnabled = true;
            // The two themes actually place different anchors, when the wall
            // and exit meet the same safety checks as the manual commands.
            PlacePortal(player, theme == 21 ? 0 : 1);
            break;
        case 23: GiveOpforInspiredLoadout(player); break;
    }
}

void ChaosManager::RunVariant(edict_t *player, int variant) {
    const float now = gpGlobals->time;
    switch (variant) {
        case 0: BeginGravity(160, 8, "MOON GRAV"); break;
        case 1: BeginGravity(1400, 7, "HEAVY GRAV"); break;
        case 2: BeginSpeed(player, 650, 20, 8, "TURBO"); break;
        case 3: BeginSpeed(player, 140, 10, 7, "SNAIL"); break;
        case 4: BeginIce(8); break;
        case 5: m_flSuperJumpEnd = now + 8; ExtendFallProtection(13); break;
        case 6: m_flRegenEnd = now + 8; m_flNextRegenTick = now + 0.2f; break;
        case 7: BeginShield(player, 6); break;
        case 8: BeginFov(player, 45, 8, "ZOOM"); break;
        case 9: BeginFov(player, 125, 8, "WIDE FOV"); break;
        case 10: if (m_flDrunkEnd <= 0) m_previousPunchAngle = player->v.punchangle;
                 m_flDrunkEnd = now + 8; break;
        case 11: m_flQuakeEnd = now + 7; m_flNextQuakeTick = now + 0.1f; break;
        case 12: m_flDiscoEnd = now + 8; m_flNextDiscoTick = now + 0.1f; m_discoPhase = 0; break;
        case 13: if (player->v.health < 100) player->v.health = 100; break;
        case 14: if (player->v.armorvalue < 100) player->v.armorvalue = 100; break;
        case 15:
            QueueItemToPlayer(player, "ammo_9mmAR"); QueueItemToPlayer(player, "ammo_buckshot");
            if (player->v.health < 60) player->v.health = 60;
            break;
        case 16: // New recipe variant: a fast bunnyhop with fall protection.
            BeginSpeed(player, 520, 16, 8, "B-HOP");
            m_flSuperJumpEnd = now + 8; ExtendFallProtection(13);
            break;
        case 17: // A short second chance rather than permanent invulnerability.
            if (player->v.health < 50) player->v.health = 50;
            if (player->v.armorvalue < 25) player->v.armorvalue = 25;
            BeginShield(player, 4);
            break;
    }
}
