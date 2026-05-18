// window.c
#include "game.h"
#include <SDL2/SDL_image.h>
#include <stdlib.h>
#include <sys/time.h>

extern CharaInfo *gCharaHead;

long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

//========================
// ウインドウ初期化
//========================
int InitWindow(GameInfo *gGame, const char *title, const char *bg_file, int width, int height)
{
    printf("[INIT] InitWindow() called\n");

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG)
        return PrintError("failed to initialize SDL_image");

    gGame->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     width, height,SDL_WINDOW_FULLSCREEN_DESKTOP); // SDL_WINDOW_FULLSCREEN_DESKTOP

    printf("[INIT] window=%p\n", gGame->window);

    gGame->render = SDL_CreateRenderer(gGame->window, -1, SDL_RENDERER_ACCELERATED);
    printf("[INIT] renderer=%p\n", gGame->render);

    //背景の読み込み
    SDL_Surface *surf = IMG_Load(bg_file);
    printf("[INIT] background surface=%p (%s)\n", surf, bg_file);

    gGame->bg = SDL_CreateTextureFromSurface(gGame->render, surf);
    SDL_FreeSurface(surf);
    printf("[INIT] background texture=%p\n", gGame->bg);

    // =========================
    // wall (固定背景)
    // =========================
    SDL_Surface *surf_wall = IMG_Load("img/wall.png");
    if (!surf_wall)
    {
        printf("[ERROR] IMG_Load failed (wall.png): %s\n", IMG_GetError());
    }
    else
    {
        gGame->wall = SDL_CreateTextureFromSurface(gGame->render, surf_wall);
        SDL_FreeSurface(surf_wall);
        printf("[INIT] wall texture=%p\n", gGame->wall);
    }

    // UIの読み込み
    SDL_Surface *surf_UI = IMG_Load("img/CharaUI.png");
    printf("[INIT] background surface=%p (%s)\n", surf_UI, "img/CharaUI.png");

    gGame->Texture_UI = SDL_CreateTextureFromSurface(gGame->render, surf_UI);
    gGame->image_src_UI.w = surf_UI->w;
    gGame->image_src_UI.h = surf_UI->h;
    gGame->image_src_UI.x = 0;         // 初期化
    gGame->image_src_UI.y = WD_Height; // 初期位置
    gGame->ui_y = WD_Height;
    gGame->ui_target_y = WD_Height - gGame->image_src_UI.h;
    gGame->ui_speed = 400.0f; // px/sec 

    SDL_FreeSurface(surf_UI);
    printf("[INIT] background texture=%p\n", gGame->Texture_UI);

    //---- 数字画像の読み込み ----
    SDL_Surface *surf_Num = IMG_Load("img/number.png");
    if (!surf_Num)
    {
        printf("Failed to load number image: %s\n", IMG_GetError());
    }
    else
    {
        gGame->Texture_Number = SDL_CreateTextureFromSurface(gGame->render, surf_Num);
        SDL_FreeSurface(surf_Num);
        printf("[INIT] Number texture loaded\n");
    }

    SDL_Surface *surf_space = IMG_Load("img/space.png");
    if (!surf_space)
    {
        printf("Failed to load space image: %s\n", IMG_GetError());
    }
    else
    {
        gGame->space = SDL_CreateTextureFromSurface(gGame->render, surf_space);
        SDL_FreeSurface(surf_space);
        printf("[INIT] space texture loaded\n");
    }


    // タイマー背景のロード 
    SDL_Surface *tbSrc = IMG_Load("img/Time_Board.png");
    if (!tbSrc)
    {
        printf("[ERROR] Failed to load Time_Board.png: %s\n", IMG_GetError());
    }
    else
    {
        gGame->Texture_TimeBoard = SDL_CreateTextureFromSurface(gGame->render, tbSrc);
        SDL_FreeSurface(tbSrc);
        if (!gGame->Texture_TimeBoard)
        {
            printf("[ERROR] Failed to create texture from Time_Board.png: %s\n", SDL_GetError());
        }
    }

    // キャラクター ロード
    for (int i = 0; i < CHARATYPE_NUM; i++)
    {
        printf("\n[INIT] LOADING CHARACTER TYPE %d\n", i);

        SDL_Surface *s = IMG_Load(gCharaType[i].path);
        if (!s)
            return PrintError("failed to load character image");

        printf("[INIT] character surface=%p path=%s size=%d x %d\n",
               s, gCharaType[i].path, s->w, s->h);

        gCharaType[i].img = SDL_CreateTextureFromSurface(gGame->render, s);

        printf("[INIT] gCharaType[%d].img=%p\n", i, gCharaType[i].img);

        int fullW = s->w;
        int fullH = s->h;
        int frameW = gCharaType[i].w;
        int frameH = gCharaType[i].h;

        gCharaType[i].aninum.x = fullW / frameW;
        gCharaType[i].aninum.y = fullH / frameH;

        printf("[INIT] Frames: %d x %d (frame size %d x %d)\n",
               gCharaType[i].aninum.x, gCharaType[i].aninum.y,
               frameW, frameH);

        SDL_FreeSurface(s);
    }

    gGame->cam.x = 0;
    gGame->cam.y = 0;
    gGame->cam.w = WD_Width;
    gGame->cam.h = WD_Height;

    printf("[INIT] Camera initialized (%d,%d,%d,%d)\n",
           gGame->cam.x, gGame->cam.y, gGame->cam.w, gGame->cam.h);

    return 0;
}

