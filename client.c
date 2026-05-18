// =============================================================
// client.c  
// =============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "game.h"

#define PORT 5000

int my_id, sock;
NetPack latest[MAX_CLIENT];
pthread_mutex_t pm = PTHREAD_MUTEX_INITIALIZER;
extern CharaInfo *gCharaHead; 
static int prevBlackOut = 0;
static int sinkansen_played = 0;
static int trainpass_played = 0;
static int train_played = 0;
static int board_played = 0;


ssize_t readn(int fd, void *buf, size_t n) {
    size_t left=n; char*p=buf;
    while(left>0){ 
        ssize_t r=read(fd,p,left);
     if(r<=0)return(r==0?n-left:-1); 
     left-=r;
     p+=r;
     } 
     return n;
}
ssize_t writen(int fd,const void*buf,size_t n){
    size_t l=n;
    const char*p=buf;
    while(l>0){
        ssize_t w=write(fd,p,l);
        if(w<=0)return-1;
        l-=w;
        p+=w;
        }
        return n;
        }


void *RecvLoop(void *a){
    while(1){
        NetPack buf[MAX_CLIENT];
        NetPack objects[MAX_OBJECTS];

        // プレイヤー座標受信
        if (readn(sock, buf, sizeof(buf)) <= 0) break;

        // オブゼット情報受信
        if (readn(sock, objects, sizeof(objects)) <= 0) break;

        pthread_mutex_lock(&pm);
        memcpy(latest, buf, sizeof(buf));

        // Objects 情報を gCharaHead に反映
        CharaInfo* ch = gCharaHead;
        int RecvObjectIndex = 0;
        while (ch && RecvObjectIndex < MAX_OBJECTS){

            if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3) {
                int id = ch->type - CT_PLAYER0;
                if (id >= 0 && id < MAX_CLIENT) {
                    ch->input.nowstts = buf[id].nowstts;
                }
            }

            if (ch->type == CT_COIN || ch->type == CT_TRAIN || ch->type==CT_TRAININSIDE || ch -> type == CT_TRAININSIDEDOOR || 
            ch->type == CT_MOUSE || ch->type == CT_Vendor || ch->type == CT_ATM ||
             ch->type == CT_TRAIN_PASS || ch->type == CT_IMG || ch->type == CT_REDCOIN || ch->type == CT_Arrival_Board ||
             ch->type == CT_Left_Pillar || ch->type == CT_Right_Pillar || ch->type == CT_Lever || ch->type == CT_BENCH 
             ){
                ch->point.x = objects[RecvObjectIndex].x;
                ch->point.y = objects[RecvObjectIndex].y;
                ch->rect.x = objects[RecvObjectIndex].rect.x;
                ch->rect.y = objects[RecvObjectIndex].rect.y;
                ch->stts = objects[RecvObjectIndex].visible;
                ch->score = objects[RecvObjectIndex].score;
                ch->movestts = objects[RecvObjectIndex].Movestts;
                ch->InSubway = objects[RecvObjectIndex].InSubway;
                ch->whoholder = objects[RecvObjectIndex].whoholder;
                ch->railroadflag = objects[RecvObjectIndex].railroadflag;
                ch->hitByTrain = objects[RecvObjectIndex].hitByTrain;
                ch->IntheHole = objects[RecvObjectIndex].IntheHole;
                RecvObjectIndex++;
            
            }
            ch = ch->next;
        
            
        }
        pthread_mutex_unlock(&pm);
    }
    return NULL;
}

void SendInput(Keystts *input)
{
    if (writen(sock, input, sizeof(Keystts)) != sizeof(Keystts))
    {   
        perror("write failed");
    }
}



