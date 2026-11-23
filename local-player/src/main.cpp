#include "skin.hpp"
#include "session.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#ifdef __SWITCH2__
#include <switch.h>
#else
#include <fstream>
static void capture(const Skin& skin,const std::string& path) {
    std::ofstream out(path,std::ios::binary);
    if(!out) throw std::runtime_error("Cannot write screenshot");
    out<<"P6\n1280 720\n255\n";
    for(int y=0;y<720;y++) for(int x=0;x<1280;x++) {
        auto col=skin.pixels[(y/2)*width+x/2];
        const char rgb[]={char(col>>16),char(col>>8),char(col)};out.write(rgb,3);
    }
}
#endif
int main(int argc,char** argv) {
    av_log_set_level(AV_LOG_ERROR);
    try {
        Platform device;device.open();
#ifdef __SWITCH2__
        (void)argc;(void)argv;
        Player player(device,"sdmc:/","sdmc:/music");
        std::string sessionDir="sdmc:/switch/Turnwave";
#else
        std::string dir=argc>1?argv[1]:".";
        while(dir.size()>1 && dir.back()=='/') dir.pop_back();
        Player player(device,dir,dir);
        bool capturing=argc==4 && std::string(argv[2])=="--capture";
        std::string sessionDir=capturing?"":dir;
        if(capturing) player.play(0);
#endif
        if(!sessionDir.empty()) {player.loadExtras(sessionDir);player.loadLibraryState();restoreSession(player,sessionDir); }
        auto skin=std::make_unique<Skin>();
        bool run=true;
        while(run) {
            try {
                for(auto event:device.poll()) {
                    if(event.key==Key::Quit) {run=false;break;}
                    if(event.key==Key::Tap || event.key==Key::Drag) skin->touch(player,event);
                    else player.input(event.key);
                }
                if(!run) break;
                player.tick();
                if(player.audio && !player.coverAttempted) {player.coverAttempted=true;player.audio->loadCover();}
            } catch(const std::exception& error) {player.fail(error.what());}
            skin->draw(player);
            device.present(skin->pixels.data());
#ifndef __SWITCH2__
            if(capturing) {
                if(!player.audio) throw std::runtime_error("Capture needs a playable track: "+player.notice);
                if(player.position>=1.15) {
                    auto prefix=std::string(argv[3]);
                    capture(*skin,prefix+"-player.ppm");
                    player.input(Key::Browse);
                    skin->draw(player);device.present(skin->pixels.data());capture(*skin,prefix+"-files.ppm");
                    player.input(Key::Mix);
                    skin->draw(player);device.present(skin->pixels.data());capture(*skin,prefix+"-mix.ppm");
                    break;
                }
            }
#endif
        }
        if(!sessionDir.empty()) saveSession(player,sessionDir);
        player.saveHistory();
    } catch(const std::exception& e) {
        std::fprintf(stderr,"Turnwave: %s\n",e.what());
#ifdef __SWITCH2__
        if(FILE* log=std::fopen("sdmc:/switch/Turnwave/startup-error.txt","w")) {
            std::fprintf(log,"%s\n",e.what());std::fclose(log);
        }
#endif
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
