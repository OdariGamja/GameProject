//client_system.c

#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_LINEBUF 256

GameInfo gGames[CHARATYPE_NUM];  

CharaInfo* gCharaHead = NULL;  
CharaTypeInfo gCharaType[CHARATYPE_NUM];
char gImgFilePath[CHARATYPE_NUM][MAX_PATH];

typedef enum { AD_LR, AD_UD, AD_NONE } AdjustDir;

int PrintError(const char* msg) {
    fprintf(stderr, "Error: %s\n", msg);
    return -1;
}

int InitSystem(const char* chara_data_file, const char* position_data_file, int my_id) {
    int ret = 0;
    FILE* fp = fopen(chara_data_file, "r");
    if (!fp) return PrintError("failed to open chara data file.");

    int typeno = 0;
    char linebuf[MAX_LINEBUF];
    while (fgets(linebuf, MAX_LINEBUF, fp)) {
        if (linebuf[0] == '#') continue;
        if (typeno < CHARATYPE_NUM) {
            if (3 != sscanf(linebuf, "%d%d%s",
                            &gCharaType[typeno].w,
                            &gCharaType[typeno].h,
                            gImgFilePath[typeno])) {
                ret = PrintError("failed to read the chara image data.");
                goto CLOSE_CHARA;
            }
            gCharaType[typeno].path = gImgFilePath[typeno];
            typeno++;
        }
    }
CLOSE_CHARA:
    fclose(fp);
    if (ret < 0) return ret;

    fp = fopen(position_data_file, "r");
    if (!fp) return PrintError("failed to open position data file.");

    gCharaHead = NULL;
    for (int i = 0; i < CHARATYPE_NUM; i++) {
        gGames[i].player = NULL;
        memset(&gGames[i].input, 0, sizeof(gGames[i].input));
    }

    while (fgets(linebuf, MAX_LINEBUF, fp)) {
        if (linebuf[0] == '#') continue;
        CharaInfo* ch = malloc(sizeof(CharaInfo));
        if (!ch) { ret = PrintError("failed to allocate chara"); goto CLOSE_POSITION; }
        ch->next = gCharaHead;
        gCharaHead = ch;

        if (7 != sscanf(linebuf, "%u%f%f%d%d%d%d",
                        (unsigned int*)&ch->type,
                        &ch->point.x,
                        &ch->point.y,
                        &ch->rect.x,
                        &ch->rect.y,
                        &ch->rect.w,
                        &ch->rect.h)) {
            ret = PrintError("failed to load position data"); goto CLOSE_POSITION;
        }
        if (ch->type >= CHARATYPE_NUM) { free(ch); continue; }
        ch->entity = &gCharaType[ch->type];
        InitCharaInfo(ch);
    }

    // 現在のプレイヤー設定
    GameInfo* game = &gGames[my_id];
    game->player = NULL;
    for (CharaInfo* ch = gCharaHead; ch; ch = ch->next) {
        if (ch->type == CT_PLAYER0 + my_id) {
            game->player = ch;
            break;
        }
    }
    if (!game->player) ret = PrintError("Player init failed");

CLOSE_POSITION:
    fclose(fp);
    return ret;
}


//オブゼット状態初期化
void InitCharaInfo(CharaInfo* ch) {
    if (!ch || ch->type >= CHARATYPE_NUM) return;
    ch->entity = &gCharaType[ch->type];
    ch->stts = CS_Normal;
    ch->point.x = (int)ch->point.x;
    ch->point.y = (int)ch->point.y;
    ch->ani.x = 0;
    ch->rect.w = ch->entity->w;
    ch->rect.h = ch->entity->h;
    
    ch->rect.x = (int)ch->point.x;
    if(ch->type >= CT_PLAYER0 && ch->type <= CT_PLAYER3){
        ch->rect.y = 150 + (int)ch->point.y;    //player だけ+150
    }else{
        ch->rect.y = (int)ch->point.y;   
    }
    
    ch->score = 0;
    ch->hitcount = 0;
    ch->deathcount = 0;
    
    ch->imgsrc.x = ch->imgsrc.y = 0;
    ch->imgsrc.w = ch->entity->w;
    ch->imgsrc.h = ch->entity->h;
    ch->holder = NULL;
    ch->holdTarget = NULL;
    ch->prevMovestts = ch->movestts;
    ch->animPlayed   = 0;
    ch->railroadflag = 0;
    ch->railFallRemain = 0;
    ch->hitByTrain = 0;
    ch->hitByTrainStartMs = 0.0f;
    ch->hitByTrainDurationMs  = 0.0f;
    ch->input.nowstts =0;
    ch->gameTime = 0.0f;
    ch->IntheHole = 0;
    ch->audio_state.raisePlayed = 0;
}

SDL_bool InputEvent(GameInfo* game)
{
    if (!game) return SDL_FALSE;

    SDL_Event e;
    SDL_bool running = SDL_TRUE;

    // 毎フレーム spacePressed をリセット
    game->input.spacePressed = 0;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {

        case SDL_QUIT:
            running = SDL_FALSE;
            break;

        case SDL_KEYDOWN:
            // リピートは無視
            if (e.key.repeat) break;

            switch (e.key.keysym.sym) {
            case SDLK_SPACE:
                if (!game->input.space) {
                    game->input.spacePressed = 1; // 押した瞬間
                }
                game->input.space = 1;
                break;

            case SDLK_d: game->input.right = 1; break;
            case SDLK_a: game->input.left  = 1; break;
            case SDLK_s: game->input.down  = 1; break;
            case SDLK_w: game->input.up    = 1; break;
            case SDLK_ESCAPE: running = SDL_FALSE; break;
            }
            break;

        case SDL_KEYUP:
            switch (e.key.keysym.sym) {
            case SDLK_SPACE:
                game->input.space = 0;
                break;
            case SDLK_d: game->input.right = 0; break;
            case SDLK_a: game->input.left  = 0; break;
            case SDLK_s: game->input.down  = 0; break;
            case SDLK_w: game->input.up    = 0; break;
            }
            break;
        }
    }

    return running;
}


// 自分のプレイヤーIDに対応するキャラクターをリンクリストから探して返す
// 見つからなければ NULL を返す
CharaInfo* GetPlayerChara(int my_id) {
    CharaInfo* ch = gCharaHead; 
    int type = CT_PLAYER0 + my_id;
    while (ch) {
        if (ch->type == type)
            return ch;
        ch = ch->next;
    }
    return NULL; 
}



void DestroySystem(void) {
    CharaInfo* ch = gCharaHead;
    while (ch) {
        CharaInfo* next = ch->next;
        free(ch);
        ch = next;
    }
    gCharaHead = NULL;
}

