// UI.c
#include "game.h"
#include <SDL2/SDL_image.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#define TICK_MS 33


SDL_Texture *illustTex = NULL;
SDL_Texture *startBackTex = NULL;
SDL_Texture *titleTex = NULL;
SDL_Texture *startTex = NULL;
SDL_Texture *quitTex = NULL;
SDL_Texture *spinpenTex = NULL;
SDL_Texture *traineffectTex = NULL;
SDL_Texture *playerwait1 = NULL;
SDL_Texture *playerwait2 = NULL;
SDL_Texture *playerwait3 = NULL;
SDL_Texture *playerwait4 = NULL;
SDL_Texture *networkstatusTex = NULL;
SDL_Texture *youtex = NULL;
SDL_Texture *startTexttex = NULL;
SDL_Texture *resulttex = NULL;
SDL_Texture *scoretex = NULL;


extern CharaInfo *gCharaHead;
SDL_Texture *getPlayerWaitTexture(int playerID)
{
    switch (playerID)
    {
    case 0:
        return playerwait1;
    case 1:
        return playerwait2;
    case 2:
        return playerwait3;
    case 3:
        return playerwait4;
    default:
        return playerwait1; 
    }
}
#define SCREEN_W 1980
#define SCREEN_H 1080
#define READY_W 300   // アニメーション1フレーム 横サイズ
#define READY_H 300   // アニメーション 1フレーム 縦サイズ
#define READY_FRAME 8 // 1行あたりのフレーム数

static float readyY[MAX_CLIENT] = {-400, -400, -400, -400};
static int readyFrame[MAX_CLIENT] = {0, 0, 0, 0};
static int readyDir[MAX_CLIENT] = {1, 1, 1, 1}; // 1: 右、-1: 左

static float spinOffsetX = 0.0f;
static float spinOffsetY = 0.0f;
extern NetPack latest[MAX_CLIENT];
extern pthread_mutex_t pm;

typedef enum
{   UI_INTRO_STARTFADEOUT= 0,
    UI_INTRO = 1,
    UI_MENU = 2,
    UI_WAITING = 3,
    UI_TRANSITION = 4
} UIPhase;
static Uint32 lastNormal[MAX_CLIENT]; // 1行タイマー
static Uint32 lastActive[MAX_CLIENT]; // 2行タイマー

typedef struct
{
    float x, y;
    float vy;
    float life; // ms
} Steam;
#define MAX_STEAM 48
Steam steam[MAX_STEAM];

static Uint32 transStartTime = 0;
static float transX = 0.0f; //黒い板のx位置
static int transStarted = 0;
void ResetUITransition(void)
{
    transStarted = 0;
    transX = 0.0f;
}
// YOU テキストフェード用
float youAlpha = 0.0f;

int UI_Transition(SDL_Renderer *r, int fade)
// fade =  1 : cover in  (右 → 左 カバー)
// fade = -1 : cover out (右 → 左 開く)
{
    const float SPEED = 0.12f;

    float targetX;

    
    // 目標位置
    if (fade > 0)
        targetX = 0.0f; 
    else
        targetX = -SCREEN_W; 

    // =========================
    // 初期化
    // =========================
    if (!transStarted)
    {
        transStartTime = SDL_GetTicks();
        transStarted = 1;

        if (fade > 0)
            transX = SCREEN_W; // 右側の外から開始
        else
            transX = 0.0f; 
    }

    Uint32 now = SDL_GetTicks();

    //3秒待機
    if (fade > 0 && now - transStartTime < 3000)
        return 0;

    transX += (targetX - transX) * SPEED;

    // =========================
    // DRAW
    // =========================
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);

    SDL_Rect cover = {
        (int)transX,
        0,
        SCREEN_W,
        SCREEN_H};
    SDL_RenderFillRect(r, &cover);


    if (fabsf(transX - targetX) < 1.0f)
    {
        transStarted = 0;
        return 1;
    }

    return 0;
}

