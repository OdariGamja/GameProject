#ifndef SERVER_SYSTEM_H
#define SERVER_SYSTEM_H
#define TICK_MS 33

#define THROW_POWER_X 5000.0f
#define THROW_POWER_UP 50000.0f
#define THROW_POWER_DOWN 7000.0f
#define THROW_GRAVITY 15000.0f

#define FALL_WAIT_MS 1000

#include "game.h"

extern CharaInfo *SevergCharaHead;

extern SDL_Rect fallRect;
extern SDL_Rect fallRect2;

typedef struct
{
    float x;
    float y;
} RespawnPoint;

extern RespawnPoint gRespawnPoints[4];


int InitServerChara(const char *position_data_file, CharaInfo players[], int max_players);


SDL_bool CollisionPlayer(CharaInfo *a, CharaInfo *b);
SDL_bool Collision(CharaInfo *ci, CharaInfo *cj);
SDL_bool FlyCollision(CharaInfo *a, CharaInfo *b);
int PrintError(const char *msg);
void UpdateScaleServer(CharaInfo *ch);
long long now_ms();
void Attack(CharaInfo *ch);
void UpdateAttackRects(CharaInfo *ch);
void TrapInTrain(CharaInfo *ch);
int traindoor;
int InTheTrain;
int TrapHumanSpeed;
int uptrainx;
int InTheRail;
int IsPointInSlantedRect(float px, float py ,CharaInfo *ch );
void MergeSinkansenPlayerPoint(CharaInfo *ch);
void ThrowObject(CharaInfo *ch);
void UpdateATM(CharaInfo *atm);
void UpdateKnockdown(CharaInfo *ch);
void ReleaseHold(CharaInfo *holder, CharaInfo *obj);
void UpdateVendor(CharaInfo *ch);
void CheckDeathZone(CharaInfo *ch, SDL_Rect fallRect);
void RespawnCheck(CharaInfo *ch);
void UpdateFall(CharaInfo *ch, SDL_Rect fallRect);
void KillPlayer(CharaInfo *ch);
void UpdateInvincible(CharaInfo *ch);
void StartHitByTrain(CharaInfo *ch);
void UpdateHitByTrain(CharaInfo *ch);
void UpdateTrainRailFall(CharaInfo *ch);
void RespawnDisabledCoin(void); 
void RespawnDisabledCoin2(void); 
#endif