int main(int argc, char* argv[]) {

    memset(latest, 0, sizeof(latest));
    if (argc < 2) {
        printf("usage: <server-ip>\n");
        return 0;
    }

    // サーバー接続
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sv = {0};
    sv.sin_family = AF_INET;
    sv.sin_port   = htons(PORT);
    inet_pton(AF_INET, argv[1], &sv.sin_addr);

    connect(sock, (struct sockaddr*)&sv, sizeof(sv));

    // サーバーが送信するIDを取得
    if (readn(sock, &my_id, sizeof(int)) != sizeof(int)) {
        printf("Failed to get my_id from server\n");
        return 0;
    }
    printf("My ID from server = %d\n", my_id);

    // InitSystem
    InitSystem("chara.data", "position.data", my_id);

    //オーディオの初期化
    InitAudio();
    InitSound_Load(&gGames[my_id]);

    int gameFadeOutDone = 0;
    int NowBlackOut = 0;
    int endingFadeInDone = 0;
    int inEndingFadeIn   = 0;

    // 自分のIDに対応するGameInfoでウィンドウを作成
    InitWindow(&gGames[my_id], "Test", "img/map.png", 1980, 1080);
    
    // RecvLoop 開始
    pthread_t th;
    pthread_create(&th, NULL, RecvLoop, NULL);

    // Lobby Bgm
    PlayBGMFadeIn_Music(&gGames[my_id], SE_Lobby, 1000);

    // StartMenu 
    StartMenu(&gGames[my_id]);

    gGames[my_id].input.nowstts = NOW_INGAME;
    SendInput(&gGames[my_id].input);

    // inGame 進入Bgm
    PlayBGMFadeIn_Music(&gGames[my_id], SE_IngameBGM, 1000);

    Uint32 last = SDL_GetTicks();
    SDL_bool run = SDL_TRUE;

    gameFadeOutDone = 0;
    NowBlackOut = 0;
    endingFadeInDone = 0;
    inEndingFadeIn = 0;


while (run) {
    Uint32 now = SDL_GetTicks();
    static Uint32 last_announce_time = 0;
    gGames[my_id].timeDelta = (now - last) / 1000.0f;
    last = now;
    
    run = InputEvent(&gGames[my_id]);
    SendInput(&gGames[my_id].input);

    pthread_mutex_lock(&pm);

    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next) {
        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3) {
            int id = ch->type - CT_PLAYER0;
            if (id < 0 || id >= MAX_CLIENT) {
                continue; 
            }
            float t = 0.2f;
            ch->point.x += (latest[id].x - ch->point.x) * t;
            ch->point.y += (latest[id].y - ch->point.y) * t;
            ch->rect.x = (int)ch->point.x;
            ch->rect.y = 150 + (int)ch->point.y;
            ch->score    = latest[id].score;
            ch->hitcount    = latest[id].hitcount;
            ch->deathcount    = latest[id].deathcount;
            ch->movestts = latest[id].Movestts;
            ch->InSubway = latest[id].InSubway;
            ch->playerdir = latest[id].direction;
            ch->whoholder = latest[id].whoholder;
            ch->stts = latest[id].visible;
            ch->railroadflag = latest[id].railroadflag;
            ch->hitByTrain = latest[id].hitByTrain; 
            ch->hitBySinkansen = latest[id].hitBySinkansen;   
            ch->input.nowstts = latest[id].nowstts;
            ch->invincibleflag = latest[id].invincibleflag;
            ch->gameTime = latest[id].gameTime;
            ch->IntheHole = latest[id].IntheHole;

            static SDL_bool deathShakeDone = SDL_FALSE; // 一度揺れたかチェック

            if (id == my_id) {
                if (ch->hitByTrain || ch->hitBySinkansen) {
                    StartScreenShake(&gGames[my_id], 20.0f, 0.4f);
                }
                if (ch->movestts >= ANI_HIT_RIGHT && ch->movestts <= ANI_HIT_UP) {
                    StartScreenShake(&gGames[my_id], 20.0f, 0.1f);
                }

                // CS_Deathになってすぐに一度だけ振る
                if (ch->stts == CS_Death && deathShakeDone == SDL_FALSE) {
                    StartScreenShake(&gGames[my_id], 20.0f, 1.0f);
                    deathShakeDone = SDL_TRUE;  // また揺れないように
                }
                if (ch->stts != CS_Death) {
                    deathShakeDone = SDL_FALSE;
                }
            }

            if(ch->invincibleflag){
                UpdateInvincibleBlink(ch);//無敵時間blink
            }
           
        }
        else if (ch->type == CT_COIN) {
            ch->rect.x = (int)ch->point.x;
            ch->rect.y = 48 + (int)ch->point.y;
        }
          else if (ch->type == CT_REDCOIN) {
            ch->rect.x = (int)ch->point.x;
            ch->rect.y = 48 + (int)ch->point.y;
        }  
        else if(ch->type == CT_Vendor){
            ch->rect.x = (int)ch->point.x;
            ch->rect.y = Vendor_ConstNumber + (int)ch->point.y;
        }else if(ch->type == CT_ATM){
            ch->rect.x = (int)ch->point.x;
            ch->rect.y = ATM_ConstNumber + (int)ch->point.y;
        }else if(ch->type == CT_Left_Pillar ||
            ch->type == CT_Right_Pillar ||
            ch->type == CT_Lever){
            ch->rect.x = (int)ch->point.x;
            ch->rect.y = ATM_ConstNumber + (int)ch->point.y;
        }
        else if(ch->type == CT_ATM){
            ch->rect.x = (int)ch->point.x;
            ch->rect.y = (int)ch->point.y;
        }
        else if(ch->type == CT_IMG){
            if(ch->point.x > -15000 && ch->point.x < 11000 && !sinkansen_played){
                Mix_PlayChannel(-1, gGames[my_id].SE_Load[SE_SinkanSen], 0); 
                sinkansen_played = 1; 
            }else if(ch->point.x < -24000){
                sinkansen_played = 0;
            }
        }else if(ch->type == CT_TRAIN_PASS){
            if(ch->point.x > -15000 && ch->point.x < 11000 && !trainpass_played){
                Mix_PlayChannel(-1, gGames[my_id].SE_Load[SE_FrontTrain], 0);
                Mix_Volume( ch->audio_state.chanel, 50);
                trainpass_played = 1;
            }else if(ch->point.x > 30000){
                trainpass_played = 0;
            }
        }else if(ch->type == CT_TRAIN){
            if(ch->point.x > -36000 && !train_played &&ch->point.x < 20000 ){
                Mix_PlayChannel(-1, gGames[my_id].SE_Load[SE_Train], 0);
                Mix_Volume( ch->audio_state.chanel, 50);
                train_played = 1;
            }else if( ch->point.x > 21000 ){
                train_played = 0;
            }
        }else if(ch->type == CT_Arrival_Board){
            if(ch->point.y > -99 && !board_played){
                Mix_PlayChannel(-1, gGames[my_id].SE_Load[SE_Announce], 0);
                board_played = 1;
            }else if(ch->point.y  == -100){
                board_played = 0;
            }
        }
        if (now - last_announce_time >= 120* 1000) { //announce_time
                Mix_PlayChannel(-1, gGames[my_id].SE_Load[SE_Gate], 0);
                last_announce_time = now;
            }


    }

    //停電　
    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next) {
        if(ch->type == CT_Lever){//非常ボタンのmovesttsで識別する
            if (ch->movestts == ANI_BlackOutOn) {
                NowBlackOut = 1;
                break;
            }else if (ch->movestts != ANI_BlackOutOn) {
                NowBlackOut = 0;
                break;
            }   
        }
    }

    PlayAudio(gGames);

    CharaInfo* myPlayer = GetPlayerChara(my_id);
    if (myPlayer) {
        UpdateCamera(&gGames[my_id], myPlayer);//カメラUpdate
    }
    UpdateAnimation(&gGames[my_id], gGames[my_id].timeDelta);
    
    DrawGame(&gGames[my_id]);

    SDL_bool goEnding = SDL_FALSE;
        if (myPlayer && myPlayer->gameTime >= GAME_LIMIT_MS)
        {
            goEnding = SDL_TRUE;
        }


    if (!gameFadeOutDone && !goEnding)
    {
        if (UI_Transition(gGames[my_id].render, -1))//game入ったらfadeout
        {
            gameFadeOutDone = 1; 
        }
    } 
    else//fadeout 終わったら START! 文字が入る
    {
        DrawStartTEXTUI(&gGames[my_id]);
    }
