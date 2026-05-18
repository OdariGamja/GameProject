// system_server.c
#include "server_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <sys/time.h>

SDL_Rect fallRect = {
    .x = 1746,
    .y = 671,
    .w = 800,
    .h = 350}; // 死ぬとこ

SDL_Rect fallRect2 = {
    .x = -100,
    .y = 1190.0,
    .w = 10000,
    .h = 350}; // 死ぬとこ(下側)

RespawnPoint gRespawnPoints[4] = {
    {4800, 800}, // Player0
    {5000, 800}, // Player1
    {5200, 800}, // Player2
    {5400, 800}, // Player3
}; // 生き返る場所

static long long lastCoinRespawnMs = 0; 
static long long lastCoinRespawnMs2 = 0;


// 初期化
int InitServerChara(const char *position_data_file, CharaInfo players[], int max_players)
{
    int ret = 0;
    SevergCharaHead = NULL;

    FILE *fp = fopen(position_data_file, "r");
    if (!fp)
        return PrintError("failed to open position data file.");

    char linebuf[MAX_LINEBUF];

    while (fgets(linebuf, MAX_LINEBUF, fp))
    {
        if (linebuf[0] == '#')
            continue;

        unsigned int type;
        float px, py;
        int rx, ry, rw, rh;

        if (7 != sscanf(linebuf, "%u%f%f%d%d%d%d", &type, &px, &py, &rx, &ry, &rw, &rh))
        {
            PrintError("failed to parse line in position data");
            continue;
        }

        if (type >= CT_PLAYER0 && type < CT_PLAYER0 + max_players)
        {
            // プレイヤーの初期化
            int i = type - CT_PLAYER0;

            players[i].type = type;
            players[i].point.x = px;
            players[i].point.y = py;
            players[i].rect.x = (int)px;
            players[i].rect.y = 150 + (int)py;
            players[i].rect.w = rw;
            players[i].rect.h = rh;
            players[i].Baserect.w = rw;
            players[i].Baserect.h = rh;
            players[i].playerdir = Down;
            players[i].stts = CS_Normal;
            players[i].movestts = ANI_Stop;
            players[i].hold = 0;
            players[i].holder = NULL;
            players[i].holdTarget = NULL;
            players[i].hp = 3;
            players[i].inputcount = 0;
            players[i].whoholder = 0;
            players[i].invincibleflag = 0;
            players[i].railroadflag = 0;
            players[i].hitByTrain = 0;
            players[i].hitBySinkansen = 0;

            memset(&players[i].input, 0, sizeof(players[i].input));
            UpdateAttackRects(&players[i]);

            // linked list に追加
            players[i].next = SevergCharaHead;
            SevergCharaHead = &players[i];
        }
        else
        {
            // その他のNPCなど
            CharaInfo *ch = malloc(sizeof(CharaInfo));
            if (!ch)
            {
                ret = PrintError("failed to allocate NPC");
                break;
            }

            ch->type = type;
            ch->point.x = px;
            ch->point.y = py;
            ch->rect.x = (int)px;
            ch->rect.y = (int)ry;
            if (ch->type == CT_Right_Pillar)
            {
                printf("server init ch->rect.y %d\n", ch->rect.y);
            }
            ch->rect.w = rw;
            ch->rect.h = rh;
            if (ch->type == CT_Vendor)
            {
                ch->Vendor_rect.x = ch->point.x - 30; // 自販機のゴール矩形
                ch->Vendor_rect.y = ch->point.y;
                ch->Vendor_rect.w = ch->rect.w + 60;
                ch->Vendor_rect.h = ch->rect.h + 245; // コイン投げて入れるとき違和感合ったらここ変えて
            }
            if (ch->type == CT_ATM)
            {
                ch->movestts = ANI_ATM_LIGHTON_DEFALT;
            }
            ch->Baserect.w = rw;
            ch->Baserect.h = rh;
            ch->stts = CS_Normal;
            if (ch->type == CT_COIN && ch->point.x < -1000)
            {

                ch->stts = CS_Disable;
            }
            if (ch->type == CT_REDCOIN && ch->point.x < -1000)
            {

                ch->stts = CS_Disable;
            }

            ch->vel.x = ch->vel.y = 0;
            ch->ani.x = 0;
            ch->entity = NULL; 
            ch->holder = NULL;
            ch->holdTarget = NULL;
            ch->coinRespawnTime = 0;
            ch->whoholder = 0;
            ch->hp = 0;

            if (ch->type == CT_MOUSE)
            {
                ch->hp = 5;
            }

            ch->next = SevergCharaHead;
            SevergCharaHead = ch;
        }
    }

    fclose(fp);
    return ret;
}

void KillPlayer(CharaInfo *ch)
{
    if (!ch)
        return;
    if (ch->stts == CS_Death)
        return;

    ch->stts = CS_Death;
    ch->deathcount ++;
    ch->inthemovesubway = 0;
    ch->deadAtMs = now_ms();

    if (ch->hold && ch->holdTarget)
    {
        ReleaseHold(ch, ch->holdTarget);
    }
}

void CheckDeathZone(CharaInfo *ch, SDL_Rect fRect)
{
    if (!ch)
        return;

    SDL_Rect inter;
    if (!SDL_IntersectRect(&ch->rect, &fRect, &inter))
        return;

    float interArea = (float)(inter.w * inter.h);
    float playerArea = (float)(ch->rect.w * ch->rect.h);

    float ratio = interArea / playerArea;

    if (ratio >= 0.8f)
    {

        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
        {
            if (ch->stts == CS_Normal || ch->stts == CS_Knockdown)
            {
                ch->stts = CS_FallWait;
                ch->fallStartMs = now_ms();
                ch->fallVy = 0.0f;
            }
        }
        else if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
        {
            ch->stts = CS_Disable;
        }
    }
}

void UpdateFall(CharaInfo *ch, SDL_Rect fallRect)
{
    long long now = now_ms();

    // ① 落下前の一瞬停止
    if (ch->stts == CS_FallWait)
    {
        if (now - ch->fallStartMs >= FALL_WAIT_MS)
        {
            ch->stts = CS_Falling;
            ch->fallVy = 0.0f;
        }
        return;
    }

    // ② 落下中
    if (ch->stts == CS_Falling)
    {
        ch->fallVy += 0.6f; // 重力
        ch->point.y += ch->fallVy;
        ch->rect.y = (int)ch->point.y;

        int playerBottom = ch->rect.y + ch->rect.h;
        int holeBottom = fallRect.y + fallRect.h;

        if (playerBottom >= holeBottom)
        {
            KillPlayer(ch);
        }
    }
}

#define RAIL_Y_TARGET 580.0f
#define RAIL_FALL_SPEED 13.5f
void UpdateTrainRailFall(CharaInfo *ch)
{

    if (!ch->railroadflag && InTheRail && !ch->InSubway && !InTheTrain)
    {
        ch->railroadflag = 1;
    }
    else if (ch->railroadflag)
    {

        // 落ちる演出
        if (ch->point.y < RAIL_Y_TARGET)
        {
            ch->point.y += RAIL_FALL_SPEED;

            if (ch->point.y >= RAIL_Y_TARGET)
                ch->point.y = RAIL_Y_TARGET;
        }
        // 到着後完全固定
        else
        {
            ch->point.y = RAIL_Y_TARGET;
        }

        //  物理的影響遮断
        ch->vel.y = 0;
    }
}

