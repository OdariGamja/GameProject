// server.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include "game.h"
#include "server_system.h"

#define PORT 5000    // サーバーポート
#define MAX_CLIENT 4 // 最大クライアント数

int DoorX;
int InTheTrain;
int csock[MAX_CLIENT];                           // 各クライアントのソケット
int alive[MAX_CLIENT];                           // クライアント接続状態
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER; // csock/alive の排他用
SDL_Rect VendorRect = {0, 0, 0, 0};
CharaInfo players[MAX_CLIENT]; // プレイヤー情報
SDL_Rect TrainPoint = {0, 0, 0, 0};
int Traped;
int downtrainx;
int Door2 = 0;

static long long gameStartTime = -1;
static SDL_bool gameStarted = SDL_FALSE;

//========================
// 指定バイト数の読み込み
//========================
ssize_t readn(int fd, void *buf, size_t n)
{
    size_t left = n;
    char *p = buf;
    while (left > 0)
    {
        ssize_t r = read(fd, p, left);
        if (r <= 0)
            return (r == 0 ? n - left : -1); // EOF またはエラー
        left -= r;
        p += r;
    }
    return n;
}

//========================
// 指定バイト数の書き込み
//========================
ssize_t writen(int fd, const void *buf, size_t n)
{
    size_t left = n;
    const char *p = buf;
    while (left > 0)
    {
        ssize_t w = write(fd, p, left);
        if (w <= 0)
            return -1; // エラー
        left -= w;
        p += w;
    }
    return n;
}

//========================
//当たり判定
//========================
void PhysicsStep(void)
{
    for (CharaInfo *p = SevergCharaHead; p; p = p->next)
    {

        // 物理演算から電車を除外(除外しないと手前の電車の座標を更新できない)
        if (p->type == CT_TRAIN || p->type == CT_TRAININSIDE || p->type == CT_TRAININSIDEDOOR || p->type == CT_IMG || p->type == CT_TRAIN_PASS)
            continue;

        if (p->stts == CS_Holded || p->stts == CS_Disable)
            continue;

        for (CharaInfo *q = p->next; q; q = q->next)
        {

            if (q->stts == CS_Holded)
                continue;

            // Fly中にVendor衝突
            if (FlyCollision(p, q))
                continue;

            if (p->stts == CS_Disable || q->stts == CS_Disable)
                continue;

            Collision(p, q);
        }

        // ===== y 制限 =====
        if (p->type == CT_COIN || p->type == CT_REDCOIN)
        {
            if (!traindoor && !p->InSubway)
            {
                //  if (p->point.y < 624.0f)
                // p->point.y = 624.0f;
            }
        }
        else if (p->type >= CT_PLAYER0 && p->type <= CT_PLAYER3)
        {
            if (!traindoor && !p->InSubway)
            {
            }
            else
            {
                if (p->point.y < 470.0f)
                    p->point.y = 470.0f;
            }
        }
    }
}

