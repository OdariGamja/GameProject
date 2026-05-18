#include<SDL2/SDL_mixer.h>
#include<stdio.h>
#include<stdlib.h>
#include"game.h"
#include <time.h>


extern int my_id;
extern CharaInfo *gCharaHead;

void InitAudio(void)
{

    Mix_AllocateChannels(32);
    
    if(SDL_Init(SDL_INIT_AUDIO) < 0){
        PrintError("failed to SDL_Init\n");
        return;
    }

    if(Mix_Init(MIX_INIT_MP3) == 0){
        PrintError("failed to Mix_Init\n");
        return;
    }

    if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024) == -1){
        PrintError("failed to Mix_OpenAudio\n");
        return;
    }

    Mix_AllocateChannels(32);

    Mix_ReserveChannels(1);

    srand((unsigned int)time(NULL));

    return;
} 

static int currentBGM = -1;

void PlayBGMFadeIn_Music(GameInfo *gGame, int bgmKind, int ms)
{
    if (currentBGM == bgmKind)
        return;

    Mix_FadeOutMusic(ms);

    Mix_Music *music = NULL;

    switch (bgmKind) {
        case SE_Lobby:
            music = gGame->BGM_Lobby;
            break;
        case SE_IngameBGM:
            music = gGame->BGM_Ingame;
            break;
        default:
            return;
    }

    // BGMの音量調整(0 ~ 128)
    Mix_VolumeMusic(25);  

    Mix_FadeInMusic(music, -1, ms);
    currentBGM = bgmKind;
}

void StopBGMFadeOut_Music(int ms)
{
    Mix_FadeOutMusic(ms);
}


//効果音の読み込み（クライアント）
void InitSound_Load(GameInfo *gGame){

    gGame->BGM_Lobby   = Mix_LoadMUS("sound/lobbybgm.mp3");
    gGame->BGM_Ingame  = Mix_LoadMUS("sound/IngameBGM.mp3");

    gGame->SE_Load[SE_Die] = Mix_LoadWAV("sound/die.mp3");
    gGame->SE_Load[SE_KnockDown] = Mix_LoadWAV("sound/knockdown.mp3");
    gGame->SE_Load[SE_Raise] = Mix_LoadWAV("sound/RAISE.mp3"); 
    gGame->SE_Load[SE_ElectricDown] = Mix_LoadWAV("sound/electricShutDown.wav"); 
    gGame->SE_Load[SE_GameStart] = Mix_LoadWAV("sound/gamestart.mp3"); 
    gGame->SE_Load[SE_Gate] = Mix_LoadWAV("sound/gate.mp3"); 

    gGame->SE_Load[SE_HIT1] = Mix_LoadWAV("sound/hit1.mp3"); 
    gGame->SE_Load[SE_HIT2] = Mix_LoadWAV("sound/hit2.mp3");
    gGame->SE_Load[SE_HIT3] = Mix_LoadWAV("sound/hit3.mp3"); 
    gGame->SE_Load[SE_HIT4] = Mix_LoadWAV("sound/hit4.mp3"); 

    gGame->SE_Load[SE_PressStart] = Mix_LoadWAV("sound/pressstart.wav");
    gGame->SE_Load[SE_Score] = Mix_LoadWAV("sound/score.mp3"); 

    gGame->SE_Load[SE_SinkanSen] = Mix_LoadWAV("sound/sinkansenrun.mp3");  
    gGame->SE_Load[SE_HitbyTrain] = Mix_LoadWAV("sound/HitByTrain.mp3");   

    gGame->SE_Load[SE_FrontTrain] = Mix_LoadWAV("sound/FrontTrain.wav"); 
    gGame->SE_Load[SE_Train] = Mix_LoadWAV("sound/train.mp3");  
    gGame->SE_Load[SE_Announce] = Mix_LoadWAV("sound/announce.mp3");  
    gGame->SE_Load[SE_Gate] = Mix_LoadWAV("sound/gate.mp3"); 
    gGame->SE_Load[SE_Vendor] = Mix_LoadWAV("sound/vendor.wav");
    gGame->SE_Load[SE_ATM] = Mix_LoadWAV("sound/atm.wav");
    gGame->SE_Load[SE_ATTACK] = Mix_LoadWAV("sound/pop.wav"); 

    gGame->SE_Load[SE_EndInterval] = Mix_LoadWAV("sound/end.wav"); 

    printf("SE_SinkanSen ptr = %p\n", gGame->SE_Load[SE_SinkanSen]);
}

int GetRandomHitSE(void)
{
    return SE_HIT1 + (rand() % 4);
}

int GetSELoop(int se)
{
    switch(se){
        case SE_Raise:
        case SE_HIT1:
        case SE_HIT2:
        case SE_HIT3:
        case SE_HIT4:
        case SE_Die:
        case SE_PressStart:
        case SE_GameStart:
        case SE_ElectricDown:
        case SE_SinkanSen:
        case SE_Vendor:
            return 0;   // 1回だけ
        case SE_KnockDown:
        case SE_HitbyTrain:
            return -1;  // ループ
        default:
            return 0;
    }
}



