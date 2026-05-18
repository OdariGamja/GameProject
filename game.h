// game.h

#ifndef GAME_H
#define GAME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>

#define MAX_PATH 256
#define MAX_LINEBUF 256
#define MOVE_SPEED 20.f
#define WD_Width 1980
#define WD_Height 1080
#define MAP_Width 10312
#define MAP_Height 1391

#define MAX_OBJECTS 50
#define MAX_CLIENT 4

#define ATM_ConstNumber 225
#define Vendor_ConstNumber 230
#define ATTACK_ANIM_TIME 400

#define GAME_LIMIT_SEC 300 // 5分 = 300秒
#define GAME_LIMIT_MS (GAME_LIMIT_SEC * 1000)

#define BGM_CHANNEL 0

#define MAX_SE_NUMBER SE_MAX


//サウンド列挙体
typedef enum{
    SE_KnockDown,
    SE_Die,
    SE_Raise,
    SE_ElectricDown,
    SE_GameStart,
    SE_Gate,
    SE_HIT1,
    SE_HIT2,
    SE_HIT3,
    SE_HIT4,
    SE_Lobby,
    SE_PressStart,
    SE_Score,
    SE_SinkanSen,
    SE_HitbyTrain,
    SE_IngameBGM,
    SE_Announce,
    SE_FrontTrain,
    SE_Train,
    SE_Vendor,
    SE_ATM,
    SE_ATTACK,
    SE_EndInterval,


    SE_MAX
} SE_Type;

//clientの接続状態列挙体
typedef enum
{
    NOW_READY = 0,
    NOW_START = 1,
    NOW_INGAME = 2
} ClientNow;

//キャラクタタイプ列挙体
typedef enum
{
    CT_PLAYER0 = 0,
    CT_PLAYER1 = 1,
    CT_PLAYER2 = 2,
    CT_PLAYER3 = 3,
    CT_COIN = 4,
    CT_TRAIN = 5,
    CT_TRAININSIDE = 6,
    CT_TRAININSIDEDOOR = 7,
    CT_MOUSE = 8,
    CT_Vendor = 9,
    CT_ATM = 10,
    CT_TRAIN_PASS = 11,
    CT_IMG = 12,
    CT_REDCOIN = 13,
    CT_Arrival_Board = 14,
    CT_Left_Pillar = 15,
    CT_Right_Pillar = 16,
    CT_Lever = 17,
    CT_BENCH = 18
} CharaType;
#define CHARATYPE_NUM 19

//キャラクタタイプの構造体
typedef struct
{
    int w, h;
    char *path;
    SDL_Point aninum;
    SDL_Texture *img;
} CharaTypeInfo;

//キャラクタ状態列挙体
typedef enum
{
    CS_Normal = 0,
    CS_Disable = 1,
    CS_Holded = 2, 
    CS_Fly = 3,
    CS_Knockdown = 4,
    CS_Death = 5,
    CS_FallWait = 6,
    CS_Falling = 7
} CharaStts;

//アニメーション用列挙体
typedef enum
{
    ANI_Stop = 0,
    ANI_RunRight = 1,
    ANI_RunLeft = 2,
    ANI_RunDown = 3,
    ANI_RunUp = 4,
    ANI_RightAttack = 5,
    ANI_LeftAttack = 6,
    ANI_DownAttack = 7,
    ANI_UpAttack = 8,
    ANI_TrainDoorCLOSE = 9,
    ANI_TrainDoorOPEN = 10,
    ANI_ATM_LIGHTON_DEFALT = 11,
    ANI_ATM_LIGHTOFF_DEFALT = 12,
    ANI_ATM_LIGHTOFF_VIBRATE = 13,
    ANI_VENDER_PUMP = 14,
    ANI_HIT_RIGHT = 15,
    ANI_HIT_LEFT = 16,
    ANI_HIT_DOWN = 17,
    ANI_HIT_UP = 18,
    ANI_KNOCKDOWN = 19,
    ANI_RAISE_RIGHT = 20,
    ANI_RAISE_LEFT = 21,
    ANI_RAISE_DOWN = 22,
    ANI_RAISE_UP = 23,
    ANI_SPIN = 24,
    ANI_Board_LightOn = 25, 
    ANI_Board_LightOff = 26,
    ANI_BlackOutOn = 27,
    ANI_BlackOutOff = 28
} CharaMoveStts;