// ==========================
// Texture 初期化
// ==========================
void LoadStartTextures(GameInfo *gGame)
{
    illustTex = IMG_LoadTexture(gGame->render, "img/illust.png");
    startBackTex = IMG_LoadTexture(gGame->render, "img/startback.png");
    titleTex = IMG_LoadTexture(gGame->render, "img/TITLE.png");
    startTex = IMG_LoadTexture(gGame->render, "img/start.png");
    quitTex = IMG_LoadTexture(gGame->render, "img/quit.png");
    spinpenTex = IMG_LoadTexture(gGame->render, "img/spin.png");
    traineffectTex = IMG_LoadTexture(gGame->render, "img/traineffect.png");
    playerwait1 = IMG_LoadTexture(gGame->render, "img/ready1.png");
    playerwait2 = IMG_LoadTexture(gGame->render, "img/ready2.png");
    playerwait3 = IMG_LoadTexture(gGame->render, "img/ready3.png");
    playerwait4 = IMG_LoadTexture(gGame->render, "img/ready4.png");
    networkstatusTex = IMG_LoadTexture(gGame->render, "img/waiting.png");
    youtex = IMG_LoadTexture(gGame->render, "img/you.png");
    startTexttex = IMG_LoadTexture(gGame->render, "img/starttext.png");
    resulttex = IMG_LoadTexture(gGame->render, "img/result.png");
    scoretex = IMG_LoadTexture(gGame->render, "img/score.png");

    if (!startBackTex || !titleTex || !startTex || !quitTex || 
    !spinpenTex || !spinpenTex || !playerwait1 || !playerwait2 || 
    !playerwait3 || !playerwait4 || !networkstatusTex || !youtex || 
    !startTexttex || !resulttex || !scoretex|| !illustTex)
    {
        printf("Failed to load textures: %s\n", IMG_GetError());
        exit(1);
    }
}


// ==========================
// Texture 破棄
// ==========================
void FreeStartTextures()
{
    if (startBackTex)
        SDL_DestroyTexture(startBackTex);
    if (titleTex)
        SDL_DestroyTexture(titleTex);
    if (startTex)
        SDL_DestroyTexture(startTex);
    if (quitTex)
        SDL_DestroyTexture(quitTex);
    if (spinpenTex)
        SDL_DestroyTexture(spinpenTex);
    if (playerwait1)
        SDL_DestroyTexture(playerwait1);
    if (playerwait2)
        SDL_DestroyTexture(playerwait2);
    if (playerwait3)
        SDL_DestroyTexture(playerwait3);
    if (playerwait4)
        SDL_DestroyTexture(playerwait4);
    if (networkstatusTex)
        SDL_DestroyTexture(networkstatusTex);
    if (youtex)
        SDL_DestroyTexture(youtex);
    if (startTexttex)
        SDL_DestroyTexture(startTexttex);
    if (resulttex)
        SDL_DestroyTexture(youtex);
    if (scoretex)
        SDL_DestroyTexture(youtex);
    if (illustTex)
        SDL_DestroyTexture(illustTex);

}


// ==========================
// 画面下部に移動する列車
// ==========================
void TrainEffect(GameInfo *gGame)
{
    SDL_Rect src = {0, 0, 13730, 504};
    SDL_Rect dst;

    static float trainX = 0.0f;

    const float speed = 8.0f;
    const int resetPoint = 6865;

    const float scale = 0.35f; 

    trainX -= speed;

    if (trainX <= -resetPoint * scale)
    {
        trainX = 0.0f;
    }

    dst.w = (int)(13730 * scale);
    dst.h = (int)(504 * scale);

    dst.x = (int)trainX;
    dst.y = 950;

    SDL_RenderCopy(
        gGame->render,
        traineffectTex,
        &src,
        &dst);
}