void RespawnCheck(CharaInfo *ch)
{
    if (!ch)
        return;
    if (ch->stts != CS_Death)
        return;

    long long now = now_ms();

    if (now - ch->deadAtMs < 5000)
        return; // まだ5秒経ってない

    int id = ch->type - CT_PLAYER0;
    if (id < 0 || id >= 4)
        return;

    // 座標セット
    ch->point.x = gRespawnPoints[id].x;
    ch->point.y = gRespawnPoints[id].y;
    ch->rect.x = (int)ch->point.x;
    ch->rect.y = (int)ch->point.y;

    // 状態リセット

    ch->hp = 3;
    ch->vel.x = ch->vel.y = 0;
    ch->railroadflag = 0;
    ch->hold = 0;
    ch->holder = NULL;
    ch->holdTarget = NULL;
    ch->hitBySinkansen = 0;
    ch->deadAtMs = 0;
    ch->stts = CS_Normal;
    ch->IntheHole = 0;

    ch->invincibleflag = 1;
    ch->invincibleStartMs = now_ms();
}

void UpdateInvincible(CharaInfo *ch)
{
    if (!ch)
        return;
    if (!ch->invincibleflag)
        return;

    long long now = now_ms();
    if (now - ch->invincibleStartMs >= 3000)
    {
        ch->invincibleflag = 0;
    }
}

// y軸値によるスケール値をrectにも適用
void UpdateScaleServer(CharaInfo *ch)
{
    float yMin = 500.0f;
    float yMax = 1200.0f;
    float scaleMin = 0.5f;
    float scaleMax = 1.8f;

    float py = ch->point.y + ch->rect.h / 2;

    if (py < 560.0f)
        py = 560.0f;

    float t = (py - yMin) / (yMax - yMin);
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;

    float scale = scaleMax - t * (scaleMax - scaleMin);

    ch->rect.w = (int)(ch->Baserect.w / scale);
    ch->rect.h = (int)(ch->Baserect.h / scale);
}

void AddScore(CharaInfo *ch, int event)
{
    if (!ch)
    {
        return;
    }

    switch (event)
    {
    case THROWtoVendor:
        ch->score += 100;
        printf("Score Up! Player %d → score=%d\n",
               ch->type - CT_PLAYER0, ch->score);
        break;
    case THROWtoVendorRed:
        ch->score += 500;
        printf("Score Up! Player %d → score=%d\n",
               ch->type - CT_PLAYER0, ch->score);
        break;
    }
}

CharaInfo *SevergCharaHead = NULL;
void UpdateAttackRects(CharaInfo *ch) // 攻撃判定の範囲更新、色々合って初期化も兼ねてる
{
    int w = ch->rect.w;
    int h = ch->rect.h;
    int margin = 30; // 調節可能
    // 上
    ch->attackrectUp.x = ch->rect.x - margin;
    ch->attackrectUp.y = ch->rect.y - h - margin;
    ch->attackrectUp.w = w + 2 * margin;
    ch->attackrectUp.h = h + margin;

    // 下
    ch->attackrectDown.x = ch->rect.x - margin;
    ch->attackrectDown.y = ch->rect.y + ch->rect.h;
    ch->attackrectDown.w = w + 2 * margin;
    ch->attackrectDown.h = h + margin;

    // 左
    ch->attackrectLeft.x = ch->rect.x - w - margin;
    ch->attackrectLeft.y = ch->rect.y - margin;
    ch->attackrectLeft.w = w + margin;
    ch->attackrectLeft.h = h + 2 * margin;

    // 右
    ch->attackrectRight.x = ch->rect.x + ch->rect.w;
    ch->attackrectRight.y = ch->rect.y - margin;
    ch->attackrectRight.w = w + margin;
    ch->attackrectRight.h = h + 2 * margin;
}

void StartHitByTrain(CharaInfo *ch)
{
    if (ch->hitByTrain)
        return;

    ch->hitByTrain = 1;
    ch->hitByTrainStartMs = now_ms();
    ch->hitByTrainDurationMs = 3000;

    ch->movestts = ANI_SPIN;
    ch->vel.x = 0;
    ch->vel.y = 0;
}

void UpdateHitByTrain(CharaInfo *ch)
{
    if (!ch->hitByTrain)
        return;

    long long now = now_ms();

    if (now - ch->hitByTrainStartMs >= ch->hitByTrainDurationMs)
    {
        KillPlayer(ch);

        ch->hitByTrain = 0;
        ch->hitByTrainDurationMs = 0;
    }
}

//========================
// 物体を持ち上げる処理
//========================
void LiftObject(CharaInfo *ch, CharaInfo *obj)
{
    if (!ch || !obj)
        return;
    if (ch->railroadflag)
        return;
    // 持ち上げ状態に変更
    obj->stts = CS_Holded;
    obj->InSubway = SDL_FALSE;
    // 持ち主を記録
    obj->holder = ch;
    ch->holdTarget = obj;

    // プレイヤーの位置
    float px = ch->point.x;
    float py = ch->point.y;
    int pw = ch->rect.w;

    // 物体のサイズ
    int ow = obj->rect.w;
    int oh = obj->rect.h;

    // 持ち上げ量を切り替える
    float liftMul = 5.0f;

    // 対象がプレイヤーなら、もっと上に
    if (obj->type >= CT_PLAYER0 && obj->type <= CT_PLAYER3)
    {
        liftMul = 10.0f;
    }

    // きれいに中心合わせ
    float ox = 20 + (px + (pw - ow) / 2.0f);
    float oy = py + (liftMul * oh) + 100;

    // 位置を更新
    obj->point.x = ox;
    obj->point.y = oy;
    obj->rect.x = (int)ox;
    obj->rect.y = (int)oy;

    ch->hold = 1; // 「持っている」状態に

    if (ch->type == CT_PLAYER0)
    {
        obj->whoholder = 1; // 0使うの怖いから一個ずつずらしてる
    }
    if (ch->type == CT_PLAYER1)
    {
        obj->whoholder = 2;
    }
    if (ch->type == CT_PLAYER2)
    {
        obj->whoholder = 3;
    }
    if (ch->type == CT_PLAYER3)
    {
        obj->whoholder = 4;
    }

}

//====================
//KnockDown関数
//====================
void UpdateKnockdown(CharaInfo *ch)
{
    if (ch->stts != CS_Knockdown)
        return;

    long long now = now_ms();
    long long passed = now - ch->knockLastMs;
    ch->knockLastMs = now;

    ch->knockRemainMs -= passed;
    if (ch->knockRemainMs > 0)
        return;

    // ===== 復帰 =====
    ch->hold = 0;
    ch->holdTarget = NULL;
    ch->holder = NULL;

    ch->stts = CS_Normal;
    ch->movestts = ANI_Stop;
    ch->hp = 3;
    ch->knockRemainMs = 0;

}

void SpawnRedCoin()
{ 
    for (CharaInfo *coin = SevergCharaHead; coin; coin = coin->next)
    {

        if (coin->type == CT_REDCOIN)
        {
            if (coin->stts != CS_Disable)
                continue;

            coin->stts = CS_Normal;

            coin->point.x = 5050;
            coin->point.y = 500;

            coin->rect.x = (int)coin->point.x;
            coin->rect.y = (int)coin->point.y;

            break; // 1枚だけ
        }
    }
}

