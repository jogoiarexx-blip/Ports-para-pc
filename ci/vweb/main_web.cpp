#if defined(__EMSCRIPTEN__)
#include "Game.h"
#include <emscripten.h>
#include <chrono>
#include <memory>
#include <algorithm>
#include <cstdio>

static std::unique_ptr<ffx::Game> g;
static double lastMs=0.0;

EM_JS(void, js_ffx_engine_state,
    (int mode,int stage,float camera,float progress,int actors,int enemies,float playerX,int playerHp,int nextAction,int actionCount,int spawnCount,int waiting), {
    if(globalThis.FFXWeb && FFXWeb.updateEngineDebug){
        FFXWeb.updateEngineDebug({
            mode,stage,camera,progress,actors,enemies,playerX,playerHp,nextAction,actionCount,spawnCount,waiting:!!waiting
        });
    }
});

static void frame(){
    if(!g)return;
    double now=emscripten_get_now();
    float dt=lastMs>0?std::clamp((float)((now-lastMs)/1000.0),0.f,.10f):1.f/60.f;
    lastMs=now;
    constexpr float maxStep=1.f/120.f;
    float remaining=dt;int substeps=0;
    while(remaining>0.f&&substeps<12){float step=std::min(remaining,maxStep);g->update(step);remaining-=step;++substeps;}
    g->render();
    js_ffx_engine_state(
        g->webDebugMode(),g->webDebugStageIndex(),g->webDebugCameraX(),g->webDebugScrollProgress(),
        g->webDebugActorCount(),g->webDebugActiveEnemies(),g->webDebugPlayerX(),g->webDebugPlayerHp(),
        g->webDebugNextAction(),g->webDebugActionCount(),g->webDebugSpawnCount(),g->webDebugLevelWaiting());
}

int main(){
    g=std::make_unique<ffx::Game>();
    if(!g->init(nullptr,"/")){
        std::fprintf(stderr,"Final Fight X Web: failed to load /assets/data\n");
        EM_ASM({ if(globalThis.FFXWeb) FFXWeb.bootError('Falha ao carregar os dados do jogo.'); });
        return 2;
    }
    EM_ASM({ if(globalThis.FFXWeb) FFXWeb.bootReady(); });
    emscripten_set_main_loop(frame,0,1);
    return 0;
}
#endif