// ==========================
// 回転するペンギンの背景
// ==========================
void DrawSpinPens(GameInfo *gGame)
{
    SDL_Rect src = {0, 0, 204, 204};
    SDL_Rect dst;

    const int spacing = 200;
    const float moveSpeed = 0.6f;
    const float rotSpeed = 0.8f;

    const float minScale = 0.4f;
    const float maxScale = 0.6f;

    static float angle = 0.0f;

    spinOffsetX -= moveSpeed;
    spinOffsetY -= moveSpeed;

    if (spinOffsetX <= -spacing)
        spinOffsetX += spacing;
    if (spinOffsetY <= -spacing)
        spinOffsetY += spacing;

    angle += rotSpeed;
    if (angle >= 360)
        angle -= 360;

    for (int y = -spacing; y < SCREEN_H + spacing; y += spacing)
    {
        for (int x = -spacing; x < SCREEN_W + spacing; x += spacing)
        {
            // ==========================
            // チェッカーボードパターン
            // ==========================
            int ix = x / spacing;
            int iy = y / spacing;

            if ((ix + iy) & 1)
                continue;

            float t = (float)(y + spinOffsetY) / SCREEN_H;
            if (t < 0.0f)
                t = 0.0f;
            if (t > 1.0f)
                t = 1.0f;

            float scale = minScale + t * (maxScale - minScale);

            dst.w = (int)(204 * scale);
            dst.h = (int)(204 * scale);

            dst.x = (int)(x + spinOffsetX + (204 - dst.w) * 0.5f);
            dst.y = (int)(y + spinOffsetY + (204 - dst.h) * 0.5f);

            SDL_RenderCopyEx(
                gGame->render,
                spinpenTex,
                &src,
                &dst,
                angle,
                NULL,
                SDL_FLIP_NONE);
        }
    }
}

//======================
//背景の描画
//======================
void DrawBg(GameInfo *gGame)
{
    SDL_RenderClear(gGame->render);
    SDL_RenderCopy(gGame->render, startBackTex, NULL, NULL);
    DrawSpinPens(gGame);
    TrainEffect(gGame);
}

//======================
//Introボタンの描画
//======================
int IntroAnimation(GameInfo *gGame, int come)
{

    SDL_Rect titleSrc = {0, 0, 850, 170};
    SDL_Rect startSrc = {0, 0, 250, 100};
    SDL_Rect quitSrc = {0, 0, 250, 100};
    SDL_Rect illustSrc = {0, 0, 2100, 1260};

    SDL_Rect title = {-1500 + come, 100, 850, 170};
    SDL_Rect startBtn = {2400 - come, 400, 250, 100};
    SDL_Rect quitBtn = {2400 - come, 550, 250, 100};
    SDL_Rect illust = {2400 - come, 200, 1050, 630};


    if (title.x >= 150)
        title.x = 150;
    if (startBtn.x <= 150)
        startBtn.x = 150;
    if (quitBtn.x <= 150)
        quitBtn.x = 150;
    if (illust.x <= 800)
        illust.x = 800;


    SDL_RenderCopy(gGame->render, titleTex, &titleSrc, &title);
    SDL_RenderCopy(gGame->render, startTex, &startSrc, &startBtn);
    SDL_RenderCopy(gGame->render, quitTex, &quitSrc, &quitBtn);
    SDL_RenderCopy(gGame->render, illustTex, &illustSrc, &illust);

    if (title.x != 150 || startBtn.x != 150 || quitBtn.x != 150 || illust.x != 800)
        return 1;

    return 0;
}

