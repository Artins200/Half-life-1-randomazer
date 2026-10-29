#ifndef CHAOS_TYPES_H
#define CHAOS_TYPES_H

#include "hlsdk_mini.h"

enum ChaosCategory {
    CHAOS_CAT_MONSTERS = 0,
    CHAOS_CAT_PHYSICS  = 1,
    CHAOS_CAT_WEAPONS  = 2,
    CHAOS_CAT_SURVIVAL = 3,
    CHAOS_CAT_VISUAL   = 4
};

enum ChaosTone {
    CHAOS_TONE_GOOD    = 0, // Green HUD banner
    CHAOS_TONE_NEUTRAL = 1, // Cyan HUD banner
    CHAOS_TONE_WEIRD   = 2, // Magenta/Purple HUD banner
    CHAOS_TONE_DANGER  = 3  // Red-Orange HUD banner
};

struct ChaosEventDef {
    int           id;
    const char   *titleEn;       // Pure ASCII for GoldSrc TE_TEXTMESSAGE sprite font
    const char   *titleRu;       // UTF-8 Russian for SayText chat & console
    const char   *descRu;        // UTF-8 Russian short explanation
    ChaosCategory category;
    ChaosTone     tone;
    float         durationSec;   // 0.0f for instant events, >0.0f for timed effects
    bool          requiresSuit;  // Inventory changes wait until the player has HEV.
    bool          skipOnTram;    // During tram/cutscenes, pick only non-disruptive effects.
};

struct PlayerBreadcrumb {
    Vec3  origin;
    Vec3  angles;
    float timestamp;
    bool  valid;
};

#endif // CHAOS_TYPES_H