void SpawnCoin(CharaInfo *atm, CharaInfo *ch)
{ 
    for (CharaInfo *coin = SevergCharaHead; coin; coin = coin->next)
    {

        if (coin->type == CT_COIN)
        {
            if (coin->stts != CS_Disable)
                continue;

            // ---- コイン復活 ----
            coin->stts = CS_Normal;

            // ATMの上に出す
            coin->point.x = atm->point.x + rand() % 40 - 20;
            coin->point.y = atm->point.y - coin->rect.h - 10;

            coin->rect.x = (int)coin->point.x;
            coin->rect.y = (int)coin->point.y;

            // ---- 一瞬持たせる ----
            LiftObject(atm, coin);

            // ---- すぐ投げる ----
            atm->playerdir = ch->playerdir;
            ThrowObject(atm);

            break; // 1枚だけ
        }
    }
}

void RespawnDisabledCoin(void)
{
    long long now = now_ms();

    // 40秒経っていなければ何もしない
    if (now - lastCoinRespawnMs < 60000)
        return;

    // コイン探索
    for (CharaInfo* ch = SevergCharaHead; ch; ch = ch->next) {

        if (ch->type != CT_COIN)
            continue;

        if (ch->stts != CS_Disable)
            continue;

        // ===== 見つけた1個だけ復活 =====
        ch->stts = CS_Normal;
    

        ch->point.x = 1500 + rand() % (7000 - 1500 + 1);
        ch->point.y = 700;

        ch->rect.x = (int)ch->point.x;
        ch->rect.y = (int)ch->point.y;
 
        lastCoinRespawnMs = now;
        return; // ← 1個だけ
    }
}


void RespawnDisabledCoin2(void) 
{
    long long now = now_ms();

    // 40秒経っていなければ何もしない
    if (now - lastCoinRespawnMs2 < 90000)
        return;

    // コイン探索
    for (CharaInfo* ch = SevergCharaHead; ch; ch = ch->next) {

        if (ch->type != CT_COIN)
            continue;

        if (ch->stts != CS_Disable)
            continue;

        // ===== 見つけた1個だけ復活 =====
        ch->stts = CS_Normal;
    

        ch->point.x = 1500 + rand() % (7000 - 1500 + 1);
        ch->point.y =  700 + rand() % ( 900 -  700 + 1);

         ch->rect.x = (int)ch->point.x;
            ch->rect.y = (int)ch->point.y;

        lastCoinRespawnMs2 = now;
        return; // ← 1個だけ
    }
}


//====================
//ATM関数
//====================
void UpdateATM(CharaInfo *atm)
{
    if (!atm || atm->type != CT_ATM)
        return;

    long long now = now_ms();

    // 振動終了
    if (atm->VibrateEnd > 0 && now >= atm->VibrateEnd)
    {
        atm->VibrateEnd = 0;
        atm->movestts = ANI_ATM_LIGHTOFF_DEFALT;
    }

    // 5秒経過 → 再び点灯
    if (atm->AtmTime > 0 && now >= atm->AtmTime)
    {
        atm->AtmTime = 0;
        atm->movestts = ANI_ATM_LIGHTON_DEFALT;
    }
}

void UpdateVendor(CharaInfo *ch)
{
    if (ch->animTimer > 0)
    {
        ch->animTimer--;
        if (ch->animTimer == 0)
        {
            ch->movestts = ANI_Stop;
        }
    }
}
void VendorPump(CharaInfo *vendor) // アニメーション用
{
    vendor->movestts = ANI_VENDER_PUMP;
    vendor->animTimer = 20; // 持続フレーム
}

void HitATM(CharaInfo *atm, CharaInfo *attacker)
{
    if (!atm || atm->type != CT_ATM)
        return;

    long long now = now_ms();

    // 振動は常に可能
    atm->movestts = ANI_ATM_LIGHTOFF_VIBRATE;
    atm->VibrateEnd = now + 300; // 0.3秒振動

    // クールタイムの始まり 初めて殴ったときだけ
    if (atm->AtmTime == 0)
    {
        atm->AtmTime = now + 5000;
        SpawnCoin(atm, attacker);
    }

}

//========================
// 持ち上げ解除処理
//========================
void ReleaseHold(CharaInfo *holder, CharaInfo *obj)
{
    if (!holder || !obj)
        return;
    if (obj->type >= CT_PLAYER0 && obj->type <= CT_PLAYER3)
    {
        if (obj->inputcount >= 10)
        {
            obj->stts = CS_Normal;
            obj->knockRemainMs = 0;
        }
        else
        {
            obj->stts = CS_Knockdown;
            obj->movestts = ANI_KNOCKDOWN;
            obj->knockLastMs = now_ms();
        }
    }
    else
    {
        obj->stts = CS_Normal;
    }
    // --- obj 側 ---

    obj->holder = NULL;
    obj->inputcount = 0;
    obj->hp = 3;
    obj->whoholder = 0;

    // 位置を少し下に戻す（めり込み防止）
    obj->point.y = holder->point.y + holder->rect.h;
    obj->rect.y = (int)obj->point.y;

    // --- holder 側 ---
    holder->hold = 0;
    holder->holdTarget = NULL;

}

