#include "core.hpp"
#include "navigation.hpp"
#include "platform.hpp"
#include "cloud.hpp"
#include "json_util.hpp"
#include <switch.h>
#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <curl/curl.h>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <memory>
#include <cstdio>
#include <chrono>

namespace nx {
struct Services
{
    bool account{},ns{},pl{},socket{},nifm{},curl{},romfs{};
    static void check(Result r){if(R_FAILED(r))throw std::runtime_error("Um serviço do Switch não pôde ser iniciado.");}
    Services(){try{check(romfsInit());romfs=true;prepare();check(accountInitialize(AccountServiceType_Administrator));account=true;check(nsInitialize());ns=true;check(plInitialize(PlServiceType_User));pl=true;check(socketInitializeDefault());socket=true;check(nifmInitialize(NifmServiceType_User));nifm=true;if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)throw std::runtime_error("Falha ao preparar a conexão.");curl=true;}catch(...){close();throw;}}
    void close(){if(curl){curl_global_cleanup();curl=false;}if(nifm){nifmExit();nifm=false;}if(socket){socketExit();socket=false;}if(pl){plExit();pl=false;}if(ns){nsExit();ns=false;}if(account){accountExit();account=false;}if(romfs){romfsExit();romfs=false;}}
    ~Services(){close();}
};
class Display
{
    SDL_Window*window{};SDL_Renderer*renderer{};FT_Library library{};FT_Face face{};
    struct Glyph{SDL_Texture*texture{};int left{},top{},w{},h{},advance{};};std::map<uint64_t,Glyph>glyphs;
    Glyph& glyph(uint32_t c,int size)
    {
        uint64_t key=(uint64_t(size)<<32)|c;auto i=glyphs.find(key);if(i!=glyphs.end())return i->second;
        FT_Set_Pixel_Sizes(face,0,size);if(FT_Load_Char(face,c,FT_LOAD_RENDER))FT_Load_Char(face,'?',FT_LOAD_RENDER);
        auto&s=*face->glyph;Glyph g{nullptr,s.bitmap_left,s.bitmap_top,int(s.bitmap.width),int(s.bitmap.rows),int(s.advance.x>>6)};
        if(g.w&&g.h){std::vector<uint32_t>pixels(g.w*g.h);for(int y=0;y<g.h;y++)for(int x=0;x<g.w;x++){auto*row=s.bitmap.buffer+(s.bitmap.pitch>=0?y*s.bitmap.pitch:(g.h-1-y)*-s.bitmap.pitch);pixels[y*g.w+x]=0xffffff00u|row[x];}
            g.texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STATIC,g.w,g.h);if(g.texture){SDL_UpdateTexture(g.texture,nullptr,pixels.data(),g.w*4);SDL_SetTextureBlendMode(g.texture,SDL_BLENDMODE_BLEND);}}
        return glyphs.emplace(key,g).first->second;
    }
    static uint32_t point(std::string_view s,size_t&i)
    {
        auto c=uint8_t(s[i++]);if(c<128)return c;int n=c>=0xf0?3:c>=0xe0?2:c>=0xc2?1:0;if(!n||i+n>s.size())return '?';uint32_t cp=c&((1<<(6-n))-1);
        while(n--){auto t=uint8_t(s[i]);if((t&0xc0)!=0x80)return '?';i++;cp=(cp<<6)|(t&63);}return cp>0x10ffff?'?':cp;
    }
    void text(const Draw&d)
    {
        SDL_Rect clip{d.x,d.y,d.w,d.h};SDL_RenderSetClipRect(renderer,&clip);int pen=d.x;for(size_t i=0;i<d.value.size();){auto c=point(d.value,i);if(c<32)continue;auto&g=glyph(c,d.size);
            if(pen+g.advance>d.x+d.w)break;if(g.texture){SDL_SetTextureColorMod(g.texture,(d.color>>16)&255,(d.color>>8)&255,d.color&255);SDL_Rect dest{pen+g.left,d.y+d.size-g.top,g.w,g.h};SDL_RenderCopy(renderer,g.texture,nullptr,&dest);}pen+=g.advance;}
        SDL_RenderSetClipRect(renderer,nullptr);
    }