//入力列挙体
typedef struct
{
    SDL_bool right;
    SDL_bool left;
    SDL_bool down;
    SDL_bool up;
    SDL_bool space;
    int spacePrev;
    int spacePressed;
    ClientNow nowstts;
} Keystts;

//キャラクタ方向
typedef enum
{
    Right = 1,
    Left = 2,
    Down = 3,
    Up = 4
} Dir;

//Audio用構造体
typedef struct {
    CharaMoveStts beforeMovestts;   //前のmovestts
    CharaStts beforeStts;
    int chanel;    //キャラクタが持つ効果音のチャネル
    int raisePlayed;
    int isLoopSE;
    int beforeNowstts;
} AudioSTATE;


//キャラクタの情報構造体
typedef struct CharaInfo_t
{
    CharaStts stts;
    CharaTypeInfo *entity;
    SDL_Rect rect;
    SDL_Rect Vendor_rect;
    SDL_Rect attackrectUp;
    SDL_Rect attackrectDown;
    SDL_Rect attackrectLeft;
    SDL_Rect attackrectRight;
    SDL_Rect Baserect;
    SDL_Rect imgsrc;
    SDL_FPoint vel;
    float knockVX;
    float knockVY;
    float vx, vy;
    int playerdir;
    SDL_FPoint point;
    SDL_Point ani;
    CharaType type;
    CharaMoveStts movestts;
    float aniTimer;
    struct CharaInfo_t *next;
    int exp;
    int level;
    Keystts input;        
    long long coinRespawnTime; 
    long long AtmTime;
    long long VibrateEnd;
    long long KnockdownTime;
    int animTimer;

    int score;
    int hitcount;
    int deathcount;

    int hp;
    int hold; 
    struct CharaInfo_t *holder;
    struct CharaInfo_t *holdTarget;
    float throwBaseY; 
    float throwLandX;
    float throwLandY; 
    SDL_bool InSubway;
    int inputcount;
    int whoholder; // player0 が1　player1が2 と一個ずつずらしてる
    long long knockRemainMs; // 残りKnockdown時間
    long long knockLastMs;   // 前回更新時刻
    long long lastAttackMs;  // 最後に攻撃したときの時間
    long long deadAtMs;      // 死亡した時刻（ms）
    long long fallStartMs;   // 落下待ち開始時刻
    float fallVy;            // 落下速度
    int invincibleflag;      // 無敵時間のフラグ０がノーマル１が無敵
    long long invincibleStartMs;
    int railroadflag; // 線路に落ちてるとき１
    int hitByTrain;
    int hitBySinkansen;
    float railFallRemain;
    long long hitByTrainStartMs;    // 列車に当たってからの経過時間
    long long hitByTrainDurationMs; // = 3000
    int notinfrontofdoor;           // 電車内でドアの前でないなら１
    int holedindubway;              // 持たれてるとき電車に入ったら１
    int inthemovesubway;            // 電車に囚われたら１
    long long hitStartMs;
    int hitDurationMs;
    int prevMovestts;
    int animPlayed;

    int IntheHole;
    Uint32 invincibleBlinkTimer; // 点滅タイマー
    SDL_bool invincibleVisible;  // 現在見えるかどうか

    Uint32 blackoutStartTick; // blackout

    long long gameTime; // Start 〜 Ending time
    AudioSTATE audio_state;

} CharaInfo;

//ゲーム状態
typedef enum
{
    GS_Ready = 0,
    GS_Playing = 1
} GameStts;

//カメラの位置情報
typedef struct
{
    int x, y; 
    int w, h; 
} Camera;


// client <-> sever pack
typedef struct
{ 
    float x, y;
    SDL_Rect rect;
    int visible;
    int score;
    int hitcount;
    int deathcount;
    CharaMoveStts Movestts;
    SDL_bool InSubway;
    Dir direction;
    int whoholder;
    CharaStts stts;
    int railroadflag;
    int hitByTrain;
    int hitBySinkansen;
    int nowstts;
    int invincibleflag;
    int64_t gameTime;
    int IntheHole;
} NetPack;