//======================
//ボタンの描画
//======================
void DrawButtonAni(GameInfo *gGame, int state)
{
    static float scale[2] = {1.0f, 1.0f};
    static float angle[2] = {0.0f, 0.0f};
    static float hue = 0.0f; 

    const float SCALE_STEP = 0.05f;
    const float MAX_SCALE = 1.5f;
    const float MIN_SCALE = 1.0f;

    const float ANGLE_STEP = 1.9f;
    const float MAX_ANGLE = 8.0f;

    // 色の変化速度
    const float COLOR_SPEED = 1.0f;

    SDL_Rect title = {150, 100, 850, 170};
    SDL_Rect illust = {800, 200, 1050, 630};
    SDL_RenderCopy(gGame->render, titleTex, NULL, &title);
    SDL_RenderCopy(gGame->render, illustTex, NULL, &illust);

    SDL_Rect startBtn = {150, 400, 250, 100};
    SDL_Rect quitBtn = {150, 550, 250, 100};

    // 色変化タイマーの増加
    hue += COLOR_SPEED * (TICK_MS / 1000.0f);

    //　stateに応じてscaleとangleが変化する
    for (int i = 0; i < 2; i++)
    {
        if (i == state)
        {
            scale[i] += SCALE_STEP;
            if (scale[i] > MAX_SCALE)
                scale[i] = MAX_SCALE;

            angle[i] += ANGLE_STEP;
            if (angle[i] > MAX_ANGLE)
                angle[i] = MAX_ANGLE;
        }
        else
        {
            scale[i] -= SCALE_STEP;
            if (scale[i] < MIN_SCALE)
                scale[i] = MIN_SCALE;

            angle[i] -= ANGLE_STEP;
            if (angle[i] < 0)
                angle[i] = 0;
        }
    }

    SDL_Rect startDst = {
        startBtn.x - (int)((scale[0] - 1.0f) * startBtn.w / 2),
        startBtn.y - (int)((scale[0] - 1.0f) * startBtn.h / 2),
        (int)(startBtn.w * scale[0]),
        (int)(startBtn.h * scale[0])};

    SDL_Rect quitDst = {
        quitBtn.x - (int)((scale[1] - 1.0f) * quitBtn.w / 2),
        quitBtn.y - (int)((scale[1] - 1.0f) * quitBtn.h / 2),
        (int)(quitBtn.w * scale[1]),
        (int)(quitBtn.h * scale[1])};

    SDL_Rect src = {0, 0, 400, 176};

    // -----------------------------
    // 色の変化計算
    // -----------------------------
    Uint8 r = 255, g = 255, b = 255;

    // 選択したボタンだけ色が変わる
    if (state == 0)
    {
        float t = (sinf(hue) + 1.0f) / 2.0f; // 0~1
        r = 255;
        g = (Uint8)(120 + 135 * t); // 200~255
        b = (Uint8)(120 + 135 * t);
    }
    else if (state == 1)
    {
        float t = (sinf(hue + 1.57f) + 1.0f) / 2.0f; // 0~1
        r = (Uint8)(120 + 135 * t);
        g = (Uint8)(120 + 135 * t);
        b = 255;
    }

    // 選択したボタンだけにカラーモードを適用
    SDL_SetTextureColorMod(startTex, (state == 0) ? r : 255, (state == 0) ? g : 255, (state == 0) ? b : 255);
    SDL_SetTextureColorMod(quitTex, (state == 1) ? r : 255, (state == 1) ? g : 255, (state == 1) ? b : 255);

    // 回転レンダリング
    SDL_RenderCopyEx(gGame->render, startTex, &src, &startDst, angle[0], NULL, SDL_FLIP_NONE);
    SDL_RenderCopyEx(gGame->render, quitTex, &src, &quitDst, angle[1], NULL, SDL_FLIP_NONE);

    // レンダリングが終わった後、色の変調は元に戻す
    SDL_SetTextureColorMod(startTex, 255, 255, 255);
    SDL_SetTextureColorMod(quitTex, 255, 255, 255);
}

// ================================
// Ready Lobby
// ================================
static float readyY[MAX_CLIENT];
static int readyFrame[MAX_CLIENT];

// 初期化
void InitReadySlots(void)
{
    for (int i = 0; i < MAX_CLIENT ; i++) 
    {
        readyY[i] = -400;
        readyFrame[i] = 0;
        readyDir[i] = 1;
        lastNormal[i] = 0;
        lastActive[i] = 0;
    }
}

static float waitAlpha = 0.0f; 
static float waitSrcY = 0.0f;
static int waitPhase = 0;
/*
0 = 待機
1 = フェードイン (50px)
2 = 表示を保持
3 = 拡張 (50 → 150)
4 = 完了
*/
void UpdateWaitingUIState(void)
{

    // 1. キャラクターが全部降りてきたのか
    int allDown = 1;
    for (int i = 0; i < 4; i++)
    {
        if (fabsf(readyY[i] - 500.0f) > 1.0f)
            allDown = 0;
    }

    // 4人でプレイとき ↓をコメントから出す。　 complie するとき、エラが出るけど、無視して。
    if (allDown && waitPhase == 0) // debug
    waitPhase = 1; 

    // 2. FadeIn
    if (waitPhase == 1)
    {
        youAlpha += 0.03f;
        if (youAlpha > 1.0f)
        {
            youAlpha = 1.0f;
        }

        waitAlpha += 0.03f;
        if (waitAlpha >= 1.0f)
        {
            waitAlpha = 1.0f;
            waitPhase = 2;
        }
    }

    // 3. 全員 READY 確認
    int allReady = 1;
    CharaInfo *ch = gCharaHead;

    int safety = 0;


    while (ch)
    {

        safety++;
        if (safety > 2000) {
            break;
        }

        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3) // debug  ひとり CT_PLAYER0  ->  4人　CT_PLAYER3
        {
            if (ch->input.nowstts != NOW_START )
            {
                allReady = 0;
                break;
            }
        }
        ch = ch->next;
    }

    if (allReady && waitPhase == 2)
        waitPhase = 3;

    // 4. src.y 移動 (0 → 100)
    if (waitPhase == 3)
    {
        waitSrcY += (100.0f - waitSrcY) * 0.12f;

        if (fabsf(waitSrcY - 100.0f) < 1.0f)
        {
            waitSrcY = 100.0f;
            waitPhase = 4;
        }
    }
}