//========================
// サーバー側のプレイヤー移動
//========================
void MoveServer(CharaInfo *ch)
{

    if (ch->hitDurationMs > 0)
    {
        return;
    }

    if (ch->stts == CS_Death)
    {
        return;
    }

    if (!ch || ch->stts == CS_Disable)
        return;
    float vx = 0, vy = 0;

    if (ch->type == CT_Left_Pillar ||
        ch->type == CT_Right_Pillar)
        return;

    if (ch->type == CT_Vendor)
    {
        VendorRect = ch->Vendor_rect;
    }

    if (ch->stts == CS_FallWait || ch->stts == CS_Falling)
    {
        UpdateFall(ch, fallRect);
        return;
    }

    
    if (ch->type == CT_IMG)
    {
        return;
    }

    // =====================
    // 先に「飛んでいる物」の物理更新（放物線）
    // =====================
    if (ch->stts == CS_Fly)
    {
        float dt = TICK_MS / 1000.0f;

        // 重力
        ch->vel.y += THROW_GRAVITY * dt;

        printf("velx %f\n ", ch->vel.x);
        // 位置更新
        ch->point.x += ch->vel.x * dt;
        ch->point.y += ch->vel.y * dt;
        if (ch->holedindubway == 1)
        {
            if (ch->point.x <= uptrainx + 1000)
            {
                ch->point.x = uptrainx + 1000;
            } // 7550
            if (ch->point.x >= uptrainx + 7000)
            {
                ch->point.x = uptrainx + 7000;
            }
        }

        // rect 追従
        ch->rect.x = (int)ch->point.x;
        ch->rect.y = (int)ch->point.y;

        // ----- ✨ 着地判定（あなた仕様） -----
        // 投げた瞬間に計算された "この投げの着地点"
        float targetY = ch->throwLandY;

        printf("throwLand y %f\n", ch->throwLandY);
        // 下向きに到達するか、上向きに上限を超えたら着地
        // （Up でも Down でも Left/Right でも、targetY を跨いだら着地）
        if ((ch->vel.y > 0 && ch->point.y >= targetY) ||
            (ch->vel.y < 0 && ch->point.y <= targetY))
        {

            // 位置を着地点に固定
            ch->point.y = targetY;
            ch->rect.y = (int)targetY;

            // 停止
            ch->vel.x = 0;
            ch->vel.y = 0;

            Door2 =
                (ch->point.x >= 1725 && ch->point.x <= 1900) ||
                (ch->point.x >= 2515 && ch->point.x <= 2690) ||
                (ch->point.x >= 3794 && ch->point.x <= 3970) ||
                (ch->point.x >= 4970 && ch->point.x <= 5140) ||
                (ch->point.x >= 6260 && ch->point.x <= 6430) ||
                (ch->point.x >= 7045 && ch->point.x <= 7230);

            if (ch->holedindubway == 1)
            {
                if (ch->point.x <= uptrainx + 1000)
                {
                    ch->point.x = uptrainx + 1000;
                } // 7550
                if (ch->point.x >= uptrainx + 7000)
                {
                    ch->point.x = uptrainx + 7000;
                }
            }
            if (!(uptrainx + 505 <= ch->point.x && ch->point.x <= uptrainx + 7405))
            {
                if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
                {
                    if (ch->point.y < 624.0f)
                        ch->point.y = 624.0f;
                }
            }

            if (!traindoor)
            {
                if (uptrainx + 505 <= ch->point.x && ch->point.x <= uptrainx + 7405)
                { // 動いてる電車の壁
                    if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
                    {
                        if (ch->point.y < 624.0f)
                            ch->point.y = 624.0f;
                    }
                    else if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                    {
                        if (ch->point.y < 560)
                            ch->point.y = 560;
                    }
                }
            }
            else if (traindoor)
            {
                if (uptrainx + 505 <= ch->point.x && ch->point.x <= uptrainx + 7405)
                {
                    if (!Door2)
                    {

                        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                        {
                            if (ch->point.y < 560)
                            {
                                ch->point.y = 560;
                            }
                        }
                    }
                    if (!Door2 && (ch->holedindubway == 1))
                    {
                        if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
                        {
                            if (ch->point.y > 594)
                                ch->point.y = 594;
                        }
                        else if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                        {
                            if (ch->point.y < 470)
                                ch->point.y = 470;
                        }

                        if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
                        {
                            if (ch->point.y > 594)
                                ch->point.y = 594;
                        }
                        else if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                        {
                            if (ch->point.y > 480)
                                ch->point.y = 480;
                        }
                    }
                }
            }

            // ここ重要
            if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
            {
                ch->stts = CS_Knockdown;
                ch->knockLastMs = now_ms();
                printf("[KNOCKDOWN RESUME] remain=%lldms\n", ch->knockRemainMs);
            }
            else
            {
                ch->stts = CS_Normal;
            }
            ch->holedindubway = 0;

            printf("[FLY] y=%.1f target=%.1f vel.y=%.1f\n",
                   ch->point.y, ch->throwLandY, ch->vel.y);
        }
        // ----- ここまで -----

        UpdateAttackRects(ch);
        return;
    }
    if (ch->stts == CS_Holded)
    {
        if (ch->holder->InSubway)
        {
            ch->InSubway = SDL_TRUE;
        }
        else
        {
            ch->InSubway = SDL_FALSE;
        }
    }

    if (ch->type == CT_MOUSE)
    {
        float speed = 1.2f;

        ch->point.x += speed;

    } // 敵１のMOUSEを動かす処理　今は右に進むだけ
    if (ch->stts != CS_Knockdown)
    {

        long long now = now_ms();

        int isAttacking =
            (ch->lastAttackMs != 0) &&
            (now - ch->lastAttackMs < 200); // 　攻撃アニメーション
        // printf("isAttacking %d\n", isAttacking);

        // =====================
        // 攻撃
        // =====================
        if (ch->input.spacePressed)
        {
            ch->input.spacePressed = 0;

            long long now = now_ms();

        
            // =========================
            // NORMAL 状態
            // =========================


    if (ch->inthemovesubway == 0){
            
        if (ch->stts == CS_Normal)
        {
            // 何も持っていない→攻撃
            if (ch->hold == 0)
            {

                if (!isAttacking &&
                    now - ch->lastAttackMs >= 200)
                {

                    Attack(ch);
                    ch->lastAttackMs = now;
                    isAttacking = 1;
                    switch (ch->playerdir)
                    {
                    case Right:
                        ch->movestts = ANI_RightAttack;
                        break;
                    case Left:
                        ch->movestts = ANI_LeftAttack;
                        break;
                    case Down:
                        ch->movestts = ANI_DownAttack;
                        break;
                    case Up:
                        ch->movestts = ANI_UpAttack;
                        break;
                    }
                }
            }
            else
            {
                ThrowObject(ch);
            }
            
        }

        // =========================
        // HOLDED 状態
        // =========================
        else if (ch->stts == CS_Holded)
        {
            ch->inputcount++;
            if (ch->inputcount >= 10)
                ReleaseHold(ch->holder, ch);

        }  
        }
    }

        // =====================
        // 移動処理 (攻撃中でない時だけ)
        // =====================
        if (!ch->hitBySinkansen && !ch->hitByTrain && !isAttacking &&
            ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
        {

            if (ch->stts == CS_Knockdown)
            {
                return;
            }
            float PlayerSpeedScale = 1.2f;
            ch->movestts = ANI_Stop;

            if (ch->input.right)
            {
                vx += PlayerSpeedScale * MOVE_SPEED;
                ch->movestts = ANI_RunRight;
                ch->playerdir = Right;
            }
            else if (ch->input.left)
            {
                vx -= PlayerSpeedScale * MOVE_SPEED;
                ch->movestts = ANI_RunLeft;
                ch->playerdir = Left;
            }
            else if (ch->input.down)
            {
                if (!ch->railroadflag)
                {
                    vy += MOVE_SPEED;
                    ch->movestts = ANI_RunDown;
                    ch->playerdir = Down;
                }
            }
            else if (ch->input.up)
            {
                if (!ch->railroadflag)
                {
                    vy -= MOVE_SPEED;
                    ch->movestts = ANI_RunUp;
                    ch->playerdir = Up;
                }
            }
        }
    }

    if(ch->type == CT_COIN || ch->type == CT_REDCOIN){
        if(ch->InSubway && ch->point.x > 9000){
            ch->stts = CS_Disable;
        }   
    }

    if (ch->stts == CS_Holded)
        return;

    // ===============================
    // y軸ベースの速度調整
    // yが小さいほど遅くなる
    // ===============================
    float minY = 350.0f;
    float maxY = 1100.0f;
    float speedScale;

    if (ch->point.y <= minY)
        speedScale = 0.5f;
    else if (ch->point.y >= maxY)
        speedScale = 5.0f;
    else
    {
        float t = (ch->point.y - minY) / (maxY - minY);
        speedScale = 0.05f + 1.5f * t * t * (5.f - 0.05f);
    }

    vy *= 0.35 * speedScale;

    // 斜め移動補正
    if (vx && vy)
    {
        vx /= sqrtf(2);
        vy /= sqrtf(2);
    }
    ch->vx = vx;
    ch->vy = vy;
    ch->point.x += vx;
    ch->point.y += vy;

    // --- 持っている物を追従させる（heldTarget を使う） ---
    if (ch->hold == 1 && ch->holdTarget)
    {
        CharaInfo *o = ch->holdTarget;
        if (o->stts == CS_Holded && o->holder == ch)
        {
            int pw = ch->rect.w;
            int ow = o->rect.w;
            int oh = o->rect.h;
            // 持ち上げ量を切り替える

            // 対象がプレイヤーなら、もっと上に
            if (o->type >= CT_PLAYER0 && o->type <= CT_PLAYER3)
            {
                oh = o->rect.h + 80;
            }
            float ox;

            if (o->type >= CT_PLAYER0 && o->type <= CT_PLAYER3)
            {
                ox = 10 + (ch->point.x + (pw - ow) / 2.0f);
            }
            else if (o->type == CT_COIN || o->type == CT_REDCOIN)
            {

                ox = 20 + (ch->point.x + (pw - ow) / 2.0f);
            }
            else
                ox = (ch->point.x + (pw - ow) / 2.0f);

            float oy = ch->point.y - oh;

            // 追従（瞬間スナップ）
            o->point.x = ox;
            o->point.y = oy;

            // rect はプレイヤー描画基準に合わせる
            o->rect.x = (int)ox;
            o->rect.y = (int)oy;
            ch->rect.x = (int)ch->point.x;

            if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
            {
                ch->rect.y = 150 + (int)ch->point.y; // player
            }
            else if (ch->type == CT_COIN|| ch->type == CT_REDCOIN )
            {
                ch->rect.y = 48 + (int)ch->point.y; // coin
            }
            else if (ch->type == CT_Vendor)
            {
                ch->rect.y = Vendor_ConstNumber + (int)ch->point.y;
            }
            else if (ch->type == CT_ATM)
            {
                ch->rect.y = ATM_ConstNumber + (int)ch->point.y;
            }
            else
            {
                ch->rect.y = (int)ch->point.y;
            }

            UpdateAttackRects(o);
        }
    }

    // マップ外に出ないように制限
        if (ch->point.x < 620)
            ch->point.x = 620;

        if(ch->hitBySinkansen || ch->InSubway){
            
            if (ch->point.x > 9000)
            ch->point.x = 9000;
        }else if (ch->point.x > 8450 && !(ch -> type == CT_REDCOIN  || ch -> type == CT_COIN  )){
            ch->point.x = 8450;
        }

    // 線路上のとき電車と重ならないように
    if (ch->railroadflag)
    {
        float left = uptrainx + 500;
        float right = uptrainx + 7450;

        if (ch->point.x > left && ch->point.x < right)
        {
            if (ch->point.x < (left + right) * 0.5f)
                ch->point.x = left;
            else
                ch->point.x = right;
        }
    }

    DoorX =
        (ch->point.x >= 1725 && ch->point.x <= 1900) ||
        (ch->point.x >= 2515 && ch->point.x <= 2690) ||
        (ch->point.x >= 3794 && ch->point.x <= 3970) ||
        (ch->point.x >= 4972 && ch->point.x <= 5140) ||
        (ch->point.x >= 6260 && ch->point.x <= 6430) ||
        (ch->point.x >= 7045 && ch->point.x <= 7230);

    if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
    {
        InTheTrain =
            (ch->point.x >= uptrainx + 505 && ch->point.x <= uptrainx + 7305) &&
            (ch->point.y >= 460 && ch->point.y <= 540);
    }
    else if (ch->type >= CT_COIN || ch->type >= CT_REDCOIN)
    {
        InTheTrain =
            (ch->point.x >= uptrainx + 505 && ch->point.x <= uptrainx + 7305) &&
            (ch->point.y >= 540 && ch->point.y <= 615);
    }

    InTheRail =
        (ch->point.y >= 400 && ch->point.y <= 530);

    if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
    {
        UpdateTrainRailFall(ch);
    }

    if (ch->railroadflag &&
        ch->point.x <= uptrainx + 7450 && ch->point.x >= uptrainx + 7200 &&
        ch->point.y >= 400 && ch->point.y <= 600 && !ch->InSubway && ch->stts != CS_Fly && !traindoor)
    {
        StartHitByTrain(ch);
    }

    if (InTheTrain)
    {
        ch->InSubway = SDL_TRUE;
    }
    else
    {
        ch->InSubway = SDL_FALSE;
    }

    if (traindoor)
    {
        if (!DoorX && ch->InSubway)
        {

            ch->notinfrontofdoor = 1;
        }
        else
        {
            ch->notinfrontofdoor = 0;
        }
    }
    else
    {
        if (ch->InSubway)
        {
            ch->notinfrontofdoor = 1;
        }
        else
        {
            ch->notinfrontofdoor = 0;
        }
    }

    if (traindoor)
    { // open
        Traped = 0;
        int OnTrainArea =
            (uptrainx + 505 <= ch->point.x && ch->point.x <= uptrainx + 7405);

        if (OnTrainArea && !ch->railroadflag)
        {
            if (DoorX && !InTheTrain)
            {
                if (ch->point.y < 480)
                    ch->point.y = 480;

                if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
                {
                    if (ch->point.y < 552)
                        ch->point.y = 552;
                }
                else if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                {
                    if (ch->point.y < 480)
                        ch->point.y = 480;
                }
            }
            if (DoorX && InTheTrain)
            {
                if (ch->point.y < 470)
                    ch->point.y = 470;
            }
            if (!DoorX && InTheTrain)
            {
                if (ch->point.y < 470)
                    ch->point.y = 470;

                if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
                {
                    if (ch->point.y > 610)
                        ch->point.y = 610;
                }
                if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                {
                    if (ch->point.y > 530)
                        ch->point.y = 530;
                }
            }
            if (!DoorX && !InTheTrain)
            {
                if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
                {
                    if (ch->point.y < 624.0f)
                        ch->point.y = 624.0f;
                }
                else if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                {
                    if (ch->point.y < 560)
                        ch->point.y = 560;
                }
            }

            if (ch->InSubway)
            {

                if (ch->point.x <= uptrainx + 1000)
                {
                    ch->point.x = uptrainx + 1000;
                } // 7550
                if (ch->point.x >= uptrainx + 7000)
                {
                    ch->point.x = uptrainx + 7000;
                }
            }
        }
        else
        {
            if (ch->point.y >= 480 && ch->point.y <= 540)
            {
                if ((ch->point.x >= uptrainx + 800) && (ch->point.x <= uptrainx + 900))
                {
                    ch->point.x = uptrainx + 900;
                } // 7550
                if ((ch->point.x <= uptrainx + 7450) && (ch->point.x >= uptrainx + 7350))
                {
                    ch->point.x = uptrainx + 7450;
                }
            }
            if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
            {
                if (ch->point.y < 624.0f)
                    ch->point.y = 624.0f;
            }
        }
    }
    else
    { // close
        if (ch->InSubway)
        {
            if ((ch->type == CT_COIN || ch->type == CT_REDCOIN) || (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3))
            {

                TrapInTrain(ch);
                Traped = 1;
            }
        }
        else
        {
            if (!ch->railroadflag)
            {
                // 動いてる電車の壁
                if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
                {
                    if (ch->point.y < 624.0f)
                        ch->point.y = 624.0f;
                }
                if (uptrainx + 505 <= ch->point.x && ch->point.x <= uptrainx + 7405)
                {
                    if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
                    {
                        if (ch->point.y < 560)
                            ch->point.y = 560;
                    }
                }
            }

            Traped = 0;
        }
    }

    if (ch->hitByTrain && ch->stts != CS_Fly)
    {
        ch->point.x = uptrainx + 7400;
    }
    if (ch->point.x > 8700)
    {
        KillPlayer(ch);
    }

    if (downtrainx <= ch->point.x && ch->point.x <= downtrainx + 13737)
    { // 下側の電車
        if (ch->point.y >= 990)
        {
            ch->point.y = 990;
        }
    }

    if (!Traped)
    {
        if (ch->point.x + ch->rect.w > MAP_Width)
            ch->point.x = MAP_Width - ch->rect.w;
    }
    if (ch->point.y + ch->rect.h > MAP_Height)
        ch->point.y = MAP_Height - ch->rect.h;

    // rect の位置を更新
    ch->rect.x = (int)ch->point.x;
    if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
    {
        ch->rect.y = 150 + (int)ch->point.y; // player だけ+150
    }
    else if (ch->type == CT_COIN)
    { // coin だけ +48
        ch->rect.y = 48 + (int)ch->point.y;
    }
    else if (ch->type == CT_REDCOIN)
    { // coin だけ +48
        ch->rect.y = 48 + (int)ch->point.y;
    }
    else if (ch->type == CT_Vendor)
    {
        ch->rect.y = Vendor_ConstNumber + (int)ch->point.y; 
    }
    else if (ch->type == CT_ATM)
    {
        ch->rect.y = ATM_ConstNumber + (int)ch->point.y;
    }
    else if (ch->type == CT_Lever)
    {
        ch->rect.y = 800;
    }else if (ch->type == CT_BENCH){
        ch->rect.h = 48;
        ch->rect.y = ch->point.y + 48;
    }
    else
    {
        ch->rect.y = (int)ch->point.y;
    }

    UpdateAttackRects(ch);
}

//========================
// 現在時刻（ミリ秒）取得
//========================
long long now_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

//========================
// クライアント受信スレッド
//========================
void *RecvThread(void *arg)
{
    int id = *(int *)arg;
    free(arg);
    int s = csock[id];

    while (1)
    {
        Keystts inp;

        if (readn(s, &inp, sizeof(inp)) <= 0)
            break;

        CharaInfo *ch = &players[id];

        ch->input.up = inp.up;
        ch->input.down = inp.down;
        ch->input.left = inp.left;
        ch->input.right = inp.right;

        // 押した瞬間だけ true
        if (inp.space && !ch->input.spacePrev)
        {
            ch->input.spacePressed = 1;
        }

        // 前回状態を保存
        ch->input.spacePrev = inp.space;

        // 現在状態
        ch->input.space = inp.space;

        //  START状態処理
        if (inp.nowstts == NOW_START &&
            ch->input.nowstts != NOW_START)
        {
            ch->input.nowstts = NOW_START;
            printf("[SERVER] Player %d START pressed\n", id);
        }

        //  INGAME状態処理
        if (inp.nowstts == NOW_INGAME &&
            ch->input.nowstts != NOW_INGAME)
        {
            ch->input.nowstts = NOW_INGAME;
            printf("[SERVER] Player %d START pressed\n", id);
        }
    }

    // disconnect
    pthread_mutex_lock(&mtx);
    close(csock[id]);
    csock[id] = -1;
    alive[id] = 0;
    pthread_mutex_unlock(&mtx);

    printf("Client %d disconnected\n", id);
    return NULL;
}

//========================
// ゲームループスレッド
//========================
void *GameLoop(void *arg)
{
    long long last = now_ms();

    while (1)
    {
        long long cur = now_ms();
        if (cur - last < TICK_MS)
        {
            usleep((TICK_MS - (cur - last)) * 1000);
            continue;
        }
        last = cur;


        if (!gameStarted && AllPlayersStarted())//クライアント全員接続完了
        {
            gameStarted = SDL_TRUE;
            gameStartTime = now_ms();
        }

        long long gameTime = 0;
        if (gameStarted)
        {
            gameTime = now_ms() - gameStartTime;//ゲーム時間開始
        }

        for (int i = 0; i < MAX_CLIENT; i++)
        {
            players[i].gameTime = gameTime;//playersパックに入れてclientに送信
        }

        PhysicsStep();//当たり判定

        for (CharaInfo *ch = SevergCharaHead; ch; ch = ch->next)
        {

            if ((ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3) ||
                ch->type == CT_MOUSE ||
                ch->type == CT_TRAININSIDE ||
                ch->type == CT_Vendor ||
                ch->type == CT_COIN ||
                ch->type == CT_ATM ||
                ch->type == CT_REDCOIN ||
                ch->type == CT_IMG ||
                ch->type == CT_Left_Pillar ||
                ch->type == CT_Right_Pillar ||
                ch->type == CT_Lever ||
                ch->type == CT_BENCH )
            {
                MergeSinkansenPlayerPoint(ch);//新幹線とあたったとき座標更新
            
                MoveServer(ch);//移動計算
                UpdateHitByTrain(ch); // 列車衝突後の死亡処理

                if(ch -> railroadflag ){//線路上の場合、物体を落とす
                    ReleaseHold(ch, ch->holdTarget);
                }
                // hitDurationMsにおいてのmovestts計算
                if (ch->hitDurationMs > 0)
                {
                    long long now = now_ms();
                    if (now - ch->hitStartMs >= ch->hitDurationMs)
                    {
                        ch->hitDurationMs = 0;

                        // 元の状態に戻る
                        if (ch->movestts != ANI_KNOCKDOWN)
                        {
                            ch->movestts = ANI_Stop;
                        }
                    }
                }
            }

            if (ch->type == CT_Vendor)//自販機
            {
                UpdateVendor(ch);
            }
            if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
            {
                UpdateKnockdown(ch);

                //状態別アニメーション更新
                if (ch->stts == CS_Knockdown || ch->stts == CS_Holded || ch->stts == CS_Fly)
                {
                    ch->movestts = ANI_KNOCKDOWN;
                    if (ch->hitByTrain)
                    {
                        ch->movestts = ANI_SPIN;
                    }
                }
                if (ch->stts == CS_FallWait)
                {
                    ch->movestts = ANI_RunDown;
                }
                if (ch->stts == CS_Falling)
                {
                    ch->movestts = ANI_SPIN;
                }
                if (ch->holdTarget)
                {
                    switch (ch->playerdir)
                    {
                    case Right:
                        ch->movestts = ANI_RAISE_RIGHT;
                        break;
                    case Left:
                        ch->movestts = ANI_RAISE_LEFT;
                        break;
                    case Down:
                        ch->movestts = ANI_RAISE_DOWN;
                        break;
                    case Up:
                        ch->movestts = ANI_RAISE_UP;
                        break;
                    default:
                        break;
                    }
                }

                RespawnCheck(ch);
                UpdateInvincible(ch);
            }
            if (ch->type == CT_ATM)//ATMロジック
                UpdateATM(ch);
            if (ch->type == CT_TRAIN)//電車座標更新
            {
                uptrainx = ch->point.x;
            }
            if (ch->type == CT_TRAIN_PASS)//手前の電車座標更新
            {
                downtrainx = ch->point.x;
            }
            if (ch->type == CT_Lever)
            {
                UpdateBlackout(ch);//停電
            }
            if(ch->type == CT_COIN){//Coin Respawn
                RespawnDisabledCoin();
                RespawnDisabledCoin2();
            }

            CheckDeathZone(ch, fallRect2);//DeathZone Check


            if (!(ch->type == CT_Left_Pillar ||
                  ch->type == CT_Right_Pillar ||
                  ch->type == CT_Lever ||
                  ch->type == CT_BENCH))
            {
                UpdateScaleServer(ch);
            }
        }

        int train_x = 0, train_y = 0;
        CharaStts trainstts = ANI_TrainDoorCLOSE; // アニメーション
        // 奥の電車
        for (CharaInfo *ch = SevergCharaHead; ch; ch = ch->next)
        {
            if (ch->type == CT_TRAIN && gameTime != 0)
            {
                long long cur = now_ms() + 1000;
                TrainMovement(ch, cur);
                train_x = ch->point.x;
                train_y = ch->point.y;
                trainstts = ch->movestts; // アニメーション
                break;
            }
        }

        // 手前の電車
        for (CharaInfo *ch = SevergCharaHead; ch; ch = ch->next)
        {
            if (ch->type == CT_TRAIN_PASS && gameTime != 0)
            {
                TrainMovement_Pass(ch);
                break;
            }
        }

        // 新幹線jpeg
        for (CharaInfo *ch = SevergCharaHead; ch; ch = ch->next)
        {
            if (ch->type == CT_Arrival_Board && gameTime != 0)
            {   
                DisplayMovement(ch);
            }
        }

        // 2) 内部オブジェクトをすべて電車の位置に同期
        for (CharaInfo *ch = SevergCharaHead; ch; ch = ch->next)
        {
            if (ch->type == CT_TRAININSIDEDOOR || ch->type == CT_TRAININSIDE)
            {
                ch->point.x = train_x;
                ch->point.y = train_y;
                ch->movestts = trainstts; // アニメーション
            }
        }

        for (CharaInfo *ch = SevergCharaHead; ch; ch = ch->next)
        {
            UpdateHoleFall(ch);

        }

        // クライアントに送る座標パケット作成
        NetPack pack[MAX_CLIENT];
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            pack[i].x = players[i].point.x;
            pack[i].y = players[i].point.y;
            pack[i].score = players[i].score;
            pack[i].hitcount = players[i].hitcount;
            pack[i].deathcount = players[i].deathcount;
            pack[i].Movestts = players[i].movestts;
            pack[i].InSubway = players[i].InSubway;
            pack[i].direction = players[i].playerdir;
            pack[i].whoholder = players[i].whoholder;
            pack[i].visible = players[i].stts;
            pack[i].railroadflag = players[i].railroadflag;
            pack[i].hitByTrain = players[i].hitByTrain;
            pack[i].hitBySinkansen = players[i].hitBySinkansen;
            pack[i].nowstts = players[i].input.nowstts;
            pack[i].invincibleflag = players[i].invincibleflag;
            pack[i].gameTime = players[i].gameTime;
            pack[i].IntheHole = players[i].IntheHole;
    
        }

        // player 以外オブゼット情報パケット
        NetPack objectPack[MAX_OBJECTS];
        int objectIndex = 0;
        for (CharaInfo *ch = SevergCharaHead; ch && objectIndex < MAX_OBJECTS; ch = ch->next)
        {
            if (ch->type == CT_COIN || ch->type == CT_TRAIN ||
                ch->type == CT_MOUSE || ch->type == CT_REDCOIN || ch->type == CT_TRAININSIDE ||
                ch->type == CT_TRAININSIDEDOOR || ch->type == CT_Vendor ||
                ch->type == CT_ATM || ch->type == CT_TRAIN_PASS || ch->type == CT_IMG || ch->type == CT_Arrival_Board ||
                ch->type == CT_Left_Pillar || ch->type == CT_Right_Pillar || ch->type == CT_Lever || ch->type == CT_BENCH )
            {

                objectPack[objectIndex].x = ch->point.x;
                objectPack[objectIndex].y = ch->point.y;
                objectPack[objectIndex].rect.x = ch->rect.x;
                objectPack[objectIndex].rect.y = ch->rect.y;
                objectPack[objectIndex].visible = ch->stts;
                objectPack[objectIndex].InSubway = ch->InSubway;
                objectPack[objectIndex].Movestts = ch->movestts;
                objectPack[objectIndex].whoholder = ch->whoholder;
                objectPack[objectIndex].railroadflag = ch->railroadflag;
                objectPack[objectIndex].IntheHole = ch->IntheHole;
                objectIndex++;

            }
        }

        // 送信
        pthread_mutex_lock(&mtx);
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            if (alive[i] && csock[i] != -1)
            {

                if (writen(csock[i], pack, sizeof(pack)) < 0)
                {
                    close(csock[i]);
                    csock[i] = -1;
                    alive[i] = 0;
                    continue; // 次のクライアントへ
                }

                if (writen(csock[i], objectPack, sizeof(objectPack)) < 0)
                {
                    close(csock[i]);
                    csock[i] = -1;
                    alive[i] = 0;
                    continue;
                }
            }
        }
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

//========================
// メイン
//========================
int main()
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    // ソケットオプション（アドレス再利用）
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(s, (struct sockaddr *)&addr, sizeof(addr));
    listen(s, MAX_CLIENT);
    printf("Server ready.\n");

    // プレイヤー初期化
    for (int i = 0; i < MAX_CLIENT; i++)
    {
        csock[i] = -1;
        alive[i] = 0;
        memset(&players[i].input, 0, sizeof(players[i].input));
    }

    // NPC/プレイヤー情報を linked list に初期化
    InitServerChara("position.data", players, MAX_CLIENT);

    // ゲームループスレッド作成
    pthread_t gl;
    pthread_create(&gl, NULL, GameLoop, NULL);
    pthread_detach(gl);

    // クライアント接続待ち
    while (1)
    {
        int cs = accept(s, NULL, NULL);
        pthread_mutex_lock(&mtx);

        // 空きID検索
        int id = -1;
        for (int i = 0; i < MAX_CLIENT; i++)
            if (!alive[i])
            {
                id = i;
                break;
            }

        if (id < 0)
        {
            close(cs);
            pthread_mutex_unlock(&mtx);
            continue;
        } // 空きなし

        csock[id] = cs;
        alive[id] = 1;

        // クライアントにプレイヤーIDを伝達
        writen(cs, &id, sizeof(int));

        pthread_mutex_unlock(&mtx);

        printf("Client %d connected\n", id);

        // 受信スレッド作成
        int *pid = malloc(sizeof(int));
        *pid = id;
        pthread_t th;
        pthread_create(&th, NULL, RecvThread, pid);
        pthread_detach(th);
    }
    return 0;
}