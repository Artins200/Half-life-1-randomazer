#ifndef HLSDK_MINI_H
#define HLSDK_MINI_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

typedef int qboolean;
typedef unsigned int string_t;
typedef float vec_t;
typedef unsigned char byte;
typedef uint32_t CRC32_t;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define INTERFACE_VERSION           140
#define NEW_DLL_FUNCTIONS_VERSION   1
#define SV_BLENDING_INTERFACE_VERSION 1
#define MAX_ENT_LEAFS               48

struct Vec3 {
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float ix, float iy, float iz) : x(ix), y(iy), z(iz) {}

    Vec3 operator+(const Vec3 &o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3 &o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 &operator+=(const Vec3 &o) { x += o.x; y += o.y; z += o.z; return *this; }

    float Length() const { return sqrtf(x * x + y * y + z * z); }
    float Length2D() const { return sqrtf(x * x + y * y); }
    operator float *() { return &x; }
    operator const float *() const { return &x; }
};

typedef Vec3 vec3_t;

typedef struct link_s {
    struct link_s *prev;
    struct link_s *next;
} link_t;

typedef struct edict_s edict_t;

typedef struct cvar_s {
    const char *name;
    const char *string;
    int flags;
    float value;
    struct cvar_s *next;
} cvar_t;

#define FCVAR_ARCHIVE   (1 << 0)
#define FCVAR_SERVER    (1 << 2)
#define FCVAR_EXTDLL    (1 << 3)

typedef enum {
    at_notice = 0,
    at_console,
    at_aiconsole,
    at_warning,
    at_error,
    at_logged
} ALERT_TYPE;

typedef enum {
    print_console = 0,
    print_center,
    print_chat
} PRINT_TYPE;

typedef enum {
    force_exactfile = 0,
    force_model_samebounds,
    force_model_specifybounds,
    force_model_specifybounds_if_avail
} FORCE_TYPE;

typedef struct {
    float       time;
    float       frametime;
    float       force_retouch;
    string_t    mapname;
    string_t    startspot;
    float       deathmatch;
    float       coop;
    float       teamplay;
    float       serverflags;
    float       found_secrets;
    vec3_t      v_forward;
    vec3_t      v_up;
    vec3_t      v_right;
    float       trace_allsolid;
    float       trace_startsolid;
    float       trace_fraction;
    vec3_t      trace_endpos;
    vec3_t      trace_plane_normal;
    float       trace_plane_dist;
    edict_t    *trace_ent;
    float       trace_inopen;
    float       trace_inwater;
    int         trace_hitgroup;
    int         trace_flags;
    int         msg_entity;
    int         cdAudioTrack;
    int         maxClients;
    int         maxEntities;
    const char *pStringBase;
    void       *pSaveData;
    vec3_t      vecLandmarkOffset;
} globalvars_t;