void Attack(CharaInfo *ch) // 攻撃処理　
{

    long long now = now_ms();

    // ===== 攻撃クールタイム =====
    if (now - ch->hitStartMs < ch->hitDurationMs)
    {
        return;
    }

    SDL_Rect *atk = NULL;
    if (ch->playerdir == Up)
        atk = &ch->attackrectUp;
    if (ch->playerdir == Down)
        atk = &ch->attackrectDown;
    if (ch->playerdir == Left)
        atk = &ch->attackrectLeft;
    if (ch->playerdir == Right)
        atk = &ch->attackrectRight;

    if (!atk)
        return;

    for (CharaInfo *target = SevergCharaHead; target; target = target->next)
    {
        if (ch->hold == 0)
        { // 持ってないとき

            if (target == ch)
                continue;
            if (target->railroadflag)
                continue;
            if (target->type >= CT_PLAYER0 && target->type <= CT_PLAYER3) // playerを殴る
            {
                
                if (target->invincibleflag)
                {
                    continue;
                }

                if (target->stts == CS_Knockdown)
                {
                    if (SDL_HasIntersection(atk, &target->rect))
                    {
                        
                        if (ch->hold == 0 && target->holder == NULL)
                        {

                            // ===== Knockdown時間を一時停止 =====
                            long long now = now_ms();
                            long long passed = now - target->knockLastMs;
                            target->knockRemainMs -= passed;
                            if (target->knockRemainMs < 0)
                                target->knockRemainMs = 0;

                            LiftObject(ch, target);
                        }

                        return;
                    }
                }

                if (target->stts == CS_Normal)
                {
                    if (ch->notinfrontofdoor == 1)
                    {
                        if (target->InSubway)
                        {
                            if (SDL_HasIntersection(atk, &target->rect))
                            {

                                ch->hitcount++;
                                ch->hitStartMs = now;       
                                ch->hitDurationMs = 300;       // 攻撃クールタイム (ms)
                                

                                if (target->hp <= 0)
                                    continue;
                                target->hp -= 1;
                                // ===== HIT アニメーション =====
                                if (target->hp >= 0)
                                {
                                    switch (target->playerdir)
                                    {
                                    case Right:
                                        target->movestts = ANI_HIT_RIGHT;
                                        break;
                                    case Left:
                                        target->movestts = ANI_HIT_LEFT;
                                        break;
                                    case Down:
                                        target->movestts = ANI_HIT_DOWN;
                                        break;
                                    case Up:
                                        target->movestts = ANI_HIT_UP;
                                        break;
                                    }
                                    target->hitStartMs = now_ms();
                                    target->hitDurationMs = 200;
                                }
                                if (target->hp <= 0)
                                {
                                    target->hp = 0;

                                    if (target->hold && target->holdTarget)
                                    {
                                        ReleaseHold(target, target->holdTarget);
                                    }
                                    if (target->holder)
                                    {
                                        ReleaseHold(target->holder, target);
                                    }

                                    target->stts = CS_Knockdown;
                                    target->movestts = ANI_KNOCKDOWN;
                                    target->knockRemainMs = 5000; // 5秒
                                    target->knockLastMs = now_ms();
                                    continue;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (SDL_HasIntersection(atk, &target->rect))
                        {

                            ch->hitcount++;
                            ch->hitStartMs = now;        
                            ch->hitDurationMs = 300;      // 攻撃クールタイム (ms)
                                

                            if (target->hp <= 0)
                                continue;
                            if (target->notinfrontofdoor == 1)
                                continue;
                            target->hp -= 1;
                            // ===== HIT アニメーション =====
                            if (target->hp >= 0)
                            {
                                switch (target->playerdir)
                                {
                                case Right:
                                    target->movestts = ANI_HIT_RIGHT;
                                    break;
                                case Left:
                                    target->movestts = ANI_HIT_LEFT;
                                    break;
                                case Down:
                                    target->movestts = ANI_HIT_DOWN;
                                    break;
                                case Up:
                                    target->movestts = ANI_HIT_UP;
                                    break;
                                }
                                target->hitStartMs = now_ms();
                                target->hitDurationMs = 200;
                            }

                            if (target->hp <= 0)
                            {
                                target->hp = 0;

                                if (target->hold && target->holdTarget)
                                {
                                    ReleaseHold(target, target->holdTarget);
                                }
                                if (target->holder)
                                {
                                    ReleaseHold(target->holder, target);
                                }

                                target->stts = CS_Knockdown;
                                target->movestts = ANI_KNOCKDOWN;
                                target->knockRemainMs = 5000; // 5秒
                                target->knockLastMs = now_ms();
                                continue;
                            }
                        }
                    }
                }
            }
            else if (target->type == CT_COIN || target->type == CT_REDCOIN)
            {
                if (ch->notinfrontofdoor == 1)
                {
                    if (target->InSubway)
                    {
                        if (target->stts == CS_Normal)
                        {
                            if (SDL_HasIntersection(atk, &target->rect))
                            {
                                LiftObject(ch, target);
                                printf("LIFTUP");
                            }
                        }
                    }
                }
                else
                {
                    if (target->stts == CS_Normal)
                    {
                        if (target->notinfrontofdoor == 1)
                            continue;
                        if (SDL_HasIntersection(atk, &target->rect))
                        {
                            LiftObject(ch, target);
                            printf("LIFTUP");
                        }
                    }
                }
            }
            else if (target->type == CT_Lever)
            {
                if (ch->point.y >= 720)
                {
                    if (SDL_HasIntersection(atk, &target->rect))
                    {
                        printf("hit lever \n");
                        if (target->movestts != ANI_BlackOutOn)
                        {
                            target->movestts = ANI_BlackOutOn;
                            target->blackoutStartTick = SDL_GetTicks();
                        }
                    }
                }
            }
            else if (target->type == CT_ATM)
            { // ATMを殴る
                if (target->stts == CS_Normal)
                {
                    if (SDL_HasIntersection(atk, &target->rect))
                    {
                        HitATM(target, ch);

                        printf("Hit ATM!!");
                        return;
                    }
                }
            }
        }
    }
}

// ここに動かないオブジェクトを入れてください
static SDL_bool IsStaticObject(CharaInfo *ch)
{
    return (ch->type == CT_Vendor || ch->type == CT_ATM || ch->type == CT_Left_Pillar ||
            ch->type == CT_Right_Pillar ||
            ch->type == CT_Lever || ch->type == CT_BENCH );
}

// --- CollisionPlayer の修正 ---
SDL_bool CollisionPlayer(CharaInfo *a, CharaInfo *b)
{
    if (a->stts == CS_Disable || b->stts == CS_Disable)
        return SDL_FALSE;

    if (a->railroadflag || b->railroadflag)
        return SDL_FALSE;
    if (a->stts == CS_Holded || b->stts == CS_Holded)
        return SDL_FALSE;

    int aStatic = IsStaticObject(a);
    int bStatic = IsStaticObject(b);

    SDL_Rect ra = a->rect;
    SDL_Rect rb = b->rect;
    SDL_Rect ir;
    if (!SDL_IntersectRect(&ra, &rb, &ir))
        return SDL_FALSE;
    int overlapW = ir.w;
    int overlapH = ir.h;

    // ← ここを point を使うのではなく rect を基準にする
    float ax = (float)a->rect.x + a->rect.w / 2.0f;
    float ay = (float)a->rect.y + a->rect.h / 2.0f;
    float bx = (float)b->rect.x + b->rect.w / 2.0f;
    float by = (float)b->rect.y + b->rect.h / 2.0f;

    float dx = ax - bx;
    float dy = ay - by;

    if (overlapW < overlapH)
    {
        float push = overlapW;

        // a が動ける
        if (!aStatic && bStatic)
        {
            a->point.x += (dx > 0 ? push : -push);
        }
        // b が動ける
        else if (aStatic && !bStatic)
        {
            b->point.x -= (dx > 0 ? push : -push);
        }
        // 両方動ける
        else if (!aStatic && !bStatic)
        {
            float half = push / 2.0f;
            a->point.x += (dx > 0 ? half : -half);
            b->point.x -= (dx > 0 ? half : -half);
        }
    }
    else
    {
        float push = overlapH;

        if (!aStatic && bStatic)
        {
            a->point.y += (dy > 0 ? push : -push);
        }
        else if (aStatic && !bStatic)
        {
            b->point.y -= (dy > 0 ? push : -push);
        }
        else if (!aStatic && !bStatic)
        {
            float half = push / 2.0f;
            a->point.y += (dy > 0 ? half : -half);
            b->point.y -= (dy > 0 ? half : -half);
        }
    }

    // 移動制限（そのまま）
    if (a->point.x < 0)
        a->point.x = 0;
    if (a->point.y < 0)
        a->point.y = 0;

    if (a->point.x + a->rect.w > MAP_Width)
        a->point.x = MAP_Width - a->rect.w;
    if (a->point.y + a->rect.h > MAP_Height)
        a->point.y = MAP_Height - a->rect.h;

    if (b->point.x < 0)
        b->point.x = 0;
    if (b->point.y < 0)
        b->point.y = 0;
    if (b->point.x + b->rect.w > MAP_Width)
        b->point.x = MAP_Width - b->rect.w;
    if (b->point.y + b->rect.h > MAP_Height)
        b->point.y = MAP_Height - b->rect.h;

    // rect の更新
    if (a->type >= CT_PLAYER0 && a->type <= CT_PLAYER3)
    {
        a->rect.x = (int)a->point.x;
        a->rect.y = 150 + (int)a->point.y; // プレイヤー +150
    }
    else if (a->type == CT_COIN)
    {
        a->rect.x = (int)a->point.x;
        a->rect.y = 48 + (int)a->point.y; // coin +48
    }
    else if (a->type == CT_REDCOIN)
    {
        a->rect.x = (int)a->point.x;
        a->rect.y = 48 + (int)a->point.y; // coin +48
    }

    else if (a->type == CT_Vendor)
    {
        a->rect.x = (int)a->point.x;
        a->rect.y = Vendor_ConstNumber + (int)a->point.y;
        // a->rect.w = Vendor_ConstNumber + (int)a->rect.w;
    }
    else if (a->type == CT_ATM)
    {
        a->rect.x = (int)a->point.x; // -50;
        a->rect.y = ATM_ConstNumber + (int)a->point.y;
    }
    else if (a->type == CT_Left_Pillar ||
             a->type == CT_Right_Pillar)
    {
        a->rect.y = 790;
        a->rect.h = 90;
    }
    else
    {
        a->rect.x = (int)a->point.x;
        a->rect.y = (int)a->point.y; // その他
    }

    if (b->type >= CT_PLAYER0 && b->type <= CT_PLAYER3)
    {
        b->rect.x = (int)b->point.x;
        b->rect.y = 150 + (int)b->point.y;
    }
    else if (b->type == CT_COIN)
    {
        b->rect.x = (int)b->point.x;
        b->rect.y = 48 + (int)b->point.y;
    }
    else if (b->type == CT_REDCOIN)
    {
        b->rect.x = (int)b->point.x;
        b->rect.y = 48 + (int)b->point.y;
    }

    else if (b->type == CT_Vendor)
    {
        b->rect.x = (int)b->point.x;
        b->rect.y = Vendor_ConstNumber + (int)b->point.y;
        // a->rect.w = Vendor_ConstNumber + (int)a->rect.w;
    }
    else if (b->type == CT_ATM)
    {
        b->rect.x = (int)b->point.x; //-50;
        b->rect.y = ATM_ConstNumber + (int)b->point.y;
    }
    else
    {
        b->rect.x = (int)b->point.x;
        b->rect.y = (int)b->point.y;
    }

    UpdateAttackRects(a);
    UpdateAttackRects(b);

    return SDL_TRUE;
}

// a当たり判定ができるかを判別する関数
static SDL_bool ObjectSorting(const CharaInfo *ch)
{

    if (ch->inthemovesubway == 1)
        return SDL_FALSE;

    if (!ch)
        return SDL_FALSE;

    // 完全除外
    if (ch->stts == CS_Disable)
        return SDL_FALSE;

    // 掴まれている間は物理衝突しない
    if (ch->stts == CS_Holded)
        return SDL_FALSE;

    // ATM は常に当たる
    if (ch->type == CT_ATM)
        return SDL_TRUE;

    // プレイヤー
    if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
    {
        return (ch->stts == CS_Normal ||
                ch->stts == CS_Knockdown);
    }

    // その他
    return (ch->stts == CS_Normal);
}

// 当たり判定
SDL_bool Collision(CharaInfo *a, CharaInfo *b)
{

    if (!ObjectSorting(a) || !ObjectSorting(b))
        return SDL_FALSE;

    SDL_Rect ir;
    if (!SDL_IntersectRect(&a->rect, &b->rect, &ir))
        return SDL_FALSE;

    int aIsPlayer = (a->type >= CT_PLAYER0 && a->type <= CT_PLAYER3);
    int bIsPlayer = (b->type >= CT_PLAYER0 && b->type <= CT_PLAYER3);
    int aIsCoin = (a->type == CT_COIN);
    int bIsCoin = (b->type == CT_COIN);
    int aIsRCoin = (a->type == CT_REDCOIN);
    int bIsRCoin = (b->type == CT_REDCOIN);
    int aIsMouse = (a->type == CT_MOUSE);
    int bIsMouse = (b->type == CT_MOUSE);
    int aIsVendor = (a->type == CT_Vendor);
    int bIsVendor = (b->type == CT_Vendor);
    int aIsATM = (a->type == CT_ATM);
    int bIsATM = (b->type == CT_ATM);
    int aIsLeftPillar = (a->type == CT_Left_Pillar);
    int bIsLeftPillar = (b->type == CT_Left_Pillar);
    int aIsRightPillar = (a->type == CT_Right_Pillar);
    int bIsRightPillar = (b->type == CT_Right_Pillar);
    int aIsBench = (a->type == CT_BENCH);
    int bIsBench = (b->type == CT_BENCH);

    // --- プレイヤーと柱 ---
    if (aIsPlayer && (bIsLeftPillar || bIsRightPillar))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (bIsPlayer && (aIsLeftPillar || aIsRightPillar))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }


    // --- プレイヤーとbench ---
    if (aIsPlayer && bIsBench)
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (bIsPlayer && aIsBench)
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }


    // --- coin と柱 ---
    if (aIsCoin && (bIsLeftPillar || bIsRightPillar))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (bIsCoin && (aIsLeftPillar || aIsRightPillar))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }

    // --- redcoin と柱 ---
    if (aIsRCoin && (bIsLeftPillar || bIsRightPillar))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (bIsRCoin && (aIsLeftPillar || aIsRightPillar))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }

    // --- プレイヤー同士 ---
    if (aIsPlayer && bIsPlayer)
    {
        if (!(a->IntheHole || b->IntheHole))
        {
            CollisionPlayer(a, b);
            return SDL_TRUE;
        }
    }

    // --- プレイヤー & コイン ---
    if ((aIsPlayer && bIsCoin) || (bIsPlayer && aIsCoin))
    {
        CollisionPlayer(a, b);
        if (b->type == CT_COIN)
            b->holder = a; // bがコインでaがプレイヤーのとき
        if (a->type == CT_COIN)
            a->holder = b; // aがコインでbがプレイヤーのとき

        return SDL_TRUE;
    }
    // --- プレイヤー & rコイン ---
    if ((aIsPlayer && bIsRCoin) || (bIsPlayer && aIsRCoin))
    {
        CollisionPlayer(a, b);
        if (b->type == CT_REDCOIN)
            b->holder = a; // bがコインでaがプレイヤーのとき
        if (a->type == CT_REDCOIN)
            a->holder = b; // aがコインでbがプレイヤーのとき

        return SDL_TRUE;
    }

    // --- プレイヤー & ねずみ ---
    if (aIsPlayer && bIsMouse)
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (bIsPlayer && aIsMouse)
    {
        CollisionPlayer(b, a);
        return SDL_TRUE;
    }

    // ATMとプレイヤー
    if (aIsPlayer && bIsATM)
    {

        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (bIsPlayer && aIsATM)
    {
        CollisionPlayer(b, a);
        return SDL_TRUE;
    }

    // --- コイン同士 ---
    if (aIsCoin && bIsCoin)
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (aIsRCoin && bIsRCoin)
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (aIsCoin && bIsRCoin)
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    if (aIsRCoin && bIsCoin)
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }

    // --- ねずみ同士 ---
    if (aIsMouse && bIsMouse)
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }

    // --- コイン & ねずみ ---
    if ((aIsCoin && bIsMouse) || (bIsCoin && aIsMouse))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }
    // --- コイン & ねずみ ---
    if ((aIsRCoin && bIsMouse) || (bIsRCoin && aIsMouse))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }

    // 自販機とコイン
    if (bIsVendor && aIsCoin)
    {

        // スコア加算（holder がいる時だけ）
        if (a->holder)
        {
            AddScore(a->holder, THROWtoVendor);
            VendorPump(b);
        }

        // ★ Normal コインなら押し返す
        if (a->stts == CS_Normal)
        {
            CollisionPlayer(a, b);
        }

        // ★ 最後に消す
        a->stts = CS_Disable;
        return SDL_TRUE;
    }

    if (aIsVendor && bIsCoin)
    {

        if (b->holder)
        {
            AddScore(b->holder, THROWtoVendor);
            VendorPump(a);
        }

        if (b->stts == CS_Normal)
        {
            CollisionPlayer(a, b);
        }

        b->stts = CS_Disable;
        return SDL_TRUE;
    }
    // 自販機とコイン
    if (bIsVendor && aIsRCoin)
    {

        // スコア加算（holder がいる時だけ）
        if (a->holder)
        {
            AddScore(a->holder, THROWtoVendorRed);
            VendorPump(b);
        }

        // ★ Normal コインなら押し返す
        if (a->stts == CS_Normal)
        {
            CollisionPlayer(a, b);
        }

        // ★ 最後に消す
        a->stts = CS_Disable;
        return SDL_TRUE;
    }

    if (aIsVendor && bIsRCoin)
    {

        if (b->holder)
        {
            AddScore(b->holder, THROWtoVendorRed);
            VendorPump(a);
        }

        if (b->stts == CS_Normal)
        {
            CollisionPlayer(a, b);
        }

        b->stts = CS_Disable;
        return SDL_TRUE;
    }

    // 自販機とプレイヤー
    if ((aIsVendor && bIsPlayer) || (bIsVendor && aIsPlayer))
    {
        CollisionPlayer(a, b);

        return SDL_TRUE;
    }

    if (aIsATM || bIsATM)
    {
        // 純粋に物理衝突のみ
        CollisionPlayer(a, b);

        return SDL_TRUE;
    }

    // --- ねずみ & 自販機 ---
    if ((aIsMouse && bIsVendor) || (bIsMouse && aIsVendor))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }

    // --- ねずみ & ATM ---
    if ((aIsMouse && bIsATM) || (bIsMouse && aIsATM))
    {
        CollisionPlayer(a, b);
        return SDL_TRUE;
    }

    return SDL_FALSE;
}