printf("gameFadeOutDone %d\n " ,gameFadeOutDone );

    if (NowBlackOut) {//停電
        DrawBlackOut(gGames[my_id].render,gGames[my_id].cam.w,gGames[my_id].cam.h);
        if (prevBlackOut == 0) {
            Mix_PlayChannel(-1, gGames[my_id].SE_Load[SE_ElectricDown], 0);
        }
    }
    prevBlackOut = NowBlackOut;


    
    if (goEnding)
    {
        inEndingFadeIn = 1;
    }

    if (inEndingFadeIn && !endingFadeInDone)
    {
        if (UI_Transition(gGames[my_id].render, 1)) // fade in 
        {   // すべての効果音チャンネルを停止
            Mix_HaltChannel(-1);
            StopBGMFadeOut_Music(1800);
            PlayBGMFadeIn_Music(&gGames[my_id], SE_Lobby, 1000);
            endingFadeInDone = 1;
            break;  // ここでゲームループ脱出
        }
    }



    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next) {
        int id = ch->type - CT_PLAYER0;
        if (ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3){
            if(ch->stts == CS_Holded  && (id == my_id)){
                DrawSpaceImage(&gGames[my_id]);
                if(gGames[my_id].input.space){
                    StartScreenShake(&gGames[my_id], 20.0f, 0.1f);
                }
            }     
        }
     }

        

  
    
        



    SDL_RenderPresent(gGames[my_id].render);

    pthread_mutex_unlock(&pm);

    SDL_Delay(16); // 60fps
}