void RenderWaitingUI(SDL_Renderer *render)
{
    if (waitPhase < 1)
        return;

    SDL_SetTextureBlendMode(networkstatusTex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(
        networkstatusTex,
        (Uint8)(255 * waitAlpha));

    SDL_Rect src = {
        0,
        (int)waitSrcY, 
        782,
        50};

    SDL_Rect dst = {
        (SCREEN_W - 782) / 2,
        200,
        782,
        50};

    SDL_RenderCopy(render, networkstatusTex, &src, &dst);
}

//Ready Lobby アップデート + レンダリング
void UpdateReadyLobby(GameInfo *gGame)
{

    static Uint32 last = 0;
    Uint32 now = SDL_GetTicks();

    pthread_mutex_lock(&pm);
    
    UpdateWaitingUIState();
    CharaInfo *ch = gCharaHead;

    // ===== シネマティック演出(READY 降りてくる間) =====
    {
        float startY = -300.0f;
        float targetY = 500.0f;

        float t = (readyY[my_id] - startY) / (targetY - startY);
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;

        t = t * t;

        int barMaxH = 180;
        int barH = (int)(barMaxH * t);

        SDL_SetRenderDrawBlendMode(gGame->render, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(gGame->render, 0, 0, 0, 180);

        SDL_Rect topBar = {
            0, 0,
            SCREEN_W, barH};

        SDL_Rect bottomBar = {
            0, SCREEN_H - barH,
            SCREEN_W, barH};

        SDL_RenderFillRect(gGame->render, &topBar);
        SDL_RenderFillRect(gGame->render, &bottomBar);
    }

    while (ch)
    {
        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3) // debug　ひとり CT_PLAYER0  ->  4人　CT_PLAYER3
        {
            int playerID = ch->type - CT_PLAYER0; 
            int active = (ch->input.nowstts == NOW_START);

            float targetY = 500.0f;
            float speed = 0.085f; 
            readyY[playerID] += (targetY - readyY[playerID]) * speed;

            if (active)
            {
                // 2行
                if (now - lastActive[playerID] > 150)
                {
                    if (readyFrame[playerID] < READY_FRAME - 1)
                        readyFrame[playerID]++;

                    lastActive[playerID] = now;
                }
            }
            else
            {
                // 1行
                if (now - lastNormal[playerID] > 80)
                {
                    if (readyDir[playerID] == 1)
                    {
                        readyFrame[playerID]++;
                        if (readyFrame[playerID] >= READY_FRAME - 1)
                            readyDir[playerID] = -1;
                    }
                    else
                    {
                        readyFrame[playerID]--;
                        if (readyFrame[playerID] <= 0)
                            readyDir[playerID] = 1;
                    }

                    lastNormal[playerID] = now;
                }
            }

            SDL_Rect src, dst;
            SDL_Texture *tex = getPlayerWaitTexture(playerID);

            src.x = readyFrame[playerID] * READY_W;
            src.y = active ? READY_H : 0;
            src.w = READY_W;
            src.h = READY_H;

            // 自分のキャラクターなら 拡大
            float scale = (playerID == my_id) ? 1.5f : 1.0f;

            dst.w = (int)(READY_W * scale);
            dst.h = (int)(READY_H * scale);
            dst.x = (int)(300 + playerID * 350 - (dst.w - READY_W) / 2);
            dst.y = (int)(readyY[playerID] - (dst.h - READY_H) / 2);

            /* =========================
            READY 状態(2行) 演出効果
            ========================= */
            if (active && readyFrame[playerID] >= READY_FRAME - 1)
            {
                SDL_SetRenderDrawBlendMode(gGame->render, SDL_BLENDMODE_ADD);

                for (int i = 0; i < 70; i++)
                {
                    // 上に行くほど広がる
                    int spread = 150 + i * 2;
                    int rx = (rand() % (spread * 2)) - spread;
                    int ry = rand() % 15;

                    int x = dst.x + dst.w / 2 + rx;
                    int y = dst.y + dst.h * 0.80f - i * 7 - ry;

                    int alpha = 140 - i * 2;
                    if (alpha < 20)
                        alpha = 20;

                    int w = 14 - i / 6;
                    if (w < 2)
                        w = 2;

                    SDL_SetRenderDrawColor(
                        gGame->render,
                        255, 230, 80,
                        alpha);

                    SDL_Rect r = {x, y, w, 8};
                    SDL_RenderFillRect(gGame->render, &r);
                }
            }

            SDL_RenderCopy(gGame->render, tex, &src, &dst);

            // =========================
            // YOU テキスト (自分のチャラの上)
            // =========================
            if (playerID == my_id && youAlpha > 0.0f)
            {
                SDL_SetTextureBlendMode(youtex, SDL_BLENDMODE_BLEND);
                SDL_SetTextureAlphaMod(youtex, (Uint8)(youAlpha * 255));

                SDL_Rect youDst;
                youDst.w = 75;
                youDst.h = 35;
                youDst.x = dst.x + dst.w / 2 - youDst.w / 2;
                youDst.y = dst.y - youDst.h - 10;

                SDL_RenderCopy(gGame->render, youtex, NULL, &youDst);
            }
        }

        ch = ch->next;
    }
    RenderWaitingUI(gGame->render);

    pthread_mutex_unlock(&pm);

    if (now - last > 80)
        last = now;
}


void WaitingRoom(GameInfo *gGame)
{
    DrawBg(gGame);
    UpdateReadyLobby(gGame);
}

void StartMenu(GameInfo *gGame)
{
    LoadStartTextures(gGame);
    InitReadySlots();

    UIPhase phase = 0; 
    int come = 0;
    int state = 0;
    int introDone = 0;

    ResetUITransition();  
    SDL_Event e;

    while (1)
    {
        SDL_RenderClear(gGame->render);
        DrawBg(gGame);

        // =========================
        // PHASE 処理
        // =========================
        switch (phase)
        {
        case 0: // START_FADEOUT (黒 → スタート画面)
            DrawBg(gGame);
            gGames[my_id].input.nowstts = NOW_READY;
            SendInput(&gGames[my_id].input);
            if (UI_Transition(gGame->render, -1))
            {
                ResetUITransition();
                phase = 1; // UI_INTRO
            }
            break;

        case 1: // UI_INTRO
            if (!IntroAnimation(gGame, come))
            {
                phase = 2;
                introDone = 1;
            }
            else
            {
                come += 80;
            }
            break;

        case 2: // UI_MENU
            DrawButtonAni(gGame, state);
            break;

        case 3: // UI_WAITING
            if (come > 0)
            {
                IntroAnimation(gGame, come);
                come -= 80;
            }
            else
            {
                WaitingRoom(gGame);
                int allReady = 1;
                for (int i = 0; i < MAX_CLIENT; i++)// debug 4人でプレイするとき　MAX_CLIENT - 3 => MAX_CLIENT
                {
                    if (latest[i].nowstts != NOW_START )
                    {
                        allReady = 0;
                        break;
                    }
                }
                if (allReady)
                {   
                    StopBGMFadeOut_Music(1800);
                    phase = 4;
                    ResetUITransition();
                }
            }
            break;

        case 4: // UI_TRANSITION
            WaitingRoom(gGame);
            
            if (UI_Transition(gGame->render, 1))
            {   
                return;
            }
            break;
        }


        // =========================
        // 入力処理
        // =========================
        while (SDL_PollEvent(&e))
        {
            if (e.type != SDL_KEYDOWN)
                continue;

            if (phase == UI_MENU && introDone)
            {
                if (e.key.keysym.sym == SDLK_w)
                    state = 0;
                if (e.key.keysym.sym == SDLK_s)
                    state = 1;

                if (e.key.keysym.sym == SDLK_SPACE)
                {
                    if (state == 0)
                    {
                        gGames[my_id].input.nowstts = NOW_START;
                        SendInput(&gGames[my_id].input);//nowsttsをseverに送信

                        if (!gGame->SE_Load[SE_PressStart]) {
                        } else {
                            Mix_PlayChannel(-1, gGame->SE_Load[SE_PressStart], 0);
                        }

                        phase = UI_WAITING;

                    }
                    else
                    {
                        exit(0);
                    }
                }
            }
        }

        SDL_RenderPresent(gGame->render);
        SDL_Delay(16);
    }
}

//===================
//Start! 文字出力
//===================
void DrawStartTEXTUI(GameInfo *g)
{
    SDL_Texture *tex = startTexttex;
    if (!tex)
        return;

    static float y;
    static int phase = 0;
    static Uint32 tick = 0;

    Uint32 now = SDL_GetTicks();

    const int texW = 440;
    const int texH = 120;

    const float startY = SCREEN_H + texH;
    const float targetY = 400.0f;
    const float endY = -texH;

    if (phase == 0)
    {
        y = startY;
        phase = 1;
    }

    if (phase == 1)
    {
        y += (targetY - y) * 0.12f;
        if (fabsf(y - targetY) < 1.0f)
        {
            y = targetY;
            phase = 2;
            tick = now;
        }
    }
    else if (phase == 2)
    {
        if (now - tick >= 1000)
            phase = 3;
    }
    else if (phase == 3)
    {
        y += (endY - y) * 0.12f;
        if (y < endY + 1.0f)
            phase = 4;
    }

    if (phase >= 4)
        return;

    SDL_Rect src = {0, 0, texW, texH};
    SDL_Rect dst = {
        (g->cam.w - texW) / 2,
        (int)y,
        texW, texH};

    SDL_RenderCopy(g->render, tex, &src, &dst);
}

#define PHASE1_TIME 200 // 下 → 中央
#define PHASE2_TIME 400 // 停止 
#define PHASE3_TIME 100 // 中央 → 上

//===================
//CountDown
//===================
void DrawCountDown(GameInfo *gGame, int value, float scale)
{
    if (!gGame->Texture_Number)
        return;

    const int charW = 48;
    const int charH = 64;

    static float y = 0.0f;
    static int phase = 0;
    static Uint32 phaseStart = 0;
    static int lastValue = -1; 

    Uint32 now = SDL_GetTicks();

    const float startY = SCREEN_H + charH;
    const float targetY = 400.0f;
    const float endY = -charH;

    // =========================
    // 数字が変わるとアニメーションがリセット
    // =========================
    if (value != lastValue) 
    {
        phase = 0;
        lastValue = value;
    }

    // =========================
    // phase 0 : 初期化
    // =========================
    if (phase == 0)
    {
        y = startY;
        phase = 1;
        phaseStart = now;
    }

    // =========================
    // phase 1 : 下 → 中央
    // =========================
    if (phase == 1)
    {
        float t = (now - phaseStart) / (float)PHASE1_TIME;
        if (t >= 1.0f)
        {
            t = 1.0f;
            phase = 2;
            phaseStart = now;
        }
        y += (targetY - y) * 0.12f;
    }
    // =========================
    // phase 2 : 停止
    // =========================
    else if (phase == 2)
    {
        y = targetY;
        if (now - phaseStart >= PHASE2_TIME)
        {
            phase = 3;
            phaseStart = now;
        }
    }
    // =========================
    // phase 3 : 中央 → 上
    // =========================
    else if (phase == 3)
    {
        float t = (now - phaseStart) / (float)PHASE3_TIME;
        if (t >= 1.0f)
        {
            phase = 4; 
            return;
        }
        y = targetY + (endY - targetY) * t * t * (3.0f - 2.0f * t);
    }

    if (phase >= 4)
        return;

    // =========================
    // 数字 DRAW
    // =========================
    char str[8];
    sprintf(str, "%d", value);
    int len = strlen(str);

    SDL_Rect src = {0, 0, charW, charH};

    int drawW = (int)(charW * scale);
    int drawH = (int)(charH * scale);

    int totalW = (int)(len * drawW * 0.8f);
    int baseX = (gGame->cam.w - totalW) / 2;

    SDL_Rect dst;
    dst.y = (int)y;
    dst.w = drawW;
    dst.h = drawH;

    for (int i = 0; i < len; i++)
    {
        int num = str[i] - '0';
        src.x = num * charW;
        dst.x = baseX + (int)(i * drawW * 0.8f);

        SDL_RenderCopy(gGame->render, gGame->Texture_Number, &src, &dst);
    }
}



static Uint32 resultStartMs = 0;
static int resultPhase = 0;

void ResetResultUI(void)
{
    resultStartMs = 0;
    resultPhase = 0;

    if (scoretex)
        SDL_SetTextureAlphaMod(scoretex, 255);
        
}


#define COL_SCORE_X  830
#define COL_DEATH_X 1100
#define COL_HIT_X   1360

static void DrawPlayerResultRow(
    GameInfo *g,
    int playerId,
    int baseY
)
{
    CharaInfo *ch = GetPlayerChara(playerId);
    if (!ch) return;

    float scale = 0.7f;

    DrawNumber(g, ch->score,      COL_SCORE_X, baseY, scale);
    DrawNumber(g, ch->deathcount, COL_DEATH_X, baseY, scale);
    DrawNumber(g, ch->hitcount,   COL_HIT_X,   baseY, scale);
}


static const int rowY[4] = {
    70,
    230,
    390,
    550
};


//===============================
//結果出力
//===============================
int DrawResultUI(GameInfo *g)
{
    static int finished = 0;

    if (!resulttex || !scoretex)
        return 1;

    const Uint32 RESULT_SWIPE_TIME = 800;
    const Uint32 SCORE_FADE_TIME  = 600;

    /* ===============================
       result / score 固定位置
       =============================== */
    SDL_Rect resultDst = {
        (SCREEN_W - 315) / 2,
        80,
        315,
        160
    };

    SDL_Rect scoreDst = {
        (SCREEN_W - 1160) / 2,
        280,
        1160,
        616
    };

    /* ===============================
       アニメーション終了後（固定出力）
       =============================== */
    if (finished)
    {
        SDL_Rect resultSrc = {0, 0, 315, 160};
        SDL_RenderCopy(g->render, resulttex, &resultSrc, &resultDst);

        SDL_SetTextureAlphaMod(scoretex, 255);
        SDL_RenderCopy(g->render, scoretex, NULL, &scoreDst);

        SDL_SetTextureAlphaMod(g->Texture_Number, 255);

        for (int i = 0; i < 4; i++)
        {
            int drawY = scoreDst.y + rowY[i];
            DrawPlayerResultRow(g, i, drawY);
        }

        return 1;
    }

    /* ===============================
       タイマーのリセット
       =============================== */
    if (resultStartMs == 0)
        resultStartMs = SDL_GetTicks();

    Uint32 elapsed = SDL_GetTicks() - resultStartMs;

    /* ===============================
       result.png スワイプ
       =============================== */
    SDL_Rect resultSrc = {0, 0, 315, 160};

    float t = (float)elapsed / RESULT_SWIPE_TIME;
    if (t > 1.0f) t = 1.0f;

    resultSrc.y = (int)((1.0f - t) * resultSrc.h);
    SDL_RenderCopy(g->render, resulttex, &resultSrc, &resultDst);

    /* ===============================
       score.png + 数字フェードイン
       =============================== */
    if (elapsed >= RESULT_SWIPE_TIME)
    {
        Uint32 scoreElapsed = elapsed - RESULT_SWIPE_TIME;
        float a = (float)scoreElapsed / SCORE_FADE_TIME;
        if (a > 1.0f) a = 1.0f;

        Uint8 alpha = (Uint8)(a * 255);

        SDL_SetTextureAlphaMod(scoretex, alpha);
        SDL_RenderCopy(g->render, scoretex, NULL, &scoreDst);

        SDL_SetTextureAlphaMod(g->Texture_Number, alpha);

        for (int i = 0; i < 4; i++)
        {
            int drawY = scoreDst.y + rowY[i];
            DrawPlayerResultRow(g, i, drawY);
        }

        SDL_SetTextureAlphaMod(g->Texture_Number, 255);

        if (a >= 1.0f)
        {
            finished = 1;
            return 1;
        }
    }

    return 0;
}