//=================================================
// 空中のキャラクターへの動作
//=================================================
SDL_bool FlyCollision(CharaInfo *a, CharaInfo *b)
{
    CharaInfo *coin = NULL;
    CharaInfo *vendor = NULL;

    // Coin + Vendor 判定
    if (a->type == CT_COIN && a->stts == CS_Fly && b->type == CT_Vendor)
    {
        coin = a;
        vendor = b;
    }
    else if (b->type == CT_COIN && b->stts == CS_Fly && a->type == CT_Vendor)
    {
        coin = b;
        vendor = a;
    }
    else if (a->type == CT_REDCOIN && a->stts == CS_Fly && b->type == CT_Vendor)
    {
        coin = a;
        vendor = b;
    }
    else if (b->type == CT_REDCOIN && b->stts == CS_Fly && a->type == CT_Vendor)
    {
        coin = b;
        vendor = a;
    }
    else
    {
        return SDL_FALSE;
    }

    SDL_Rect ir;
    if (SDL_IntersectRect(&coin->rect, &vendor->Vendor_rect, &ir))
    {
        printf("Add Score in FlyCollision\n");

        if (coin->type == CT_COIN)
        {
            AddScore(coin->holder, THROWtoVendor);
            printf("Add Score COIN\n");
        }

        else
        {
            AddScore(coin->holder, THROWtoVendorRed);
            printf("Add Score redCOIN\n");
        }

        VendorPump(vendor);
        coin->stts = CS_Disable;
        coin->holder = NULL;

        return SDL_TRUE;
    }

    return SDL_FALSE;
}



