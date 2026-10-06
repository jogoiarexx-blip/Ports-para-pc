#if defined(__EMSCRIPTEN__)
#include "Game.h"
#include <emscripten.h>
#include <chrono>
#include <memory>
#include <algorithm>
#include <cstdio>

static std::unique_ptr<ffx::Game> g;
static double lastMs=0.0;

static void frame(){
    if(!g)return;
    double now=emscripten_get_now();
    float dt=lastMs>0?std::clamp((float)((now-lastMs)/1000.0),0.f,.10f):1.f/60.f;
    lastMs=now;
    constexpr float maxStep=1.f/120.f;
    float remaining=dt;int substeps=0;
    while(remaining>0.f&&substeps<12){float step=std::min(remaining,maxStep);g->update(step);remaining-=step;++substeps;}
    g->render();
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