//===================================================
//キャラクタのmovesttsの値が変わると効果音を再生する
//===================================================
void PlayAudio(GameInfo *gGame){
    CharaInfo *myplayer = GetPlayerChara(my_id);//自分の情報

    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next) {

        // 状態が変わらなければ何もしない
        if (ch->movestts == ch->audio_state.beforeMovestts &&
            ch->stts     == ch->audio_state.beforeStts &&
            ch->input.nowstts == ch->audio_state.beforeNowstts)
            continue;

        // Raiseフラグのリセット
        if (ch->movestts != ANI_RAISE_RIGHT &&
            ch->movestts != ANI_RAISE_LEFT  &&
            ch->movestts != ANI_RAISE_DOWN  &&
            ch->movestts != ANI_RAISE_UP)
        {
            ch->audio_state.raisePlayed = 0;
        }
        //ループ SEのみ停止（BGM除く）
        if (ch->audio_state.chanel != -1 &&
            ch->audio_state.isLoopSE &&
            ch->audio_state.chanel != BGM_CHANNEL)
        {
            Mix_HaltChannel(ch->audio_state.chanel);
            ch->audio_state.chanel = -1;
        }

        int kindofSE = GetSE(ch);

        if (kindofSE != -1) {
            int loops = GetSELoop(kindofSE);
            ch->audio_state.isLoopSE = (loops == -1);

            int chn = Mix_PlayChannel(-1, gGame->SE_Load[kindofSE], loops);

            // BGM チャンネル防衛
            if (chn == BGM_CHANNEL) {
                Mix_HaltChannel(chn);
                chn = Mix_PlayChannel(-1, gGame->SE_Load[kindofSE], loops);
            }

            ch->audio_state.chanel = chn;
        }

        // 状態保存
        ch->audio_state.beforeMovestts = ch->movestts;
        ch->audio_state.beforeStts     = ch->stts;
        ch->audio_state.beforeNowstts  = ch->input.nowstts;
        UpdateVolume(myplayer, ch);
    }

}

//===============================
//movesttsに対応する効果音を返す
//===============================
int GetSE(CharaInfo *ch){
    
    switch(ch->movestts){
        
        case ANI_HIT_RIGHT:
        case ANI_HIT_LEFT:
        case ANI_HIT_DOWN:
        case ANI_HIT_UP:
            return GetRandomHitSE();
            break;

        case ANI_RightAttack:
        case ANI_LeftAttack:
        case ANI_DownAttack:
        case ANI_UpAttack:
            return SE_ATTACK;   
            break;

        case ANI_VENDER_PUMP:
            return SE_Vendor;
            break;
        case ANI_ATM_LIGHTOFF_VIBRATE:
            return SE_ATM;
            break;
        case ANI_SPIN:
            if(ch->hitByTrain){
                return SE_HitbyTrain;
            }
            break;
        case ANI_KNOCKDOWN:
            return SE_KnockDown;
            break;
        case ANI_RAISE_RIGHT:
        case ANI_RAISE_LEFT:
        case ANI_RAISE_DOWN:
        case ANI_RAISE_UP:
            if(ch->audio_state.raisePlayed)
                return -1;     
            ch->audio_state.raisePlayed = 1;
                return SE_Raise;
            break;

        default:
            break;  
    }

    switch (ch->stts){
        case CS_Death:
            return SE_Die;
            break;
        default:
            break; 
    }

    switch (ch->input.nowstts) {
        case NOW_INGAME:
            if (ch->audio_state.beforeNowstts != NOW_INGAME)
                return SE_GameStart;
            break;

        default:
            break;;
    }
    return -1;
}




//=================================================
//操作プレイヤーと音源の距離に応じて音量を調整する
//=================================================
#define MAXDistance 1920 //全画面表示のときの画面端まで

void UpdateVolume(CharaInfo *myplayer, CharaInfo *sound_sorce)
{

    //再生していない場合
    if(sound_sorce->audio_state.chanel == -1)
        return;

    //距離に応じて音量を調整する
    //音源と自分の距離を計算（x軸のみ）
    float d_x = myplayer->point.x - sound_sorce->point.x;
    float distance = fabsf(d_x);//絶対値

    float scale = 1.0f - distance / MAXDistance;
    if(scale < 0)
        scale = 0;

    int SoundVolume = (int)(MIX_MAX_VOLUME *scale);

    //自分の音は最大にする
    if(sound_sorce == myplayer)
        SoundVolume = MIX_MAX_VOLUME;

    Mix_Volume(sound_sorce->audio_state.chanel, SoundVolume);//距離に応じた音量に設定
} 


void CloseAudio(void){

    Mix_CloseAudio();
    Mix_Quit();
}