//====================================
// 物体を投げる関数
//====================================
void ThrowObject(CharaInfo *ch)
{
    if (!ch || ch->hold != 1 || !ch->holdTarget)
        return;

    CharaInfo *obj = ch->holdTarget;

    float vx = 0.0f;
    float vy = 0.0f;

    // ---- 初速設定 ----
    switch (ch->playerdir)
    {
    case Up:
        vy = -THROW_POWER_UP; // 위로 던지기
        vx = 0;
        break;
    case Down:
        vx = THROW_POWER_DOWN; // 아래로 던지기
        vx = 0;
        break;
    case Left:
        vx = -THROW_POWER_UP * 0.5f; // 약간 위로 띄우기
        vx = -THROW_POWER_X;         // 왼쪽으로
        break;
    case Right:
        vx = -THROW_POWER_UP * 0.5f; // 약간 위로 띄우기
        vx = THROW_POWER_X;          // 오른쪽으로
        break;
    }

    // --- 手の位置から飛び出す ---
    int pw = ch->rect.w;
    int ow = obj->rect.w;
    float start_x = ch->point.x + (pw - ow) / 2.0f;
    float start_y = ch->point.y - obj->rect.h - 60.0f;

    obj->point.x = start_x;
    obj->point.y = start_y;
    obj->rect.x = (int)start_x;
    obj->rect.y = (int)start_y;

    // 物理初速をセット（ここが一番大事）
    obj->vel.x = vx;
    obj->vel.y = vy;

    // 状態をFlyに
    if (obj->type == CT_COIN || obj->type == CT_REDCOIN)
    {
        obj->stts = CS_Fly;
    }
    else if (obj->type >= CT_PLAYER0 && obj->type <= CT_PLAYER3)
    {
        obj->stts = CS_Fly;
        obj->hp = 3;
    }
    if(obj->InSubway){
        obj->holedindubway=1;
    }

    // プレイヤーhold解除
    if (obj->type == CT_COIN || obj->type == CT_REDCOIN)
    {
        if (obj->stts != CS_Fly)
        {
            obj->holder = NULL;
        }
    }
    else
    {
        obj->holder = NULL;
    }

    ch->holdTarget = NULL;
    ch->hold = 0;

    obj->whoholder = 0;

    // ---- 着地地点（あなたの条件） ----
    float baseY = ch->point.y + (3 * ch->rect.h);

    printf("ch rect y %d  rect h %d\n", ch->rect.y, ch->rect.h);
    printf("base Y %f   \n", baseY);

    switch (ch->playerdir)
    {
    case Up:
    {
         obj->throwLandY = baseY - 240.0f;
           if (ch->notinfrontofdoor == 1)
        {
            obj->throwLandY = 480;
        if (obj->type == CT_COIN ||obj->type == CT_REDCOIN)
        {
            obj->throwLandY = 570 ;
        }
        }
    
    }
     break;
    case Down:
    {
        obj->throwLandY = baseY + 140.0f;
        if (ch->notinfrontofdoor == 1)
        {
            obj->throwLandY = 480;
        if (obj->type == CT_COIN || obj->type == CT_REDCOIN)
        {
            obj->throwLandY = 570;
        }
        }
    }
    break;
    case Left:
    case Right:
    {
        obj->throwLandY = ch->point.y;
        if (obj->type == CT_COIN ||  obj->type == CT_REDCOIN)
        {
            obj->throwLandY = ch->point.y + 104;
        }
    }
    break;
    default:
        obj->throwLandY = ch->point.y;
        break;
    }
}

#define TRAIN_START_X -45000.0f
#define TRAIN_END_X 35000.0f
#define TRAIN_SPEED 0.0f             // 速度 3200.0f  ここ使われてなかった
#define TRAIN_INTERVAL 30000 // 1分ごとに入って来る 60000