int GetDrawPriority(CharaInfo *ch)
{
    // 数字が小さいほど先に描画
    if (ch->type == CT_TRAININSIDE)
        return 0;

    // 地下鉄の中+Holdされていないコイン
    if ((ch->type == CT_COIN && ch->InSubway == SDL_TRUE) || (ch->type == CT_REDCOIN && ch->InSubway == SDL_TRUE))
        return 1;

    if (ch->type == CT_TRAININSIDEDOOR)
        return 2;

    if (ch->type == CT_TRAIN)
        return 3;
    if (ch->railroadflag == 1)
        return 7;

    if (ch->stts == CS_Holded || ch->stts == CS_Fly)
        return 12;

    if (ch->type == CT_TRAIN_PASS)
        return 13;

    if (ch->type == CT_Arrival_Board) // 電光掲示板を一番手前に描画
        return 14;

    // 基本キャラクター(プレイヤー、NPCなど)
    return 10;
}

void FixHoldDrawOrder(CharaInfo **arr, int n)
{
    for (int i = 0; i < n; i++)
    {
        CharaInfo *holder = arr[i];

        // プレイヤーのみ holder になれる
        if (holder->type < CT_PLAYER0 || holder->type > CT_PLAYER3)
            continue;

        int holderId = holder->type - CT_PLAYER0 + 1;

        // holder の直後に held を移動
        for (int j = 0; j < n; j++)
        {
            CharaInfo *held = arr[j];

            if (held->whoholder != holderId)
                continue;

            // すでに直後ならOK
            if (j == i + 1)
                break;

            // j > i の場合：一度取り出して i+1 に差し込む
            CharaInfo *tmp = held;
            for (int k = j; k > i + 1; k--)
            {
                arr[k] = arr[k - 1];
            }
            arr[i + 1] = tmp;
            break;
        }
    }
}

static Uint32 blackoutStartTick = 0;
static int blackoutStarted = 0;


//================
//停電関数
//================
void DrawBlackOut(SDL_Renderer *r, int screenW, int screenH)
{

    if (!blackoutStarted)
    {
        blackoutStarted = 1;
        blackoutStartTick = SDL_GetTicks();
    }

    Uint32 now = SDL_GetTicks();
    float t = (now - blackoutStartTick) / 1000.0f; // seconds

    int alpha = 0;

    if (t < 0.7f)
    {
        // ステップ1: 初期 早い点滅
        alpha = (int)(fabs(sin(t * 25.0f)) * 180);
    }
    else if (t < 2.0f)
    {
        // ステップ2:点滅しながら暗転に
        float flicker = fabs(sin(t * 15.0f));
        float fade = (t - 0.7f) / 1.3f; // 0~1
        alpha = (int)((120 + flicker * 80) * fade);
    }
    else if (t < 18.0f)
    {
        //  3段階: 完全に停電
        alpha = 230;
    }
    else if (t < 20.0f)
    {
        //   ステップ4:復旧直前点滅
        float flicker = fabs(sin(t * 20.0f));
        float fadeOut = 1.0f - (t - 18.0f) / 2.0f; // 1 → 0
        alpha = (int)((150 + flicker * 80) * fadeOut);
    }
    else
    {
        // 停電終了 → 状態リセット
        blackoutStarted = 0;
        return;
    }

    if (alpha > 255)
        alpha = 255;
    if (alpha < 0)
        alpha = 0;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, alpha);

    SDL_Rect full = {0, 0, screenW, screenH};
    SDL_RenderFillRect(r, &full);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}



//================
//space.pngの出力
//================
void DrawSpaceImage(GameInfo *gGame)
{
    if (!gGame || !gGame->space) return;

    static Uint32 lastTick = 0;
    static int frame = 0;

    Uint32 now = SDL_GetTicks();
    if (now - lastTick > 50) {
        frame ^= 1;
        lastTick = now;
    }

    SDL_Rect src = { frame * 88, 0, 88, 47 };

    float sx, sy;
    SDL_RenderGetScale(gGame->render, &sx, &sy);

    //固定スケール
    SDL_RenderSetScale(gGame->render, 1.0f, 1.0f);

    //画面中央の座標
    int centerX = (gGame->cam.w / 2) + 50;
    int centerY = (gGame->cam.h / 2) + 50;

    SDL_Rect dst = {
        centerX - 44,  
        centerY - 24,   
        88,
        47
    };

    SDL_RenderCopy(gGame->render, gGame->space, &src, &dst);
    SDL_RenderSetScale(gGame->render, sx, sy);
}


