#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace nx
{
inline constexpr const char *VERSION = "0.4.3";
inline constexpr const char *ROOT = "sdmc:/switch/SaveNX";
enum class Screen { Home, Profiles, Games, Backups, Cloud, Settings, Confirm, Working, Message };
struct Profile { std::array<uint64_t,2> uid{}; std::string name,key; bool shared{},all{}; int saves{}; };
struct Game { uint64_t title{},saveId{}; std::array<uint64_t,2> uid{}; int type{},rank{},index{}; std::string name; int space{1}; std::string owner{}; };
struct Backup { std::string name,path,id; uint64_t size{}; bool cloud{}; };
struct Model
{
    Screen screen{Screen::Home}; int selected{};
    std::vector<Profile> profiles; std::vector<Game> games; std::vector<Backup> backups;
    Profile profile; Game game; Backup target;
    bool connected{},autoUpload{true},canCancel{true}; double progress{};
    std::string message,authCode,authUrl; int localCount{},pending{};
};
inline std::string hex(uint64_t n) { std::ostringstream s; s<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(16)<<n; return s.str(); }
inline std::string uid(const std::array<uint64_t,2>& u) { return hex(u[1])+hex(u[0]); }
inline std::string ownerKey(const Game&g) { return g.type==3?"DEVICE":"USER_"+uid(g.uid); }
inline std::string slot(const Game& g) { return "TYPE_"+std::to_string(g.type)+"_RANK_"+std::to_string(g.rank)+"_INDEX_"+std::to_string(g.index)+(g.space==1?"":"_SPACE_"+std::to_string(g.space)); }
inline bool owns(const Profile&p,const Game&g) { return p.all||(p.shared?g.type==3:(g.type==1&&p.uid==g.uid)); }
inline bool relative(std::string_view p)
{
    if(p.empty()||p.front()=='/'||p.find('\\')!=p.npos||p.find(':')!=p.npos||p.find('\0')!=p.npos||p.find("//")!=p.npos)return false;
    if(p.back()=='/')p.remove_suffix(1);
    if(p.empty())return false;
    while(!p.empty()) { auto n=p.find('/'); auto c=p.substr(0,n); if(c.empty()||c=="."||c=="..")return false;
        if(n==p.npos)break; p.remove_prefix(n+1); }
    return true;
}
inline bool safeKey(std::string_view s)
{
    return !s.empty()&&s.size()<240&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-';});
}
inline bool strictHex(std::string_view s,size_t length)
{
    return s.size()==length&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f');});
}
inline std::string urlEncode(std::string_view s)
{
    static const char*h="0123456789ABCDEF";std::string r;
    for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')r+=char(c);else{r+='%';r+=h[c>>4];r+=h[c&15];}}return r;
}
inline bool confirmedUpload(long http,std::string_view id,uint64_t reported,uint64_t expected)
{
    return (http==200||http==201)&&!id.empty()&&expected>0&&reported==expected;
}
inline bool archiveEntry(std::string_view p) { return p==".savenx/manifest.json" || (p.starts_with("save/")&&(p=="save/"||relative(p.substr(5)))); }
inline bool sameSlot(const Game& a,const Game& b) { return a.title==b.title&&a.uid==b.uid&&a.type==b.type&&a.rank==b.rank&&a.index==b.index&&a.space==b.space; }
inline int count(const Model& m)
{
    switch(m.screen) { case Screen::Home:return 4; case Screen::Profiles:return m.profiles.size(); case Screen::Games:return m.games.size();
        case Screen::Backups:return m.backups.size();case Screen::Cloud:return 2;case Screen::Settings:return 3;case Screen::Confirm:return 2;default:return 0; }
}
inline void move(Model& m,int delta) { int n=count(m); m.selected=n?(m.selected+delta+n)%n:0; }
inline void show(Model& m,Screen s) { m.screen=s;m.selected=0; }
struct Draw { bool text{}; int x{},y{},w{},h{},size{}; uint32_t color{}; std::string value; };
inline std::vector<Draw> frame(const Model& m)
{
    std::vector<Draw> d;
    auto box=[&](int x,int y,int w,int h,uint32_t c){d.push_back({false,x,y,w,h,0,c,{}});};
    auto text=[&](int x,int y,int w,int size,std::string s,uint32_t c=0xe7edf7){d.push_back({true,x,y,w,size+12,size,c,std::move(s)});};
    constexpr uint32_t bg=0x101827,panel=0x19263b,muted=0x9aaac1,teal=0x37dec0,chosen=0x214d55;
    // Every frame is a complete scene. There is no decorative or inaccessible sidebar.
    box(0,0,1280,720,bg);box(48,26,44,44,teal);text(59,32,28,28,"S",0x0b1220);
    text(108,28,300,28,"SaveNX");text(110,67,350,15,std::string("Seus saves, com você.  ·  ")+VERSION,muted);
    text(936,45,296,18,m.connected?"Google Drive conectado":"Google Drive desconectado",m.connected?teal:muted);
    box(48,102,1184,1,0x293950);
    text(48,124,1184,32,m.screen==Screen::Home?"Seu progresso, protegido.":m.screen==Screen::Profiles?"Escolha seus saves":m.screen==Screen::Games?"Seus jogos":m.screen==Screen::Backups?"Backups deste jogo":m.screen==Screen::Cloud?"Sua nuvem":m.screen==Screen::Settings?"Do seu jeito":m.screen==Screen::Confirm?"Restaurar este backup?":m.screen==Screen::Working?"Cuidando dos seus saves":"SaveNX");
    if(m.screen==Screen::Home)
    {
        text(50,177,1180,21,"Uma cópia no seu Switch. Outra, na sua conta Google.",muted);
        const char* title[]={"Meus jogos","Proteger todos","Google Drive","Ajustes"};
        const char* sub[]={"Escolha um save para copiar ou restaurar.","Crie uma cópia dos saves deste console.","Conecte sua conta e envie seus backups.","Escolha como seus backups são enviados."};
        for(int i=0;i<4;i++){
            int x=48+(i%2)*604,y=228+(i/2)*180;
            box(x,y,580,156,i==m.selected?chosen:panel);if(i==m.selected)box(x,y,5,156,teal);
            text(x+28,y+24,524,29,title[i]);text(x+28,y+85,524,20,sub[i],muted);
        }
        text(50,604,1180,20,"Backups locais: "+std::to_string(m.localCount)+"     Envios pendentes: "+std::to_string(m.pending),muted);
    }
    else if(m.screen==Screen::Profiles||m.screen==Screen::Games)
    {
        int n=count(m),first=(m.selected/4)*4;
        text(50,177,1180,21,m.screen==Screen::Profiles?"Todos os saves ou somente os de um perfil.":m.profile.name,muted);
        if(!n){text(76,288,1128,25,"Nenhum save de jogo encontrado neste ambiente.");text(76,348,1128,21,"Abra o SaveNX no mesmo sistema em que você joga. X: atualizar lista.",muted);}
        for(int i=first;i<n&&i<first+4;i++){
            int cell=i-first,x=48+(cell%2)*604,y=228+(cell/2)*180;
            box(x,y,580,156,i==m.selected?chosen:panel);if(i==m.selected)box(x,y,5,156,teal);
            auto name=m.screen==Screen::Profiles?m.profiles[i].name:m.games[i].name;
            auto detail=m.screen==Screen::Profiles?std::to_string(m.profiles[i].saves)+" saves encontrados":m.games[i].owner;
            if(m.screen==Screen::Games&&m.games[i].index)detail+="  ·  espaço "+std::to_string(m.games[i].index);
            if(m.screen==Screen::Games&&m.games[i].rank)detail+="  ·  cópia alternativa";
            text(x+28,y+27,524,25,name);text(x+28,y+88,524,20,detail,muted);
        }
        if(n)text(50,592,200,18,std::to_string(m.selected+1)+" de "+std::to_string(n),muted);
        if(m.screen==Screen::Games&&n)text(274,592,958,18,m.games[m.selected].name,muted);
        if(!m.message.empty())text(50,621,1180,16,m.message,teal);
    }
    else if(m.screen==Screen::Backups)
    {
        text(50,177,1180,21,m.game.name,muted);int n=count(m),first=(m.selected/6)*6;
        if(!n)text(76,285,1128,25,"Nenhum backup encontrado. Pressione X para criar uma cópia.");
        for(int i=first;i<n&&i<first+6;i++){
            int y=216+(i-first)*62;box(48,y,1184,54,i==m.selected?chosen:panel);
            text(72,y+10,1136,23,std::string(m.backups[i].cloud?"Nuvem  ·  ":"Local  ·  ")+m.backups[i].name);
        }
        if(n)text(50,604,210,18,std::to_string(m.selected+1)+" de "+std::to_string(n),muted);
        if(!m.message.empty())text(284,604,948,18,m.message,teal);
    }
    else if(m.screen==Screen::Cloud)
    {
        text(50,177,1180,21,"Suas cópias ficam na sua conta Google.",muted);
        box(48,226,1184,364,panel);text(80,250,1120,28,m.connected?"Sua conta está conectada":"Conecte seu Google Drive",teal);
        if(!m.authCode.empty()){
            text(80,307,1120,21,"No celular ou computador, abra:",muted);text(80,351,1120,27,m.authUrl);
            text(80,402,1120,44,m.authCode,teal);text(80,471,1120,21,"Digite esse código e autorize o SaveNX.",muted);
            text(80,535,1120,21,"Aguardando sua autorização...     B: cancelar",teal);
        }else{
            text(80,321,1120,22,m.connected?"Cópias locais concluídas: "+std::to_string(m.localCount):"Você autoriza pelo site oficial do Google.",muted);text(80,366,1120,22,m.connected?"Envios pendentes: "+std::to_string(m.pending):"A senha da sua conta fica com você.",muted);
            for(int i=0;i<2;i++){int x=80+i*584;box(x,494,536,64,i==m.selected?chosen:bg);text(x+24,513,488,22,i?"Enviar pendentes":m.connected?"Verificar conexão":"Conectar Google Drive");}
        }
        if(!m.message.empty())text(50,621,1180,16,m.message,muted);
    }
    else if(m.screen==Screen::Settings)
    {
        text(50,177,1180,21,"Limite automático: três backups por jogo, perfil e espaço, no cartão e na nuvem.",muted);
        const std::string titles[]={"Envio automático","Histórico de backups","Sobre o SaveNX"};
        const std::string values[]={m.autoUpload?"Ativado":"Desativado","Manter as três últimas cópias",std::string("Versão ")+VERSION};
        const char* descriptions[]={"Enviar à nuvem após cada novo backup.","Organizar também as cópias anteriores.","Aplicativo original para seus saves."};
        for(int i=0;i<3;i++){
            int x=48+(i%2)*604,y=228+(i/2)*180;box(x,y,580,156,i==m.selected?chosen:panel);if(i==m.selected)box(x,y,5,156,teal);
            text(x+28,y+21,524,27,titles[i]);text(x+28,y+68,524,21,values[i],teal);text(x+28,y+115,524,18,descriptions[i],muted);
        }
    }
    else if(m.screen==Screen::Confirm)
    {
        box(48,220,1184,344,panel);text(80,246,1120,25,m.game.name);text(80,297,1120,20,m.target.name,muted);
        text(80,351,1120,22,"O save atual será copiado antes da restauração.",muted);
        const char* labels[]={"Voltar","Restaurar — segure A"};
        for(int i=0;i<2;i++){int x=80+i*584;box(x,445,536,74,m.selected==i?(i?0x713b45:chosen):bg);text(x+24,469,488,23,labels[i]);}
    }
    else
    {
        box(48,224,1184,330,panel);text(80,270,1120,25,m.message);
        if(m.screen==Screen::Working){
            box(80,351,1120,12,0x293950);box(80,351,int(1120*std::clamp(m.progress,0.0,1.0)),12,teal);
            text(80,411,1120,21,m.canCancel?"B para interromper as próximas etapas.":"Finalizando a gravação. Aguarde nesta tela.",muted);
        }else text(80,424,1120,22,"Pressione A ou B para voltar.",teal);
    }
    box(48,652,1184,1,0x293950);
    const char* footer=m.screen==Screen::Games?"Manches ou direcional: navegar     A: ver backups     X: novo backup     B: voltar":m.screen==Screen::Profiles?"Manches ou direcional: navegar     A: selecionar     X: atualizar lista     B: voltar":m.screen==Screen::Backups?"Manches ou direcional: navegar     A: restaurar     X: novo backup     B: voltar":"Manches ou direcional: navegar     A: selecionar     B: voltar     +: sair";
    text(50,677,1180,18,footer,muted);return d;
}
}