public:
    Display()
    {
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0)throw std::runtime_error("Não foi possível iniciar a interface.");
        window=SDL_CreateWindow("SaveNX",0,0,1280,720,SDL_WINDOW_SHOWN);if(window)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(window&&!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
        if(!renderer)throw std::runtime_error("Não foi possível abrir a tela do SaveNX.");SDL_RenderSetLogicalSize(renderer,1280,720);
        PlFontData font{};if(R_FAILED(plGetSharedFontByType(&font,PlSharedFontType_Standard))||FT_Init_FreeType(&library)||FT_New_Memory_Face(library,static_cast<FT_Byte*>(font.address),font.size,0,&face))throw std::runtime_error("Não foi possível carregar a fonte do Switch.");
    }
    ~Display(){for(auto&[key,g]:glyphs)if(g.texture)SDL_DestroyTexture(g.texture);if(face)FT_Done_Face(face);if(library)FT_Done_FreeType(library);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();}
    void render(const Model&m)
    {
        SDL_SetRenderTarget(renderer,nullptr);SDL_RenderSetClipRect(renderer,nullptr);SDL_SetRenderDrawColor(renderer,16,24,39,255);SDL_RenderClear(renderer);
        for(auto&d:frame(m))if(d.text)text(d);else{SDL_SetRenderDrawColor(renderer,(d.color>>16)&255,(d.color>>8)&255,d.color&255,255);SDL_Rect r{d.x,d.y,d.w,d.h};SDL_RenderFillRect(renderer,&r);}SDL_RenderPresent(renderer);
    }
};
enum class Task { Resume,Profiles,Games,Backups,Backup,All,Restore,Connect,Flush };
struct ResultData { std::vector<Profile>profiles;std::vector<Game>games;std::vector<Backup>backups;std::string message,error;int pending{},local{};bool connected{}; };
class App
{
    Model m;Cloud cloud;Thread thread{};bool busy{},quit{};std::atomic_bool cancel{},done{};std::mutex progressLock;Progress current;
    std::function<void()> work;ResultData result;Task task{};Screen messageReturn{Screen::Home};uint64_t hold{};NavigationRepeat navigation;
    static void threadEntry(void*p){auto&a=*static_cast<App*>(p);a.work();a.done.store(true,std::memory_order_release);}
    Context context(){return {&cancel,[this](const Progress&p){std::lock_guard g(progressLock);current=p;}};}
    std::vector<Backup> backups(const Profile&p,const Game&g,const Context&ctx)
    {
        auto b=localBackups(p,g);if(cloud.connected())try{auto remote=cloud.list(p,g,ctx);b.insert(b.end(),remote.begin(),remote.end());}catch(const std::exception&e){ctx.check();log(e.what());result.message="Mostrando cópias locais. A nuvem não respondeu agora.";}return b;
    }
    void flush(const Context&ctx)
    {
        if(!cloud.connected())return;try{int sent=cloud.flush(ctx);if(sent)result.message=std::to_string(sent)+" backups confirmados no Google Drive. Cópias locais preservadas.";}catch(const std::exception&e){ctx.check();log(e.what());result.message="Sua cópia local está salva. O envio ficou pendente.";}
    }
    void start(Task kind)
    {
        if(busy)return;task=kind;cancel=false;done=false;result={};current={"Preparando...",{},{},0.03,true};auto profile=m.profile;auto game=m.game;auto target=m.target;bool upload=m.autoUpload;
        m.authCode.clear();m.authUrl.clear();show(m,kind==Task::Connect?Screen::Cloud:Screen::Working);m.message="Preparando...";
        work=[this,profile,game,target,upload,kind]{auto ctx=context();try{
            switch(kind){case Task::Resume:if(ctx.report)ctx.report({"Verificando a conexão Google...",{},{},0.1,true});cloud.resume(ctx);if(upload)flush(ctx);break;case Task::Profiles:result.profiles=nx::profiles(ctx);break;
            case Task::Games:result.games=nx::games(profile,ctx);if(result.games.empty())result.message="X: buscar novamente. A consulta foi registrada para diagnóstico.";break;
            case Task::Backups:result.backups=backups(profile,game,ctx);break;
            case Task::Backup:nx::backup(profile,game,ctx,false,upload);result.message="Backup local concluído.";if(upload)flush(ctx);result.backups=backups(profile,game,ctx);break;
            case Task::All:{int total=0;for(auto&p:nx::profiles(ctx))if(!p.all)for(auto&g:nx::games(p,ctx)){ctx.check();nx::backup(p,g,ctx,false,upload);total++;}result.message=std::to_string(total)+" backups locais concluídos.";if(upload)flush(ctx);break;}
            case Task::Restore:{auto b=target.cloud?cloud.download(target,ctx):target;nx::restore(profile,game,b,ctx);result.message="Save restaurado. A cópia de segurança foi preservada.";result.backups=localBackups(profile,game);break;}
            case Task::Connect:cloud.connect(ctx);result.message="Google Drive conectado.";flush(ctx);break;
            case Task::Flush:{int sent=cloud.flush(ctx);result.message=sent?std::to_string(sent)+" backups confirmados no Google Drive.":"Nenhum envio pendente. Crie um backup em Meus jogos com X.";break;}}
        }catch(const std::exception&e){if(kind!=Task::Resume)result.error=e.what();log(e.what());}result.connected=cloud.connected();try{result.pending=pendingCount();result.local=backupCount();}catch(const std::exception&e){log(e.what());}};
        if(R_FAILED(threadCreate(&thread,threadEntry,this,nullptr,0x100000,0x2c,1)))throw std::runtime_error("Não foi possível iniciar a tarefa.");
        if(R_FAILED(threadStart(&thread))){threadClose(&thread);throw std::runtime_error("Não foi possível executar a tarefa.");}busy=true;
    }
    void poll()
    {
        if(!busy)return;{std::lock_guard g(progressLock);m.message=current.text;m.progress=current.value;m.canCancel=current.canCancel;m.authCode=current.code;m.authUrl=current.url;}
        if(!done.load(std::memory_order_acquire))return;threadWaitForExit(&thread);threadClose(&thread);busy=false;m.authCode.clear();m.authUrl.clear();m.connected=result.connected;m.pending=result.pending;m.localCount=result.local;
        if(!result.error.empty()){m.message=result.error;messageReturn=(task==Task::Connect||task==Task::Flush)?Screen::Cloud:Screen::Home;show(m,Screen::Message);return;}
        m.message=result.message;
        switch(task){case Task::Resume:show(m,Screen::Home);break;case Task::Profiles:m.profiles=std::move(result.profiles);show(m,Screen::Profiles);break;case Task::Games:m.games=std::move(result.games);show(m,Screen::Games);break;
            case Task::Backups:case Task::Backup:case Task::Restore:m.backups=std::move(result.backups);show(m,Screen::Backups);break;
            case Task::Connect:case Task::Flush:show(m,Screen::Cloud);break;case Task::All:messageReturn=Screen::Home;show(m,Screen::Message);break;}
    }
    void accept()
    {
        switch(m.screen){case Screen::Home:if(m.selected==0)start(Task::Profiles);else if(m.selected==1)start(Task::All);else if(m.selected==2){m.message.clear();show(m,Screen::Cloud);}else show(m,Screen::Settings);break;
        case Screen::Profiles:if(!m.profiles.empty()){m.profile=m.profiles.at(m.selected);start(Task::Games);}break;
        case Screen::Games:if(!m.games.empty()){m.game=m.games.at(m.selected);start(Task::Backups);}break;
        case Screen::Backups:if(!m.backups.empty()){m.target=m.backups.at(m.selected);show(m,Screen::Confirm);}break;
        case Screen::Cloud:start(m.selected?Task::Flush:Task::Connect);break;
        case Screen::Settings:if(m.selected==0){m.autoUpload=!m.autoUpload;auto j=object();put(j.get(),"auto_upload",int64_t(m.autoUpload));writeText(std::string(ROOT)+"/config/settings-v2.json",encode(j.get()));}else{m.message=std::string("SaveNX ")+VERSION+" · Aplicativo original para proteger seus saves.";messageReturn=Screen::Settings;show(m,Screen::Message);}break;
        case Screen::Confirm:if(m.selected==0)show(m,Screen::Backups);break;
        case Screen::Message:show(m,messageReturn);break;default:break;}
    }
public:
    App(){m.pending=pendingCount();m.localCount=backupCount();auto j=parse(readText(std::string(ROOT)+"/config/settings-v2.json"));if(j&&field(j.get(),"auto_upload"))m.autoUpload=num(j.get(),"auto_upload")!=0;start(Task::Resume);}
    ~App(){if(busy){cancel=true;threadWaitForExit(&thread);threadClose(&thread);}}
    bool running()const{return !quit;}
    const Model& model()const{return m;}
    void update(PadState&pad)
    {
        poll();auto down=padGetButtonsDown(&pad),held=padGetButtons(&pad);
        if(busy){navigation.reset();if(m.canCancel&&(down&HidNpadButton_B))cancel=true;return;}
        try{
            if(down&HidNpadButton_Plus){quit=true;return;}
            Direction digital=Direction::None;
            if(held&HidNpadButton_Up)digital=Direction::Up;else if(held&HidNpadButton_Down)digital=Direction::Down;
            else if(held&HidNpadButton_Left)digital=Direction::Left;else if(held&HidNpadButton_Right)digital=Direction::Right;
            auto l=padGetStickPos(&pad,0),r=padGetStickPos(&pad,1);
            navigate(m,navigation.update(navigationDirection(digital,{l.x,l.y},{r.x,r.y}),SDL_GetTicks64()));
            if(down&HidNpadButton_B){switch(m.screen){case Screen::Profiles:case Screen::Cloud:case Screen::Settings:show(m,Screen::Home);break;case Screen::Games:show(m,Screen::Profiles);break;case Screen::Backups:show(m,Screen::Games);break;case Screen::Confirm:show(m,Screen::Backups);break;case Screen::Message:show(m,messageReturn);break;default:break;}return;}
            if(down&HidNpadButton_A)accept();
            if(down&HidNpadButton_X){if(m.screen==Screen::Profiles)start(Task::Profiles);else if(m.screen==Screen::Games&&m.games.empty())start(Task::Games);else if(m.screen==Screen::Games&&!m.games.empty()){m.game=m.games.at(m.selected);start(Task::Backup);}else if(m.screen==Screen::Backups)start(Task::Backup);}
            if(m.screen==Screen::Confirm&&m.selected==1&&(held&HidNpadButton_A)){auto now=armGetSystemTick();if(!hold)hold=now;if(now-hold>armGetSystemTickFreq()*3/2){hold=0;start(Task::Restore);}}else hold=0;
        }catch(const std::exception&e){m.message=e.what();messageReturn=Screen::Home;show(m,Screen::Message);}
    }
};
}
int main(int,char**)
{
    try{nx::Services services;nx::Display display;nx::App app;PadState pad{};padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);
        while(appletMainLoop()&&app.running()){SDL_Event event;while(SDL_PollEvent(&event)){}padUpdate(&pad);app.update(pad);display.render(app.model());}return 0;
    }catch(const std::exception&e){consoleInit(nullptr);std::printf("SaveNX: %s\nPressione + para sair.\n",e.what());PadState pad{};padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);while(appletMainLoop()){padUpdate(&pad);if(padGetButtonsDown(&pad)&HidNpadButton_Plus)break;consoleUpdate(nullptr);}consoleExit(nullptr);return 1;}
}