//================
//respawnのとき点滅
//================
void UpdateInvincibleBlink(CharaInfo *ch)
{
    if (!ch->invincibleflag)
    {
        ch->invincibleBlinkTimer = 0;
        ch->invincibleVisible = SDL_TRUE;
        return;
    }

    // 無敵状態の時だけ点滅するように
    if (ch->invincibleBlinkTimer == 0)
    {
        ch->invincibleBlinkTimer = now_ms();
        ch->invincibleVisible = SDL_TRUE;
        return;
    }

    Uint32 now = now_ms();
    // 100ms毎に点滅
    if (now - ch->invincibleBlinkTimer >= 30)
    {
        ch->invincibleBlinkTimer = now;
        ch->invincibleVisible = !ch->invincibleVisible;
    }
}

// =======================
// 数字を描画する関数 
// gGame: ゲーム情報
// value: 描画したい数値（スコア）
// x, y:  描画開始位置（左上）
// scale: 拡大率（1.0fで等倍）
//==========================
void DrawNumber(GameInfo *gGame, int value, int x, int y, float scale)
{
    if (!gGame->Texture_Number)
        return;

    // 画像内の1文字分のサイズ（自作PNGに合わせて調整）
    int charW = 48;
    int charH = 64;

    // 数値を文字列に変換
    char str[16];
    sprintf(str, "%d", value);
    int len = strlen(str);

    SDL_Rect src;
    SDL_Rect dst;

    src.w = charW;
    src.h = charH;
    src.y = 0; // 横一列ならyは0

    dst.w = (int)(charW * scale);
    dst.h = (int)(charH * scale);
    dst.y = y;

    for (int i = 0; i < len; i++)
    {
        // 文字 '0' からのオフセットで数字を特定
        int num = str[i] - '0';

        src.x = num * charW; // png内の数字を charW の数値ずつずらして表示

        dst.x = x + (int)(i * (charW * scale * 0.8)); // 文字間を詰める

        SDL_RenderCopy(gGame->render, gGame->Texture_Number, &src, &dst);
    }
}