//====================================
// 電車の移動関数
//====================================
void TrainMovement(CharaInfo *ch, long long cur)
{
    // 移動状態
    enum
    {
        TRAIN_STATE_MOVING,
        TRAIN_STATE_DECEL,
        TRAIN_STATE_STOPPED,
        TRAIN_STATE_ACCEL
    };
    static int state = TRAIN_STATE_MOVING;
    static SDL_bool redCoinSpawned = SDL_FALSE;

    // 時間保存
    static long long stopEndTime = 0;

    //-----------------------------------------
    // パラメータ
    //-----------------------------------------
    const float decelPoint = 550.0f;  // 目標停車位置
    const float stopDuration =  25000; // 15秒   15000
    const float maxSpeed = 1600.0f;   // 最高速度 1600がもとの値
    const float minSpeed = 50.0f;     // ほぼ停止速度
    const float decelDist = 8000.0f;  // 減速区間 長さ(px)
    const float accelDist = 8000.0f;  // 加速区間 長さ(px)

    float dt = TICK_MS / 1000.0f;

    //-----------------------------------------
    // 1) 右に移動
    //-----------------------------------------
    if (state == TRAIN_STATE_MOVING)
    {
        ch->point.x += maxSpeed * dt;

        // 減速開始地点に到達
        if (ch->point.x >= decelPoint - decelDist)
            state = TRAIN_STATE_DECEL;

        return;
    }

    //-----------------------------------------
    // 2)減速
    //-----------------------------------------
    if (state == TRAIN_STATE_DECEL)
    {
        float distToStop = decelPoint - ch->point.x;
        if (distToStop < 0)
            distToStop = 0;

        // 0~1
        float t = distToStop / decelDist;
        if (t < 0)
            t = 0;
        if (t > 1)
            t = 1;

        float speed = minSpeed + t * (maxSpeed - minSpeed);
        ch->point.x += speed * dt;

        // 停車地点到着
        if (ch->point.x >= decelPoint)
        {
            ch->point.x = decelPoint;
            stopEndTime = cur + (long long)stopDuration;
            state = TRAIN_STATE_STOPPED;
            redCoinSpawned = SDL_FALSE;
        }

        return;
    }

    //-----------------------------------------
    // 3) 停車(15秒待ち)
    //-----------------------------------------
    if (state == TRAIN_STATE_STOPPED)
    {
        long long remain = stopEndTime - cur;
        TrapHumanSpeed = 0;
        // 到着→ドアが開く
        if (remain <= stopDuration && remain > stopDuration - 2000)
        {
            ch->movestts = ANI_TrainDoorOPEN;
            traindoor = 1;
            if (!redCoinSpawned)
            {
                SpawnRedCoin(); // ★1回だけ
                redCoinSpawned = SDL_TRUE;
            }
        }
        // 出発2秒前→ドアを閉める
        else if (remain <= 2000 && remain > 0)
        {
            ch->movestts = ANI_TrainDoorCLOSE;
            traindoor = 0;
        }

        // 停車時間切れ→ACCELに切り替え
        if (cur >= stopEndTime)
        {
            state = TRAIN_STATE_ACCEL;
        }
        return;
    }

    //-----------------------------------------
    // 4) 加速
    //-----------------------------------------
    if (state == TRAIN_STATE_ACCEL)
    {
        float distFromStop = ch->point.x - decelPoint;
        if (distFromStop < 0)
            distFromStop = 0;

        float t = distFromStop / accelDist;
        if (t > 1)
            t = 1;

        float speed = minSpeed + t * (maxSpeed - minSpeed);
        ch->point.x += speed * dt;
        TrapHumanSpeed = speed * dt;

        if (ch->point.x >= TRAIN_END_X)
        {
            ch->point.x = TRAIN_START_X;
            state = TRAIN_STATE_MOVING;
        }

        return;
    }
}

#define PASS_TRAIN_START_X 35000.0f
#define PASS_TRAIN_END_X -25000.0f
#define PASS_TRAIN_SPEED -12000.0f
#define PASS_TRAIN_INTERVAL 70000

// 手前を通過する電車
void TrainMovement_Pass(CharaInfo *ch)
{
    static long long nextStartTime = 0;
    long long now = now_ms();
    float dt = TICK_MS / 1000.0f;

    // 初回待ち
    if (nextStartTime == 0)
    {
        nextStartTime = now + PASS_TRAIN_INTERVAL;
        ch->point.x = PASS_TRAIN_START_X;
        return;
    }

    // 　出発前
    if (now < nextStartTime)
        return;

    // 移動
    ch->point.x += PASS_TRAIN_SPEED * dt;

    // 画面の外に出たら次の待機
    if (ch->point.x <= PASS_TRAIN_END_X)
    {
        ch->point.x = PASS_TRAIN_START_X;
        nextStartTime = now + PASS_TRAIN_INTERVAL;
    }
}

// 電光掲示板（追加箇所）
#define Board_SPEED 20
#define Board_START_Y -100
#define Board_END_Y 0
#define Board_INTERVAL 180000 // 電光掲示板とIMG（新幹線.jpeg）のイベント発生間隔 60000

#define IMG_START_X -25000.0f
#define IMG_INTERVAL 10000 // 電光掲示板が降りてきてからIMG(新幹線)が発車するまでの時間

//====================================
// 新幹線とプレイヤーの座標一致関数
//====================================
void MergeSinkansenPlayerPoint(CharaInfo *ch)
{

    //===================
    // IMG(新幹線.jpeg)の右側よりプレイヤーを左に進めなくする（追加箇所）
    // IMGとプレイヤーの当たり判定はcollision関数を使わずにプレイヤーの座標を制限することで実装
    //==================
    static int IMG_point_x = 0;
    static int IMG_point_w = 0;

    if (ch->InSubway)
        return;
    if(ch->railroadflag)
        return;    

    // IMGの大きさを保持する
    if (ch->type == CT_IMG)
    {
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

        IMG_point_x = ch->point.x;
        IMG_point_w = ch->rect.w * scale;
    }

    if (CT_PLAYER0 <= ch->type && ch->type <= CT_PLAYER3)
    {
        if (ch->point.x < IMG_point_x + IMG_point_w + 150)
        {
            if (IMG_point_x > ch->point.x)
                return;

            ch->point.x = IMG_point_x + IMG_point_w + 150;
            ch->hitBySinkansen = 1;
            ch->movestts = ANI_SPIN;
        }
    }
}