typedef struct entvars_s {
    string_t    classname;
    string_t    globalname;

    vec3_t      origin;
    vec3_t      oldorigin;
    vec3_t      velocity;
    vec3_t      basevelocity;
    vec3_t      clbasevelocity;
    vec3_t      movedir;

    vec3_t      angles;
    vec3_t      avelocity;
    vec3_t      punchangle;
    vec3_t      v_angle;

    vec3_t      endpos;
    vec3_t      startpos;
    float       impacttime;
    float       starttime;

    int         fixangle;
    float       idealpitch;
    float       pitch_speed;
    float       ideal_yaw;
    float       yaw_speed;

    int         modelindex;
    string_t    model;

    int         viewmodel;
    int         weaponmodel;

    vec3_t      absmin;
    vec3_t      absmax;
    vec3_t      mins;
    vec3_t      maxs;
    vec3_t      size;

    float       ltime;
    float       nextthink;

    int         movetype;
    int         solid;

    int         skin;
    int         body;
    int         effects;

    float       gravity;
    float       friction;

    int         light_level;

    int         sequence;
    int         gaitsequence;
    float       frame;
    float       animtime;
    float       framerate;
    byte        controller[4];
    byte        blending[2];

    float       scale;

    int         rendermode;
    float       renderamt;
    vec3_t      rendercolor;
    int         renderfx;

    float       health;
    float       frags;
    int         weapons;
    float       takedamage;

    int         deadflag;
    vec3_t      view_ofs;

    int         button;
    int         impulse;

    edict_t    *chain;
    edict_t    *dmg_inflictor;
    edict_t    *enemy;
    edict_t    *aiment;
    edict_t    *owner;
    edict_t    *groundentity;

    int         spawnflags;
    int         flags;

    int         colormap;
    int         team;

    float       max_health;
    float       teleport_time;
    float       armortype;
    float       armorvalue;
    int         waterlevel;
    int         watertype;

    string_t    target;
    string_t    targetname;
    string_t    netname;
    string_t    message;

    float       dmg_take;
    float       dmg_save;
    float       dmg;
    float       dmgtime;

    string_t    noise;
    string_t    noise1;
    string_t    noise2;
    string_t    noise3;

    float       speed;
    float       air_finished;
    float       pain_finished;
    float       radsuit_finished;

    edict_t    *pContainingEntity;

    int         playerclass;
    float       maxspeed;

    float       fov;
    int         weaponanim;

    int         pushmsec;

    int         bInDuck;
    int         flTimeStepSound;
    int         flSwimTime;
    int         flDuckTime;
    int         iStepLeft;
    float       flFallVelocity;

    int         gamestate;
    int         oldbuttons;
    int         groupinfo;

    int         iuser1;
    int         iuser2;
    int         iuser3;
    int         iuser4;
    float       fuser1;
    float       fuser2;
    float       fuser3;
    float       fuser4;
    vec3_t      vuser1;
    vec3_t      vuser2;
    vec3_t      vuser3;
    vec3_t      vuser4;
    edict_t    *euser1;
    edict_t    *euser2;
    edict_t    *euser3;
    edict_t    *euser4;
} entvars_t;

struct edict_s {
    qboolean    free;
    int         serialnumber;
    link_t      area;

    int         headnode;
    int         num_leafs;
    short       leafnums[MAX_ENT_LEAFS];

    float       freetime;
    void       *pvPrivateData;
    entvars_t   v;
};

typedef struct {
    int     fAllSolid;
    int     fStartSolid;
    int     fInOpen;
    int     fInWater;
    float   flFraction;
    vec3_t  vecEndPos;
    float   flPlaneDist;
    vec3_t  vecPlaneNormal;
    edict_t *pHit;
    int     iHitgroup;
} TraceResult;

typedef struct KeyValueData_s {
    char   *szClassName;
    char   *szKeyName;
    char   *szValue;
    int32_t fHandled;
} KeyValueData;

typedef struct hudtextparms_s {
    float       x;
    float       y;
    int         effect;
    byte        r1, g1, b1, a1;
    byte        r2, g2, b2, a2;
    float       fadeinTime;
    float       fadeoutTime;
    float       holdTime;
    float       fxTime;
    int         channel;
} hudtextparms_t;

struct saverestore_s;
typedef struct saverestore_s SAVERESTOREDATA;
struct TYPEDESCRIPTION_s;
struct customization_s;
struct playermove_s;
struct clientdata_s;
struct entity_state_s;
struct weapon_data_s;
struct usercmd_s;
struct netadr_s;
struct delta_s;
struct sequenceEntry_s;
struct sentenceEntry_s;