//========================
// ゲーム描画
//========================
void DrawGame(GameInfo *gGame)
{

    CharaInfo *arr[128];
    int n = 0;
    static int endInterval_played = 0;

    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next)
    {

        arr[n] = ch;


        n++;
    }

    // y値を基準に昇順整列
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {

            CharaInfo *a = arr[i];
            CharaInfo *b = arr[j];

            int pa = GetDrawPriority(a);
            int pb = GetDrawPriority(b);

            // ① 優先順位を先に
            if (pa != pb)
            {
                if (pa > pb)
                {
                    CharaInfo *tmp = arr[i];
                    arr[i] = arr[j];
                    arr[j] = tmp;
                }
                continue;
            }

            // ② 同じ優先順位+コイン↔プレイヤーのみ例外
            if (a->type == CT_COIN &&
                b->type >= CT_PLAYER0 && b->type <= CT_PLAYER3)
            {

                float yi = a->point.y + a->rect.h;
                float yj = b->point.y + b->rect.h;

                if (yi > yj - 10)
                {
                    CharaInfo *tmp = arr[i];
                    arr[i] = arr[j];
                    arr[j] = tmp;
                }
                continue;
            }
            else if (b->type == CT_COIN &&
                     a->type >= CT_PLAYER0 && a->type <= CT_PLAYER3)
            {

                float yi = a->point.y + a->rect.h;
                float yj = b->point.y + b->rect.h;

                if (yj > yi - 10)
                {
                    CharaInfo *tmp = arr[i];
                    arr[i] = arr[j];
                    arr[j] = tmp;
                }
                continue;
            }

            // ③ その他は一般的y整列
            // float yi = a->point.y + a->rect.h;
            // float yj = b->point.y + b->rect.h;
            ////////////////////////////////////////////////////////////////
            // ↓奥行きの倍率を当たり判定に適応
            float yMin = 500.0f;
            float yMax = 1200.0f;
            float scaleMin = 0.5f;
            float scaleMax = 1.8f;

            float drawYa = a->point.y;

            if (drawYa < 560.0f)
                drawYa = 560.0f;

            float ta = (drawYa - yMin) / (yMax - yMin);
            if (ta < 0)
                ta = 0;
            if (ta > 1)
                ta = 1;
            float scaleA = scaleMin + ta * (scaleMax - scaleMin);

            float drawYb = b->point.y;

            if (drawYb < 560.0f)
                drawYb = 560.0f;

            float tb = (drawYb - yMin) / (yMax - yMin);
            if (tb < 0)
                tb = 0;
            if (tb > 1)
                tb = 1;
            float scaleB = scaleMin + tb * (scaleMax - scaleMin);

            float yi = a->point.y + a->rect.h * scaleA;
            float yj = b->point.y + b->rect.h * scaleB;

            if ((a->type == CT_Right_Pillar || a->type == CT_Left_Pillar) && !(b->type == CT_Right_Pillar || b->type == CT_Left_Pillar))
            {

                if (yi > yj - 250)
                {
                    CharaInfo *tmp = arr[i];
                    arr[i] = arr[j];
                    arr[j] = tmp;
                }
                continue;
            }
            else if ((b->type == CT_Right_Pillar || b->type == CT_Left_Pillar) && !(a->type == CT_Right_Pillar || a->type == CT_Left_Pillar))
            {

                if (yi > yj - 250)
                {
                    CharaInfo *tmp = arr[i];
                    arr[i] = arr[j];
                    arr[j] = tmp;
                }
                continue;
            }
            else if (yi > yj)
            {
                CharaInfo *tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }

    SDL_RenderClear(gGame->render);

    // ==============================
    // WALL 
    // ==============================
    SDL_Rect camRect = {
        gGame->cam.x,
        gGame->cam.y,
        gGame->cam.w,
        gGame->cam.h};

    SDL_Rect dstBg = {
        0, 0,
        gGame->cam.w,
        gGame->cam.h};

    SDL_RenderCopy(gGame->render, gGame->wall, &camRect, &dstBg);

    // ==============================
    // TRAIN
    // ==============================
    for (int i = 0; i < n; i++)
    {
        if (arr[i]->type != CT_TRAININSIDE)
            continue;

        CharaInfo *ch = arr[i];
        SDL_Rect dst = {
            (int)(ch->point.x - gGame->cam.x),
            (int)(ch->point.y - gGame->cam.y),
            (int)ch->rect.w,
            (int)ch->rect.h};
        SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &dst);
    }

    // INSIDE TRAIN
    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next)
    {
        if (ch->InSubway == SDL_TRUE)
        {
            for (int i = 0; i < n; i++)
            {
                CharaInfo *ch = arr[i];
                if (!((ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3) ||
                      (ch->type == CT_COIN && ch->InSubway) ||
                      (ch->type == CT_ATM) ||
                      (ch->type == CT_COIN && ch->stts == CS_Holded) ||
                      (ch->type == CT_REDCOIN && ch->InSubway) ||
                      (ch->type == CT_REDCOIN && ch->stts == CS_Holded)))
                    continue;

                if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                {
                    if (ch->IntheHole)
                    {
                        continue;
                    }
                }
                if ((ch->type == CT_COIN || ch->type == CT_REDCOIN) && ch->IntheHole)
                {
                    continue;
                }

                float yMin = 500.0f;
                float yMax = 1200.0f;
                float scaleMin = 0.5f;
                float scaleMax = 1.8f;

                float drawY = ch->point.y;
                if (drawY < 560.0f)
                    drawY = 560.0f;

                float t = (drawY - yMin) / (yMax - yMin);
                if (t < 0)
                    t = 0;
                if (t > 1)
                    t = 1;
                float scale = scaleMin + t * (scaleMax - scaleMin);

                SDL_Rect dst = {
                    (int)(ch->point.x - gGame->cam.x),
                    (int)(ch->point.y - gGame->cam.y),
                    (int)(ch->rect.w * scale),
                    (int)(ch->rect.h * scale)};

                SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &dst);
            }
        }
    }

    for (int i = 0; i < n; i++)
    {
        if (arr[i]->type != CT_TRAININSIDEDOOR)
            continue;

        CharaInfo *ch = arr[i];
        SDL_Rect dst = {
            (int)(ch->point.x - gGame->cam.x),
            (int)(ch->point.y - gGame->cam.y),
            (int)ch->rect.w,
            (int)ch->rect.h};
        SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &dst);
    }

    for (int i = 0; i < n; i++)
    {
        if (arr[i]->type != CT_TRAIN)
            continue;

        if (gGame->player->point.y > 543)
        {
            CharaInfo *ch = arr[i];
            SDL_Rect dst = {
                (int)(ch->point.x - gGame->cam.x),
                (int)(ch->point.y - gGame->cam.y),
                (int)ch->rect.w,
                (int)ch->rect.h};
            SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &dst);
        }
        else
            continue;
    }

    // ==============================
    // 線路に落ちたプレイヤーをmapより後ろに
    // ==============================
    for (int i = 0; i < n; i++)
    {
        CharaInfo *ch = arr[i];

        if (!(ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3) && (ch->type == CT_COIN || ch->type == CT_REDCOIN))
            continue;

        if (ch->railroadflag != 1)
            continue;

        if (ch->stts == CS_Death)
            continue;

        float yMin = 500.0f;
        float yMax = 1200.0f;
        float scaleMin = 0.5f;
        float scaleMax = 1.8f;

        float drawY = ch->point.y;
        if (drawY < 560.0f)
            drawY = 560.0f;

        float t = (drawY - yMin) / (yMax - yMin);
        if (t < 0)
            t = 0;
        if (t > 1)
            t = 1;
        float scale = scaleMin + t * (scaleMax - scaleMin);

        SDL_Rect dst = {
            (int)(ch->point.x - gGame->cam.x),
            (int)(ch->point.y - gGame->cam.y),
            (int)(ch->rect.w * scale),
            (int)(ch->rect.h * scale)};

        SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &dst);
    }

    for (int i = 0; i < n; i++)
    {
        CharaInfo *ch = arr[i];
        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
        {
            if (!ch->IntheHole)
                continue;
        }

        if (!(ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3))
        {
            continue;
        }
        if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
        {
            if (!ch->IntheHole)
                continue;
        }
        float yMin = 800.0f;
        float yMax = 1200.0f;
        float scaleMin = 0.5f;
        float scaleMax = 1.0f;

        float drawY = ch->point.y;
        if (drawY < 560.0f)
            drawY = 560.0f;

        float t = (drawY - yMin) / (yMax - yMin);
        if (t < 0)
            t = 0;
        if (t > 1)
            t = 1;

        t = 1.0f - t; 
        float scale = scaleMin + t * (scaleMax - scaleMin);

        SDL_Rect dst = {
            (int)(ch->point.x - gGame->cam.x),
            (int)(ch->point.y - gGame->cam.y),
            (int)(ch->rect.w * scale),
            (int)(ch->rect.h * scale)};

        SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &dst);
    }

    // ==============================
    // 背景描画
    // ==============================
    SDL_RenderCopy(gGame->render, gGame->bg, &camRect, &dstBg);

    for (int i = 0; i < n; i++)
    { // mapより上のやつら
        CharaInfo *ch = arr[i];
        if (ch->type == CT_TRAIN ||
            ch->type == CT_TRAININSIDE ||
            ch->type == CT_TRAININSIDEDOOR)
            continue;

        if (ch->type == CT_MOUSE && ch->stts != CS_Normal)
            continue;

        if ((ch->type == CT_COIN || ch->type == CT_REDCOIN) && ch->InSubway == SDL_TRUE)
        {
            continue;
        }
        if ((ch->type == CT_COIN || ch->type == CT_REDCOIN) && ch->IntheHole)
        {
            continue;
        }
        

        if ((ch->type == CT_COIN || ch->type == CT_REDCOIN) && ch->stts == CS_Disable)
            continue;

        if ((ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3) &&
            ch->railroadflag == 1 && ch->point.y <= 700)
            continue;

        if(ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3){
            if(ch->stts == CS_Death)
            continue;
        }

        if (ch->invincibleflag && !ch->invincibleVisible)
        {
            // 無敵点滅
            continue;
        }

        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
        {
            if (ch->IntheHole)
                continue;
        }

        if ((ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3))
        {
            if (ch->InSubway == SDL_TRUE)
            {
                continue;
            }
            if (ch->stts == CS_Death)
                continue;
        }

        if (ch->type == CT_Arrival_Board) // 電光掲示板の奥行きによるサイズ変更の無効化
            continue;

        float yMin = 500.0f;
        float yMax = 1200.0f;
        float scaleMin = 0.5f;
        float scaleMax = 1.8f;

        float drawY = ch->point.y;
        if (drawY < 560.0f)
            drawY = 560.0f;

        float t = (drawY - yMin) / (yMax - yMin);
        if (t < 0)
            t = 0;
        if (t > 1)
            t = 1;
        float scale = scaleMin + t * (scaleMax - scaleMin);

        // 手前電車だけ大きく
        if (ch->type == CT_TRAIN_PASS)
        {
            scale *= 3.0f; // 調節可能
        }
        if (ch->type == CT_Left_Pillar ||
            ch->type == CT_Right_Pillar ||
            ch->type == CT_Lever)
        {
            scale = 1.0f;
        }
        SDL_Rect dst = {
            (int)(ch->point.x - gGame->cam.x),
            (int)(ch->point.y - gGame->cam.y),
            (int)(ch->rect.w * scale),
            (int)(ch->rect.h * scale)};

        SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &dst);
    }

    // Y座標によって倍率を変えないキャラクタの描画
    for (int i = 0; i < n; i++)
    {
        CharaInfo *ch = arr[i];

        if (ch->type == CT_Arrival_Board)
        {
            SDL_Rect ArrivalBoard_Dst = {
                (1920 - ch->rect.w) / 2, // 1980はウィンドウの幅
                ch->point.y,
                (int)(ch->rect.w),
                (int)(ch->rect.h)};
            SDL_RenderSetScale(gGame->render, 1.0f, 1.0f);
            SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &ArrivalBoard_Dst);
        }
    }

    //=============================================
    //操作キャラクターのUIを描画
    //=============================================
    // カメラの倍率を無効化

    SDL_RenderSetScale(gGame->render, 1.0f, 1.0f);
    SDL_RenderCopy(gGame->render, gGame->Texture_UI, NULL, &gGame->image_src_UI);

    //=============================================
    // スコア表示 
    //=============================================
    int uiTopY = gGame->image_src_UI.y;
    int uiBaseY = uiTopY + (gGame->image_src_UI.h - 80);

    int startX = 260;
    int gapX = 483;

    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next)
    {
        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
        {
            int pid = ch->type - CT_PLAYER0;
            int drawX = startX + (pid * gapX);

            DrawNumber(gGame, ch->score, drawX, uiBaseY, 0.8f);
        }
    }




    //=============================================
    // 残り時間表示 (5分カウントダウン)
    //=============================================

    // 左上の表示位置　
    int timeX = 80;
    int timeY = 40;

    //タイマー看板の描画
    if (gGame->Texture_TimeBoard)
    {
        SDL_Rect boardRect;
        // 画像の元のサイズを取得
        SDL_QueryTexture(gGame->Texture_TimeBoard, NULL, NULL, &boardRect.w, &boardRect.h);

        // 位置調整: タイマー文字(timeX, timeY)より少し左上に配置
        // ※座標やスケール(0.8fなど)は後から調整可
        boardRect.x = timeX - 80;                // この数値だと
        boardRect.y = timeY - 48;                // ピッタリ
        boardRect.w = (int)(boardRect.w * 1.0f); // 現在はサイズそのまま
        boardRect.h = (int)(boardRect.h * 1.0f);

        SDL_RenderCopy(gGame->render, gGame->Texture_TimeBoard, NULL, &boardRect);
    }
     
    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next)
    {
        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
        {
            int remainSec = GAME_LIMIT_SEC - (int)(ch->gameTime / 1000);
            if (remainSec < 0)
                remainSec = 0;

            int min = remainSec / 60;
            int sec = remainSec % 60;

            if (!min && sec <= 5)
            {
                // カウントダウン中 → 左上に時間表示なし
                DrawCountDown(gGame, sec, 3.8f);

                if(sec == 0 && !endInterval_played){
                    Mix_PlayChannel(-1, gGames[my_id].SE_Load[SE_EndInterval], 0);
                    endInterval_played = 1;
                }else if(sec != 0){
                    endInterval_played = 0;
                }
            }
            else
            {
                // 通常時刻表示
                DrawNumber(gGame, min, timeX, timeY, 0.8f);

                if (sec < 10)
                {
                    DrawNumber(gGame, 0, timeX + 60, timeY, 0.8f);
                    DrawNumber(gGame, sec, timeX + 90, timeY, 0.8f);
                }
                else
                {
                    DrawNumber(gGame, sec, timeX + 60, timeY, 0.8f);
                }
            }
        }
    }
    

}