// =====================================================
// ENDING LOOP
// =====================================================
{
    SDL_bool endingRun = SDL_TRUE;
    Uint32 endingStart = 0;
    
    ResetResultUI();      

    int phase = 0;

    while (endingRun)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT)
                endingRun = SDL_FALSE;

        SDL_RenderClear(gGames[my_id].render);
        DrawBg(&gGames[my_id]);

        if (phase >= 1 && phase <= 3)
        {
            DrawResultUI(&gGames[my_id]);  

        }

        switch (phase)
        {
        case 0: 
            if (UI_Transition(gGames[my_id].render, -1)) {
                ResetUITransition();
                printf("  phase 0   \n ");
                phase = 1;
            }
            break;

        case 1:
            if (DrawResultUI(&gGames[my_id])) {
                printf("  phase 1  \n ");
                endingStart = SDL_GetTicks();
                Mix_PlayChannel(-1, gGames[my_id].SE_Load[SE_Score], 0);
                phase = 2;
            }
            break;

        case 2:
            DrawResultUI(&gGames[my_id]);  
            if (SDL_GetTicks() - endingStart > 2000) {
                printf("  phase 2  \n ");
                ResetUITransition();
                phase = 3;
            }
            break;

        case 3:
            DrawResultUI(&gGames[my_id]); 
            if (UI_Transition(gGames[my_id].render, 1)) {
                printf("  phase 3  \n ");
                endingStart = SDL_GetTicks(); 
                phase = 4;
            }
            
            break;
        }


        SDL_RenderPresent(gGames[my_id].render);

        if (phase == 4){
            if (SDL_GetTicks() - endingStart > 25) {
                printf("  phase 4 end \n ");
                break;
            }
        }
        

        SDL_Delay(16);
    }

}

    CloseWindow(&gGames[my_id]);
    CloseAudio();
    DestroySystem();
    close(sock);
    return 0;
}

