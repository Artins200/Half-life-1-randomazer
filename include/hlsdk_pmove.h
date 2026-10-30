#ifndef HLSDK_PMOVE_MINI_H
#define HLSDK_PMOVE_MINI_H

#include "hlsdk_mini.h"

// ABI prefix of GoldSrc playermove_t through clientmaxspeed. The engine owns
// the larger struct; the proxy must never access fields beyond this prefix.
struct playermove_s {
    int player_index;
    qboolean server, multiplayer;
    float time, frametime;
    Vec3 forward, right, up;
    Vec3 origin, angles, oldangles, velocity, movedir, basevelocity;
    Vec3 view_ofs;
    float flDuckTime;
    qboolean bInDuck;
    int flTimeStepSound, iStepLeft;
    float flFallVelocity;
    Vec3 punchangle;
    float flSwimTime, flNextPrimaryAttack;
    int effects, flags, usehull;
    float gravity, friction;
    int oldbuttons;
    float waterjumptime;
    qboolean dead;
    int deadflag, spectator, movetype;
    int onground, waterlevel, watertype, oldwaterlevel;
    char sztexturename[256];
    char chtexturetype;
    float maxspeed, clientmaxspeed;
};

static_assert(offsetof(playermove_s, oldbuttons) == 200, "GoldSrc playermove oldbuttons offset mismatch");
static_assert(offsetof(playermove_s, flags) == 184, "GoldSrc playermove flags offset mismatch");
static_assert(offsetof(playermove_s, movetype) == 220, "GoldSrc playermove movetype offset mismatch");
static_assert(offsetof(playermove_s, maxspeed) == 500, "GoldSrc playermove maxspeed offset mismatch");
static_assert(offsetof(playermove_s, clientmaxspeed) == 504, "GoldSrc playermove clientmaxspeed offset mismatch");

#endif