//========================
// アニメーション更新
//========================
void UpdateAnimation(GameInfo *gGame, float dt)
{
    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next)
    {
        // movestts 変更検出
        if (ch->movestts != ch->prevMovestts)
        {
            ch->ani.x = 0;
            ch->aniTimer = 0;
            ch->animPlayed = 0;
            ch->prevMovestts = ch->movestts;
        }

        int maxX = ch->entity->aninum.x;

        /* ------------------------------------------------------
           1) movesttsによるani.y(縦行)設定
           ------------------------------------------------------ */

        // プレイヤーのアニメーション
        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
        {
            switch (ch->movestts)
            {

            case ANI_Stop:
                switch (ch->playerdir)
                {
                case Down:
                    ch->ani.y = 0;
                    break;
                case Right:
                    ch->ani.y = 1;
                    break;
                case Left:
                    ch->ani.y = 2;
                    break;
                case Up:
                    ch->ani.y = 3;
                    break;
                }
                break;

            case ANI_RunRight:
            case ANI_RunLeft:
            case ANI_RunDown:
            case ANI_RunUp:

                switch (ch->playerdir)
                {
                case Down:
                    ch->ani.y = 4;
                    break;
                case Right:
                    ch->ani.y = 5;
                    break;
                case Left:
                    ch->ani.y = 6;
                    break;
                case Up:
                    ch->ani.y = 7;
                    break;
                }
                break;

            case ANI_DownAttack:
                ch->ani.y = 8;
                break;
            case ANI_RightAttack:
                ch->ani.y = 9;
                break;
            case ANI_LeftAttack:
                ch->ani.y = 10;
                break;
            case ANI_UpAttack:
                ch->ani.y = 11;
                break;

            case ANI_HIT_DOWN:
                ch->ani.y = 12;
                break;
            case ANI_HIT_RIGHT:
                ch->ani.y = 13;
                break;
            case ANI_HIT_LEFT:
                ch->ani.y = 14;
                break;
            case ANI_HIT_UP:
                ch->ani.y = 15;
                break;

            case ANI_KNOCKDOWN:
                ch->ani.y = 16;
                break;

            case ANI_RAISE_DOWN:
                ch->ani.y = 17;
                break;
            case ANI_RAISE_RIGHT:
                ch->ani.y = 18;
                break;
            case ANI_RAISE_LEFT:
                ch->ani.y = 19;
                break;
            case ANI_RAISE_UP:
                ch->ani.y = 20;
                break;

            case ANI_SPIN:
                ch->ani.y = 21;
                break;
            default:
                break;
            }
        }
        else if (ch->type == CT_ATM)
        {
            switch (ch->movestts)
            {
            case ANI_ATM_LIGHTON_DEFALT:
                ch->ani.y = 0;
                break;

            case ANI_ATM_LIGHTOFF_DEFALT:
                ch->ani.y = 1;
                break;

            case ANI_ATM_LIGHTOFF_VIBRATE:
                ch->ani.y = 2;
                break;

            default:
                ch->ani.y = 0;
                break;
            }
        }
        else if (ch->type == CT_COIN)
        {
            switch (ch->stts)
            {
            case CS_Fly:
                ch->ani.y = 1;
                break;
            default:
                ch->ani.y = 0;
                break;
            }
        }
        else if (ch->type == CT_REDCOIN)
        {
            switch (ch->stts)
            {
            case CS_Fly:
                ch->ani.y = 1;
                break;
            default:
                ch->ani.y = 0;
                break;
            }
        }
        else if (ch->type == CT_Vendor)
        {
            switch (ch->movestts)
            {
            case ANI_Stop:
                ch->ani.y = 0;
                break;
            case ANI_VENDER_PUMP:
                ch->ani.y = 1;
                break;
            default:
                ch->ani.y = 0;
                break;
            }
        }
        else if (ch->type == CT_Lever)
        {
            switch (ch->movestts)
            {
            case ANI_BlackOutOn:
                ch->ani.y = 1;
                break;
            default:
                ch->ani.y = 0;
                break;
            }
        }
        else
        { // coinなどのオブゼット　（後で修正）今は任意でプレイヤーと同じくする
            switch (ch->movestts)
            {
            case ANI_Stop:
                ch->ani.y = 0; // 停止
                break;
            case ANI_RunRight:
                ch->ani.y = 0; // 右
                break;
            case ANI_RunLeft:
                ch->ani.y = 0; // 左　
                break;
            case ANI_RunDown:
                ch->ani.y = 0; // 下
                break;
            case ANI_RunUp:
                ch->ani.y = 0; // 上
                break;
            case ANI_RightAttack:
                ch->ani.y = 0;
                break;
            case ANI_LeftAttack:
                ch->ani.y = 0;
                break;
            case ANI_UpAttack:
                ch->ani.y = 0;
                break;
            case ANI_DownAttack:
                ch->ani.y = 0;
                break;
            default:
                break;
            }
        }

        ch->aniTimer += dt;

        float frameInterval = 0.05f;

        if (ch->type == CT_ATM)
            frameInterval = 0.005f;
        if (ch->type == CT_Vendor)
            frameInterval = 0.1f;

        //  攻撃アニメーションフレーム速度
        if (ch->movestts == ANI_DownAttack ||
            ch->movestts == ANI_RightAttack ||
            ch->movestts == ANI_LeftAttack ||
            ch->movestts == ANI_UpAttack)
        {
            frameInterval = 0.025f;
        }
        // 被撃アニメーションフレーム速度
        if (ch->movestts == ANI_HIT_RIGHT ||
            ch->movestts == ANI_HIT_LEFT ||
            ch->movestts == ANI_HIT_DOWN ||
            ch->movestts == ANI_HIT_UP)
        {
            frameInterval = 0.025f;
        }

        if (ch->movestts == ANI_SPIN)
        {
            frameInterval = 0.005f;
        }

        if (ch->aniTimer >= frameInterval) // 0.05秒ごとにフレーム++
        {
            if (ch->type == CT_TRAIN || ch->type == CT_TRAININSIDEDOOR)
            { // y軸にフレイムを回すオブゼット
                if (ch->movestts == ANI_TrainDoorOPEN)
                {
                    if (ch->ani.y < 1512)
                        ch->ani.y += 504;

                    if (ch->ani.y > 1512)
                        ch->ani.y = 1512;
                }
                else if (ch->movestts == ANI_TrainDoorCLOSE)
                {
                    ch->ani.y -= 504;
                    if (ch->ani.y <= 0)
                        ch->ani.y = 0;
                }
                ch->aniTimer = 0;
            }
            else if (ch->type == CT_ATM)
            {
                switch (ch->movestts)
                {
                // 点灯：8フレーム常時ループ
                case ANI_ATM_LIGHTON_DEFALT:
                    ch->ani.x++;
                    if (ch->ani.x >= 8)
                        ch->ani.x = 0;
                    break;

                // 消灯した振動：1回だけ再生
                case ANI_ATM_LIGHTOFF_VIBRATE:
                    if (ch->ani.x < 7)
                        ch->ani.x++;
                    else
                        ch->ani.x = 0; // 終わったら0固定
                    break;

                // 電灯が消える 基本停止
                case ANI_ATM_LIGHTOFF_DEFALT:
                default:
                    ch->ani.x = 0;
                    break;
                }

                ch->aniTimer = 0;
            }
            else if (ch->type == CT_Vendor)
            {
                switch (ch->movestts)
                {
                case ANI_VENDER_PUMP:
                    if (ch->ani.x < 7)
                    {
                        ch->ani.x++;
                        ch->movestts = ANI_VENDER_PUMP;
                    }
                    else
                    {
                        ch->ani.x = 0;
                        ch->movestts = ANI_Stop;
                    }
                    break;
                default:
                    ch->ani.x++;
                    if (ch->ani.x >= maxX)
                        ch->ani.x = 0;

                    ch->aniTimer = 0;
                    break;
                }
            }
            else if (ch->type == CT_Arrival_Board)
            { // 掲示板のスプライト更新
                switch (ch->movestts)
                {
                case ANI_Board_LightOn:
                    ch->ani.y = 1;
                    break;
                default: // ANI_Board_LightOffだけ
                    ch->ani.y = 0;
                    break;
                }
            }
            else
            { // x軸アニメーション（プレイヤー含む）

                // =========================
                // 攻撃 被撃　アニメーション(1回のみ)
                // =========================
                if (ch->movestts == ANI_DownAttack ||
                    ch->movestts == ANI_RightAttack ||
                    ch->movestts == ANI_LeftAttack ||
                    ch->movestts == ANI_UpAttack ||
                    ch->movestts == ANI_HIT_RIGHT ||
                    ch->movestts == ANI_HIT_LEFT ||
                    ch->movestts == ANI_HIT_DOWN ||
                    ch->movestts == ANI_HIT_UP)
                {
                    if (!ch->animPlayed)
                    {
                        if (ch->ani.x < maxX - 1)
                        {
                            ch->ani.x++;
                        }
                        else
                        {
                            ch->ani.x = maxX - 1; // 最後のフレームで停止
                            ch->animPlayed = 1;
                        }
                    }
                }

                // =========================
                // その他（Run、Idle等はループ）
                // =========================
                else
                {
                    ch->ani.x++;
                    if (ch->ani.x >= maxX)
                        ch->ani.x = 0;
                }

                ch->aniTimer = 0;
            }
        }

        /* ------------------------------------------------------
           3) スプライトシート座標計算
           ------------------------------------------------------ */
        int frameW = ch->entity->w;
        int frameH = ch->entity->h;

        ch->imgsrc.x = ch->ani.x * frameW;
        if (ch->type == CT_TRAIN || ch->type == CT_TRAININSIDEDOOR)
        { // trainだけframeH を使わない
            ch->imgsrc.y = ch->ani.y;
        }
        else
        {
            ch->imgsrc.y = ch->ani.y * frameH;
        }
        ch->imgsrc.w = frameW;
        ch->imgsrc.h = frameH;
    }

    // 画面下部からUIが現れる
    if (gGame->ui_y > gGame->ui_target_y)
    {
        gGame->ui_y -= gGame->ui_speed * dt;

        if (gGame->ui_y < gGame->ui_target_y)
            gGame->ui_y = gGame->ui_target_y;
    }

    gGame->image_src_UI.y = (int)gGame->ui_y;
}