//client別のゲーム構造体
typedef struct
{
    CharaInfo *player;
    GameStts stts;
    SDL_Window *window;
    SDL_Renderer *render;
    SDL_Texture *bg;
    SDL_Texture *wall;
    SDL_Texture *Texture_UI;
    SDL_Texture *space;
    SDL_Rect image_src_UI;
    SDL_Point dp;
    Keystts input;
    Camera cam;
    float timeDelta;
    SDL_Texture *Texture_Number;    // 数字画像用のテクスチャ 
    SDL_Texture *Texture_TimeBoard; // タイマー背景の看板画像

    float shakeTime;     // 残り揺れ時間(秒)
    float shakeStrength; // 揺れ強度(ピクセル)

    //UI用
    float ui_y;        
    float ui_target_y; 
    float ui_speed;    

    Mix_Chunk *SE_Load[MAX_SE_NUMBER];//追加箇所0124    音源（クライアント）
    
    Mix_Music *BGM_Lobby;
    Mix_Music *BGM_Ingame;
} GameInfo;


enum
{
    THROWtoVendor,
    THROWtoVendorRed
} Event;



//関数及び構造体
extern GameInfo gGames[CHARATYPE_NUM];
extern CharaTypeInfo gCharaType[CHARATYPE_NUM];
extern int my_id, sock;


int InitSystem(const char *chara_file, const char *pos_file, int my_id);
void DestroySystem(void);
void InitCharaInfo(CharaInfo *ch);
SDL_bool InputEvent(GameInfo *game);
void MoveChara(CharaInfo *ch, GameInfo *game);
int UI_Transition(SDL_Renderer *r, int fade);
void ResetUITransition(void);
void DrawBg(GameInfo *gGame);
void DrawSpinPens(GameInfo *gGame);
void TrainEffect(GameInfo *gGame);
void ResetResultUI(void);
int DrawResultUI(GameInfo *g);
void InitSoundInfo(GameInfo *gGame);


void DrawStartTEXTUI(GameInfo *g);
void DrawCountDown(GameInfo *gGame, int value, float scale);
SDL_bool AllPlayersStarted(void);
int DrawResultUI(GameInfo *g);
void DrawNumber(GameInfo *gGame, int value, int x, int y, float scale);
void ResetResultText(void);
void DrawSpaceImage(GameInfo *gGame);
void UpdateInvincibleBlink(CharaInfo *ch);
void SendInput(Keystts *input);
int InitWindow(GameInfo *game, const char *title, const char *bg_file, int width, int height);
void CloseWindow(GameInfo *game);
void UpdateAnimation(GameInfo *game, float deltaTime);
void DrawGame(GameInfo *game);
int PrintError(const char *msg);
CharaInfo *GetPlayerChara(int my_id);
void UpdateCamera(GameInfo *gGame, CharaInfo *player);
void StartMenu(GameInfo *gGame);
void TrainMovement(CharaInfo *ch, long long cur);
void TrainMovement_Pass(CharaInfo *ch);

void DisplayMovement(CharaInfo *ch);
void DisplayDrawUpdate(CharaInfo *ch);
SDL_bool IMGMovement(CharaInfo *ch);
void UpdateHoleFall(CharaInfo *ch);
void UpdateKnockback(CharaInfo *ch);
void UpdateATM(CharaInfo *atm);
void StartScreenShake(GameInfo *gGame, float strength, float duration);
void UpdateBlackout(CharaInfo *ch);
void DrawBlackOut(SDL_Renderer *r, int screenW, int screenH);

void InitAudio(void);
void InitSound_Load(GameInfo *gGame);
void PlayAudio(GameInfo *gGame);
int GetSE(CharaInfo *ch);
void UpdateVolume(CharaInfo *myplayer, CharaInfo *sound_sorce);
void PlayBGMFadeIn_Music(GameInfo *gGame, int bgmKind, int ms);
void StopBGMFadeOut_Music(int ms);
void CloseAudio(void);


#endif