typedef struct enginefuncs_s {
    int             (*pfnPrecacheModel)                 (char *s);
    int             (*pfnPrecacheSound)                 (char *s);
    void            (*pfnSetModel)                      (edict_t *e, const char *m);
    int             (*pfnModelIndex)                    (const char *m);
    int             (*pfnModelFrames)                   (int modelIndex);
    void            (*pfnSetSize)                       (edict_t *e, const float *rgflMin, const float *rgflMax);
    void            (*pfnChangeLevel)                   (char *s1, char *s2);
    void            (*pfnGetSpawnParms)                 (edict_t *ent);
    void            (*pfnSaveSpawnParms)                (edict_t *ent);
    float           (*pfnVecToYaw)                      (const float *rgflVector);
    void            (*pfnVecToAngles)                   (const float *rgflVectorIn, float *rgflVectorOut);
    void            (*pfnMoveToOrigin)                  (edict_t *ent, const float *pflGoal, float dist, int iMoveType);
    void            (*pfnChangeYaw)                     (edict_t *ent);
    void            (*pfnChangePitch)                   (edict_t *ent);
    edict_t*        (*pfnFindEntityByString)            (edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue);
    int             (*pfnGetEntityIllum)                (edict_t *pEnt);
    edict_t*        (*pfnFindEntityInSphere)            (edict_t *pEdictStartSearchAfter, const float *org, float rad);
    edict_t*        (*pfnFindClientInPVS)               (edict_t *pEdict);
    edict_t*        (*pfnEntitiesInPVS)                 (edict_t *pplayer);
    void            (*pfnMakeVectors)                   (const float *rgflVector);
    void            (*pfnAngleVectors)                  (const float *rgflVector, float *forward, float *right, float *up);
    edict_t*        (*pfnCreateEntity)                  (void);
    void            (*pfnRemoveEntity)                  (edict_t *e);
    edict_t*        (*pfnCreateNamedEntity)             (int className);
    void            (*pfnMakeStatic)                    (edict_t *ent);
    int             (*pfnEntIsOnFloor)                  (edict_t *e);
    int             (*pfnDropToFloor)                   (edict_t *e);
    int             (*pfnWalkMove)                      (edict_t *ent, float yaw, float dist, int iMode);
    void            (*pfnSetOrigin)                     (edict_t *e, const float *rgflOrigin);
    void            (*pfnEmitSound)                     (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch);
    void            (*pfnEmitAmbientSound)              (edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch);
    void            (*pfnTraceLine)                     (const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
    void            (*pfnTraceToss)                     (edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr);
    int             (*pfnTraceMonsterHull)              (edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
    void            (*pfnTraceHull)                     (const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr);
    void            (*pfnTraceModel)                    (const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr);
    const char*     (*pfnTraceTexture)                  (edict_t *pTextureEntity, const float *v1, const float *v2);
    void            (*pfnTraceSphere)                   (const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr);
    void            (*pfnGetAimVector)                  (edict_t *ent, float speed, float *rgflReturn);
    void            (*pfnServerCommand)                 (char *str);
    void            (*pfnServerExecute)                 (void);
    void            (*pfnClientCommand)                 (edict_t *pEdict, char *szFmt, ...);
    void            (*pfnParticleEffect)                (const float *org, const float *dir, float color, float count);
    void            (*pfnLightStyle)                    (int style, char *val);
    int             (*pfnDecalIndex)                    (const char *name);
    int             (*pfnPointContents)                 (const float *rgflVector);
    void            (*pfnMessageBegin)                  (int msg_dest, int msg_type, const float *pOrigin, edict_t *ed);
    void            (*pfnMessageEnd)                    (void);
    void            (*pfnWriteByte)                     (int iValue);
    void            (*pfnWriteChar)                     (int iValue);
    void            (*pfnWriteShort)                    (int iValue);
    void            (*pfnWriteLong)                     (int iValue);
    void            (*pfnWriteAngle)                    (float flValue);
    void            (*pfnWriteCoord)                    (float flValue);
    void            (*pfnWriteString)                   (const char *sz);
    void            (*pfnWriteEntity)                   (int iValue);
    void            (*pfnCVarRegister)                  (cvar_t *pCvar);
    float           (*pfnCVarGetFloat)                  (const char *szVarName);
    const char*     (*pfnCVarGetString)                 (const char *szVarName);
    void            (*pfnCVarSetFloat)                  (const char *szVarName, float flValue);
    void            (*pfnCVarSetString)                 (const char *szVarName, const char *szValue);
    void            (*pfnAlertMessage)                  (ALERT_TYPE atype, char *szFmt, ...);
    void            (*pfnEngineFprintf)                 (void *pfile, char *szFmt, ...);
    void*           (*pfnPvAllocEntPrivateData)         (edict_t *pEdict, int32_t cb);
    void*           (*pfnPvEntPrivateData)              (edict_t *pEdict);
    void            (*pfnFreeEntPrivateData)            (edict_t *pEdict);
    const char*     (*pfnSzFromIndex)                   (int iString);
    int             (*pfnAllocString)                   (const char *szValue);
    entvars_t*      (*pfnGetVarsOfEnt)                  (edict_t *pEdict);
    edict_t*        (*pfnPEntityOfEntOffset)            (int iEntOffset);
    int             (*pfnEntOffsetOfPEntity)            (const edict_t *pEdict);
    int             (*pfnIndexOfEdict)                  (const edict_t *pEdict);
    edict_t*        (*pfnPEntityOfEntIndex)             (int iEntIndex);
    edict_t*        (*pfnFindEntityByVars)              (entvars_t *pvars);
    void*           (*pfnGetModelPtr)                   (edict_t *pEdict);
    int             (*pfnRegUserMsg)                    (const char *pszName, int iSize);
    void            (*pfnAnimationAutomove)             (const edict_t *pEdict, float flTime);
    void            (*pfnGetBonePosition)               (const edict_t *pEdict, int iBone, float *rgflOrigin, float *rgflAngles);
    uint32_t        (*pfnFunctionFromName)              (const char *pName);
    const char*     (*pfnNameForFunction)               (uint32_t function);
    void            (*pfnClientPrintf)                  (edict_t *pEdict, PRINT_TYPE ptype, const char *szMsg);
    void            (*pfnServerPrint)                   (const char *szMsg);
    const char*     (*pfnCmd_Args)                      (void);
    const char*     (*pfnCmd_Argv)                      (int argc);
    int             (*pfnCmd_Argc)                      (void);
    void            (*pfnGetAttachment)                 (const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles);
    void            (*pfnCRC32_Init)                    (CRC32_t *pulCRC);
    void            (*pfnCRC32_ProcessBuffer)           (CRC32_t *pulCRC, void *p, int len);
    void            (*pfnCRC32_ProcessByte)             (CRC32_t *pulCRC, unsigned char ch);
    CRC32_t         (*pfnCRC32_Final)                   (CRC32_t pulCRC);
    int32_t         (*pfnRandomLong)                    (int32_t lLow, int32_t lHigh);
    float           (*pfnRandomFloat)                   (float flLow, float flHigh);
    void            (*pfnSetView)                       (const edict_t *pClient, const edict_t *pViewent);
    float           (*pfnTime)                          (void);
    void            (*pfnCrosshairAngle)                (const edict_t *pClient, float pitch, float yaw);
    byte*           (*pfnLoadFileForMe)                 (char *filename, int *pLength);
    void            (*pfnFreeFile)                      (void *buffer);
    void            (*pfnEndSection)                    (const char *pszSectionName);
    int             (*pfnCompareFileTime)               (char *filename1, char *filename2, int *iCompare);
    void            (*pfnGetGameDir)                    (char *szGetGameDir);
    void            (*pfnCvar_RegisterVariable)         (cvar_t *variable);
    void            (*pfnFadeClientVolume)              (const edict_t *pEdict, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds);
    void            (*pfnSetClientMaxspeed)             (const edict_t *pEdict, float fNewMaxspeed);
    edict_t*        (*pfnCreateFakeClient)              (const char *netname);
    void            (*pfnRunPlayerMove)                 (edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, byte msec);
    int             (*pfnNumberOfEntities)              (void);
    char*           (*pfnGetInfoKeyBuffer)              (edict_t *e);
    char*           (*pfnInfoKeyValue)                  (char *infobuffer, char *key);
    void            (*pfnSetKeyValue)                   (char *infobuffer, char *key, char *value);
    void            (*pfnSetClientKeyValue)             (int clientIndex, char *infobuffer, char *key, char *value);
    int             (*pfnIsMapValid)                    (char *filename);
    void            (*pfnStaticDecal)                   (const float *origin, int decalIndex, int entityIndex, int modelIndex);
    int             (*pfnPrecacheGeneric)               (char *s);
    int             (*pfnGetPlayerUserId)               (edict_t *e);
    void            (*pfnBuildSoundMsg)                 (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed);
    int             (*pfnIsDedicatedServer)             (void);
    cvar_t*         (*pfnCVarGetPointer)                (const char *szVarName);
    unsigned int    (*pfnGetPlayerWONId)                (edict_t *e);

    void            (*pfnInfo_RemoveKey)                (char *s, const char *key);
    const char*     (*pfnGetPhysicsKeyValue)            (const edict_t *pClient, const char *key);
    void            (*pfnSetPhysicsKeyValue)            (const edict_t *pClient, const char *key, const char *value);
    const char*     (*pfnGetPhysicsInfoString)          (const edict_t *pClient);
    unsigned short  (*pfnPrecacheEvent)                 (int type, const char *psz);
    void            (*pfnPlaybackEvent)                 (int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);

    unsigned char*  (*pfnSetFatPVS)                     (float *org);
    unsigned char*  (*pfnSetFatPAS)                     (float *org);
    int             (*pfnCheckVisibility)               (const edict_t *entity, unsigned char *pset);

    void            (*pfnDeltaSetField)                 (struct delta_s *pFields, const char *fieldname);
    void            (*pfnDeltaUnsetField)               (struct delta_s *pFields, const char *fieldname);
    void            (*pfnDeltaAddEncoder)               (char *name, void (*conditionalencode)(struct delta_s *pFields, const unsigned char *from, const unsigned char *to));
    int             (*pfnGetCurrentPlayer)              (void);
    int             (*pfnCanSkipPlayer)                 (const edict_t *player);
    int             (*pfnDeltaFindField)                (struct delta_s *pFields, const char *fieldname);
    void            (*pfnDeltaSetFieldByIndex)          (struct delta_s *pFields, int fieldNumber);
    void            (*pfnDeltaUnsetFieldByIndex)        (struct delta_s *pFields, int fieldNumber);

    void            (*pfnSetGroupMask)                  (int mask, int op);
    int             (*pfnCreateInstancedBaseline)       (int classname, struct entity_state_s *baseline);
    void            (*pfnCvar_DirectSet)                (struct cvar_s *var, char *value);
    void            (*pfnForceUnmodified)               (FORCE_TYPE type, float *mins, float *maxs, const char *filename);
    void            (*pfnGetPlayerStats)                (const edict_t *pClient, int *ping, int *packet_loss);
    void            (*pfnAddServerCommand)              (char *cmd_name, void (*function)(void));

    qboolean        (*pfnVoice_GetClientListening)      (int iReceiver, int iSender);
    qboolean        (*pfnVoice_SetClientListening)      (int iReceiver, int iSender, qboolean bListen);
    const char*     (*pfnGetPlayerAuthId)               (edict_t *e);

    struct sequenceEntry_s* (*pfnSequenceGet)           (const char *fileName, const char *entryName);
    struct sentenceEntry_s* (*pfnSequencePickSentence)  (const char *groupName, int pickMethod, int *picked);
    int             (*pfnGetFileSize)                   (char *filename);
    unsigned int    (*pfnGetApproxWavePlayLen)          (const char *filepath);
    int             (*pfnIsCareerMatch)                 (void);
    int             (*pfnGetLocalizedStringLength)      (const char *label);
    void            (*pfnRegisterTutorMessageShown)     (int mid);
    int             (*pfnGetTimesTutorMessageShown)     (int mid);
    void            (*ProcessTutorMessageDecayBuffer)   (int *buffer, int bufferLength);
    void            (*ConstructTutorMessageDecayBuffer) (int *buffer, int bufferLength);
    void            (*ResetTutorMessageDecayData)       (void);
    void            (*pfnQueryClientCvarValue)          (const edict_t *player, const char *cvarName);
    void            (*pfnQueryClientCvarValue2)         (const edict_t *player, const char *cvarName, int requestID);
    int             (*pfnCheckParm)                     (const char *pchCmdLineToken, char **ppnext);
    edict_t*        (*pfnPEntityOfEntIndexAllEntities)  (int iEntIndex);
} enginefuncs_t;

typedef struct {
    void        (*pfnGameInit)                  (void);
    int         (*pfnSpawn)                     (edict_t *pent);
    void        (*pfnThink)                     (edict_t *pent);
    void        (*pfnUse)                       (edict_t *pentUsed, edict_t *pentOther);
    void        (*pfnTouch)                     (edict_t *pentTouched, edict_t *pentOther);
    void        (*pfnBlocked)                   (edict_t *pentBlocked, edict_t *pentOther);
    void        (*pfnKeyValue)                  (edict_t *pentKeyvalue, KeyValueData *pkvd);
    void        (*pfnSave)                      (edict_t *pent, SAVERESTOREDATA *pSaveData);
    int         (*pfnRestore)                   (edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity);
    void        (*pfnSetAbsBox)                 (edict_t *pent);

    void        (*pfnSaveWriteFields)           (SAVERESTOREDATA *, const char *, void *, struct TYPEDESCRIPTION_s *, int);
    void        (*pfnSaveReadFields)            (SAVERESTOREDATA *, const char *, void *, struct TYPEDESCRIPTION_s *, int);
    void        (*pfnSaveGlobalState)           (SAVERESTOREDATA *);
    void        (*pfnRestoreGlobalState)        (SAVERESTOREDATA *);
    void        (*pfnResetGlobalState)          (void);

    qboolean    (*pfnClientConnect)             (edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128]);
    void        (*pfnClientDisconnect)          (edict_t *pEntity);
    void        (*pfnClientKill)                (edict_t *pEntity);
    void        (*pfnClientPutInServer)         (edict_t *pEntity);
    void        (*pfnClientCommand)             (edict_t *pEntity);
    void        (*pfnClientUserInfoChanged)     (edict_t *pEntity, char *infobuffer);

    void        (*pfnServerActivate)            (edict_t *pEdictList, int edictCount, int clientMax);
    void        (*pfnServerDeactivate)          (void);

    void        (*pfnPlayerPreThink)            (edict_t *pEntity);
    void        (*pfnPlayerPostThink)           (edict_t *pEntity);

    void        (*pfnStartFrame)                (void);
    void        (*pfnParmsNewLevel)             (void);
    void        (*pfnParmsChangeLevel)          (void);

    const char* (*pfnGetGameDescription)        (void);
    void        (*pfnPlayerCustomization)       (edict_t *pEntity, struct customization_s *pCustom);

    void        (*pfnSpectatorConnect)          (edict_t *pEntity);
    void        (*pfnSpectatorDisconnect)       (edict_t *pEntity);
    void        (*pfnSpectatorThink)            (edict_t *pEntity);

    void        (*pfnSys_Error)                 (const char *error_string);

    void        (*pfnPM_Move)                   (struct playermove_s *ppmove, qboolean server);
    void        (*pfnPM_Init)                   (struct playermove_s *ppmove);
    char        (*pfnPM_FindTextureType)        (char *name);
    void        (*pfnSetupVisibility)           (edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas);
    void        (*pfnUpdateClientData)          (const edict_t *ent, int sendweapons, struct clientdata_s *cd);
    int         (*pfnAddToFullPack)             (struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet);
    void        (*pfnCreateBaseline)            (int player, int eindex, struct entity_state_s *baseline, edict_t *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs);
    void        (*pfnRegisterEncoders)          (void);
    int         (*pfnGetWeaponData)             (edict_t *player, struct weapon_data_s *info);

    void        (*pfnCmdStart)                  (const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed);
    void        (*pfnCmdEnd)                    (const edict_t *player);

    int         (*pfnConnectionlessPacket)      (const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size);
    int         (*pfnGetHullBounds)             (int hullnumber, float *mins, float *maxs);
    void        (*pfnCreateInstancedBaselines)  (void);
    int         (*pfnInconsistentFile)          (const edict_t *player, const char *filename, char *disconnect_message);
    int         (*pfnAllowLagCompensation)      (void);
} DLL_FUNCTIONS;

typedef struct {
    void        (*pfnOnFreeEntPrivateData)      (edict_t *pEnt);
    void        (*pfnGameShutdown)              (void);
    int         (*pfnShouldCollide)             (edict_t *pentTouched, edict_t *pentOther);
    void        (*pfnCvarValue)                 (const edict_t *pEnt, const char *value);
    void        (*pfnCvarValue2)                (const edict_t *pEnt, int requestID, const char *cvarName, const char *value);
} NEW_DLL_FUNCTIONS;

// Engine / Entity constants
#define FL_FLY              (1 << 0)
#define FL_SWIM             (1 << 1)
#define FL_CLIENT           (1 << 3)
#define FL_INWATER          (1 << 4)
#define FL_MONSTER          (1 << 5)
#define FL_GODMODE          (1 << 6)
#define FL_NOTARGET         (1 << 7)
#define FL_ONGROUND         (1 << 9)
#define FL_PARTIALGROUND    (1 << 10)
#define FL_WATERJUMP        (1 << 11)
#define FL_FROZEN           (1 << 12)
#define FL_DUCKING          (1 << 14)
#define FL_BASEVELOCITY     (1 << 22)
#define FL_ONTRAIN          (1 << 24)
#define FL_WORLDBRUSH       (1 << 25)
#define FL_KILLME           (1 << 30)

#define MOVETYPE_NONE       0
#define MOVETYPE_WALK       3
#define MOVETYPE_STEP       4
#define MOVETYPE_FLY        5
#define MOVETYPE_TOSS       6
#define MOVETYPE_PUSH       7
#define MOVETYPE_NOCLIP     8
#define MOVETYPE_BOUNCE     10

#define SOLID_NOT           0
#define SOLID_TRIGGER       1
#define SOLID_BBOX          2
#define SOLID_SLIDEBOX      3
#define SOLID_BSP           4

#define DEAD_NO             0
#define DEAD_DYING          1
#define DEAD_DEAD           2
#define DEAD_RESPAWNABLE    3

#define DAMAGE_NO           0.0f
#define DAMAGE_YES          1.0f
#define DAMAGE_AIM          2.0f

#define EF_BRIGHTFIELD      1
#define EF_BRIGHTLIGHT      4
#define EF_DIMLIGHT         8
#define EF_LIGHT            64
#define EF_NODRAW           128

#define kRenderNormal       0
#define kRenderFxNone       0
#define kRenderFxGlowShell  19

// Network message constants
#define MSG_BROADCAST       0
#define MSG_ONE             1
#define MSG_ALL             2
#define MSG_PVS             4
#define MSG_ONE_UNRELIABLE  8

#define SVC_TEMPENTITY      23

#define TE_TELEPORT         11
#define TE_IMPLOSION        14
#define TE_DLIGHT           27
#define TE_TEXTMESSAGE      29
#define TE_PARTICLEBURST    122

// ScreenFade flags
#define FFADE_IN            0x0000
#define FFADE_OUT           0x0001
#define FFADE_MODULATE      0x0002
#define FFADE_STAYOUT       0x0004

// World contents & hulls
#define CONTENTS_EMPTY      -1
#define CONTENTS_SOLID      -2
#define CONTENTS_WATER      -3
#define CONTENTS_SLIME      -4
#define CONTENTS_LAVA       -5
#define CONTENTS_SKY        -6

#define point_hull          0
#define human_hull          1
#define large_hull          2
#define head_hull           3

// Sound channels
#define CHAN_AUTO           0
#define CHAN_WEAPON         1
#define CHAN_VOICE          2
#define CHAN_ITEM           3
#define CHAN_BODY           4
#define CHAN_STATIC         6

#define ATTN_NONE           0.0f
#define ATTN_NORM           0.8f
#define PITCH_NORM          100
#define PITCH_LOW           85
#define PITCH_HIGH          125

// Buttons & weapons
#define IN_ATTACK           (1 << 0)
#define IN_JUMP             (1 << 1)
#define IN_DUCK             (1 << 2)
#define IN_FORWARD          (1 << 3)
#define IN_BACK             (1 << 4)
#define IN_USE              (1 << 5)
#define IN_MOVELEFT         (1 << 9)
#define IN_MOVERIGHT        (1 << 10)

#define WEAPON_SUIT         31
#define SF_NORESPAWN        (1 << 30) // Valve cbase.h: never respawn an item

// Compile-time x86 GoldSrc ABI layout checks
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(Vec3) == 12, "Vec3 size mismatch");
static_assert(sizeof(globalvars_t) == 172, "globalvars_t size mismatch");
static_assert(sizeof(entvars_t) == 676, "entvars_t size mismatch");
static_assert(sizeof(edict_t) == 804, "edict_t size mismatch");
static_assert(offsetof(edict_t, v) == 128, "edict_t::v offset mismatch");
static_assert(sizeof(TraceResult) == 56, "TraceResult size mismatch");
static_assert(sizeof(KeyValueData) == 16, "KeyValueData size mismatch");
static_assert(sizeof(cvar_t) == 20, "cvar_t size mismatch");
static_assert(sizeof(enginefuncs_t) == 636, "enginefuncs_t size mismatch");
static_assert(offsetof(enginefuncs_t, pfnCheckParm) == 628, "enginefuncs_t::pfnCheckParm offset mismatch");
// Cross-checked against the 32-bit Valve HL SDK headers, not just sizeof.
static_assert(offsetof(globalvars_t, maxClients) == 144, "globalvars_t::maxClients offset mismatch");
static_assert(offsetof(globalvars_t, pStringBase) == 152, "globalvars_t::pStringBase offset mismatch");
static_assert(offsetof(entvars_t, velocity) == 32, "entvars_t::velocity offset mismatch");
static_assert(offsetof(entvars_t, health) == 352, "entvars_t::health offset mismatch");
static_assert(offsetof(entvars_t, weapons) == 360, "entvars_t::weapons offset mismatch");
static_assert(offsetof(entvars_t, flags) == 420, "entvars_t::flags offset mismatch");
static_assert(offsetof(entvars_t, maxspeed) == 528, "entvars_t::maxspeed offset mismatch");
static_assert(offsetof(entvars_t, flFallVelocity) == 564, "entvars_t::flFallVelocity offset mismatch");
static_assert(offsetof(entvars_t, iuser4) == 592, "entvars_t::iuser4 offset mismatch");
static_assert(offsetof(enginefuncs_t, pfnCreateNamedEntity) == 92, "engine entity callback offset mismatch");
static_assert(offsetof(enginefuncs_t, pfnTraceHull) == 136, "engine trace callback offset mismatch");
static_assert(offsetof(enginefuncs_t, pfnMessageBegin) == 184, "engine message callback offset mismatch");
static_assert(offsetof(enginefuncs_t, pfnFunctionFromName) == 312, "engine save callback offset mismatch");
static_assert(offsetof(enginefuncs_t, pfnNumberOfEntities) == 420, "engine edict callback offset mismatch");
static_assert(offsetof(enginefuncs_t, pfnAddServerCommand) == 560, "engine command callback offset mismatch");
static_assert(offsetof(DLL_FUNCTIONS, pfnClientCommand) == 76, "DLL client command offset mismatch");
static_assert(offsetof(DLL_FUNCTIONS, pfnStartFrame) == 100, "DLL frame callback offset mismatch");

static_assert(sizeof(DLL_FUNCTIONS) == 200, "DLL_FUNCTIONS size mismatch");
static_assert(sizeof(NEW_DLL_FUNCTIONS) == 20, "NEW_DLL_FUNCTIONS size mismatch");
#endif

// Global engine pointers initialized in GiveFnptrsToDll
extern enginefuncs_t g_engfuncs;
extern globalvars_t *gpGlobals;
extern DLL_FUNCTIONS g_origDllFuncs;
extern qboolean      g_bHasOrigDllFuncs;

inline qboolean FNullEnt(const edict_t *pent) {
    return (pent == nullptr || pent->free != 0 ||
            (g_engfuncs.pfnEntOffsetOfPEntity && g_engfuncs.pfnEntOffsetOfPEntity(pent) == 0));
}

inline const char *STRING(string_t offset) {
    return (gpGlobals && gpGlobals->pStringBase) ? (gpGlobals->pStringBase + offset) : "";
}

#endif // HLSDK_MINI_H