//========================
// カメラアップデート
//========================
void UpdateCamera(GameInfo *gGame, CharaInfo *player)
{
    if (!gGame || !player)
        return;

    float px = player->point.x + player->rect.w / 2;
    float py = player->point.y + player->rect.h / 2;

    // =====================
    // ズーム計算
    // =====================
    float yMin = 200.0f;
    float yMax = 800.0f;
    float zoomMin = 1.0f;
    float zoomMax = 2.5f;

    float t = (py - yMin) / (yMax - yMin);
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;

    float zoom = zoomMax - t * (zoomMax - zoomMin);

    SDL_RenderSetScale(gGame->render, zoom, zoom);

    // =====================
    // カメラ位置
    // =====================
    float targetX = px - (gGame->cam.w / 2) / zoom;
    float targetY = py - (gGame->cam.h / 2) / zoom;

    // =====================
    //  画面の揺れ
    // =====================
    if (gGame->shakeTime > 0.0f)
    {
        float dx = ((rand() % 200) / 100.0f - 1.0f) * gGame->shakeStrength;
        float dy = ((rand() % 200) / 100.0f - 1.0f) * gGame->shakeStrength;

        targetX += dx;
        targetY += dy;

        gGame->shakeTime -= 1.0f / 60.0f;
        if (gGame->shakeTime < 0)
            gGame->shakeTime = 0;
    }

    // =====================
    // マップ境界
    // =====================
    if (targetX < 650)
        targetX = 650;
    if (targetY < 0)
        targetY = 0;

    if (targetX > (MAP_Width - gGame->cam.w / zoom) - 1700)
        targetX = (MAP_Width - gGame->cam.w / zoom) - 1700;
    if (targetY > MAP_Height - gGame->cam.h / zoom)
        targetY = MAP_Height - gGame->cam.h / zoom;

    gGame->cam.x = (int)targetX;
    gGame->cam.y = (int)targetY;
}