//====================================
// 電光掲示板の移動関数
//====================================
void DisplayMovement(CharaInfo *ch)
{
    static long long nextStartTime = 0;
    long long now = now_ms();
    float dt = TICK_MS / 1000.0f;

    // 状態
    typedef enum
    {
        Board_STOPP,
        Board_DOWN,
        Board_UP
    } BoardMOVE;

    typedef enum
    {
        Board_LightOn,
        Board_LightOff
    } BoardANI;

    typedef enum
    {
        IMG_STOPP,
        IMG_MOVE
    } IMGMOVE;

    static BoardMOVE Board_STATE = Board_DOWN;
    static BoardANI Board_ANI = Board_LightOff;
    static IMGMOVE IMG_STATE = IMG_STOPP;

    // 初回待ち
    if (nextStartTime == 0)
    {
        nextStartTime = now + Board_INTERVAL;
        ch->point.y = Board_START_Y;
        return;
    }
    // 　出発前
    if (now < nextStartTime)
        return;

    //=========================
    // 画面外から電光掲示板の表示場所までy座標を下げる
    //=========================
    if (Board_STATE == Board_DOWN && Board_ANI == Board_LightOff && IMG_STATE == IMG_STOPP)
    {
        ch->point.y += Board_SPEED * dt;

        if (Board_END_Y <= ch->point.y && ch->point.y <= Board_END_Y + 5)
        {
            ch->point.y = Board_END_Y;
            Board_STATE = Board_STOPP;
            Board_ANI = Board_LightOn;
            IMG_STATE = IMG_MOVE;
            printf("IMG_MoveSTART = %d\n", IMG_MOVE);
        }
    }

    //====================================
    // 電光掲示板のy座標を画面外まで上げる
    //=====================================
    if (Board_STATE == Board_UP && IMG_STATE == IMG_STOPP)
    {
        ch->point.y -= Board_SPEED * dt;
        if (Board_START_Y - 5 <= ch->point.y && ch->point.y <= Board_START_Y)
        {
            ch->point.y = Board_START_Y;
            Board_STATE = Board_DOWN;
            Board_ANI = Board_LightOff;

            // 次の出現時刻の更新
            nextStartTime = now + Board_INTERVAL;
        }
    }

    //============================================
    // スプライトシートの更新（文字の表示・移動）
    //============================================
    if (Board_ANI == Board_LightOn)
    {
        ch->movestts = ANI_Board_LightOn;
    }
    else
    {
        ch->movestts = ANI_Board_LightOff;
    }

    //============================================
    // 新幹線.jpegを移動させる。
    //============================================
    static long long IMG_WaitTime = 0;
    long long IMG_now = now_ms();
    for (CharaInfo *ch2 = SevergCharaHead; ch2; ch2 = ch2->next)
    {
        if (ch2->type == CT_IMG && IMG_STATE == IMG_MOVE)
        {
            // 初回待ち
            if (IMG_WaitTime == 0)
            {
                IMG_WaitTime = IMG_now + IMG_INTERVAL;
                ch2->point.x = IMG_START_X;
            }
            // インターバル後に発車
            if (IMG_now >= IMG_WaitTime)
            {
                if (IMGMovement(ch2) == SDL_TRUE)
                {
                    IMG_WaitTime = 0;
                    IMG_STATE = IMG_STOPP;
                    Board_STATE = Board_UP;
                    Board_ANI = Board_LightOff;
                }
            }
        }
    }
}

// 新幹線.jpeg
#define IMG_SPEED 9000.0f  // 9000.0f
#define IMG_END_X 40000.0f // 15000.0f //9000.0f

SDL_bool IMGMovement(CharaInfo *ch)
{

    float dt = TICK_MS / 1000.0f;

    printf("IMG:ch->point.x = %f\n", ch->point.x);

    ch->point.x += IMG_SPEED * dt;

    if (ch->point.x >= IMG_END_X)
    {
        ch->point.x = IMG_START_X;

        return SDL_TRUE;
    }

    return SDL_FALSE;
}


// ============================
// 電車に閉じ込められたとき
// ============================
void TrapInTrain(CharaInfo *ch)
{
    ch->point.x += TrapHumanSpeed;
    ch->inthemovesubway = 1;

    if(ch->hold ==1){
        ch->holdTarget->inthemovesubway =1;
    }

    if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
    {
        if (ch->point.y < 580)
            ch->point.y = 580;
        if (ch->point.y > 610)
            ch->point.y = 610;
  
    }
    else if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)
    {
        if (ch->point.y < 500)
            ch->point.y = 500;
        if (ch->point.y > 530)
            ch->point.y = 530;
    }
    if (ch->point.x <= uptrainx + 1000)
    {
        ch->point.x = uptrainx + 1000;
    } // 7550
    if (ch->point.x >= uptrainx + 7000)
    {
        ch->point.x = uptrainx + 7000;
    }
}


// ============================
// 停電
// ============================
void UpdateBlackout(CharaInfo *ch)
{
    if (ch->type != CT_Lever)
        return;

    if (ch->movestts != ANI_BlackOutOn)
        return;

    Uint32 now = SDL_GetTicks();

    if (now - ch->blackoutStartTick >= 20000) // 20초
    {
        ch->movestts = ANI_BlackOutOff;
        printf("Blackout OFF\n");
    }
}




typedef struct
{
    float x;
    float y;
} Vec2;


static float cross(Vec2 a, Vec2 b, Vec2 p)
{
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// ============================
// 左のホールの座標（外積）
// ============================
int IsPointInSlantedRect(float px, float py ,CharaInfo *ch )
{
    Vec2 P = {px, py};

    Vec2 A = {800, 680};
    Vec2 B = {1600, 680};
    Vec2 C = {1200, 750};
    Vec2 D = {670, 750};
     if (ch->type == CT_COIN || ch->type == CT_REDCOIN)
    {
        A.x = 1020;  A.y = 775;
        B.x = 1600; B.y = 775;
        C.x = 1300; C.y = 865;
        D.x = 720;  D.y = 865;
    }
    float c1 = cross(A, B, P);
    float c2 = cross(B, C, P);
    float c3 = cross(C, D, P);
    float c4 = cross(D, A, P);

    // すべて同じ符号なら内部
    if ((c1 >= 0 && c2 >= 0 && c3 >= 0 && c4 >= 0) ||
        (c1 <= 0 && c2 <= 0 && c3 <= 0 && c4 <= 0))
        return 1;

    return 0;
}


// ============================
// 左のホール
// ============================
void UpdateHoleFall(CharaInfo *ch)
{
    // ============================
    // ホール進入判定座標の決定
    // ============================
    float checkX = ch->point.x;
    float checkY = ch->point.y;

    if (ch->stts == CS_Holded && ch->holder)
    {
        checkX = ch->holder->point.x;
        checkY = ch->holder->point.y;
    }

    // ============================
    // 進入check
    // ============================
    if (!ch->IntheHole)
    {
        if (IsPointInSlantedRect(checkX, checkY , ch ))
        {
            ch->IntheHole = 1;

            if ((ch->type == CT_COIN || ch->type == CT_REDCOIN) && ch->stts != CS_Normal){
                ch->IntheHole = 0;
            }
            // 持っていたターゲットも一緒に
            if (ch->holdTarget)
            {
                CharaInfo *tgt = ch->holdTarget;
                tgt->IntheHole = 1;

            }
        }
    }

    // ============================
    // 落下処理
    // ============================
    if (ch->IntheHole)
    {
        ch->point.y += 35;

        if (ch->point.y >= 1500)
        {
            ch->point.y = 1500;
            KillPlayer(ch);
            ch->IntheHole = 0;
        }
    }
}


//=======================
//全員接続状態確認
//=======================
SDL_bool AllPlayersStarted(void)
{
    int count = 0;

    for (CharaInfo *ch = SevergCharaHead; ch; ch = ch->next)
    {
        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3)//debug
        {
            if (ch->input.nowstts != NOW_INGAME)
                return SDL_FALSE;

            count++;
        }
    }

    return (count == 4) ? SDL_TRUE : SDL_FALSE;//debug  ひとり　count == 1　//　4人　count == ４
}


int PrintError(const char *msg)
{
    fprintf(stderr, "Error: %s\n", msg);
    return -1;
}