// 画面振動関数
void StartScreenShake(GameInfo *gGame, float strength, float duration)
{
    gGame->shakeStrength = strength;
    gGame->shakeTime = duration;
}

//========================
// 終了処理
//========================
void CloseWindow(GameInfo *gGame)
{
    printf("[CLOSE] CloseWindow()\n");

    if (!gGame)
        return;

    if (gGame->bg)
        printf("[CLOSE] Destroying background\n"), SDL_DestroyTexture(gGame->bg);

    if (gGame->Texture_UI)
        printf("[CLOSE] Destroying background\n"), SDL_DestroyTexture(gGame->Texture_UI);

    // 数字テクスチャの破棄
    if (gGame->Texture_Number)
        printf("[CLOSE] Destroying Number texture\n"), SDL_DestroyTexture(gGame->Texture_Number);
    //-------------------------------

    // タイマー背景テクスチャの破棄
    if (gGame->Texture_TimeBoard)
        SDL_DestroyTexture(gGame->Texture_TimeBoard);

    for (int i = 0; i < CHARATYPE_NUM; i++)
    {
        if (gCharaType[i].img)
        {
            printf("[CLOSE] Destroy char texture %d\n", i);
            SDL_DestroyTexture(gCharaType[i].img);
        }
    }

    if (gGame->render)
        printf("[CLOSE] Destroy renderer\n"), SDL_DestroyRenderer(gGame->render);
    if (gGame->window)
        printf("[CLOSE] Destroy window\n"), SDL_DestroyWindow(gGame->window);

    IMG_Quit();
}
