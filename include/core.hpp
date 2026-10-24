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
inline constexpr const char *VERSION = "0.4.0";
inline constexpr const char *ROOT = "sdmc:/switch/SaveNX";
enum class Screen { Home, Profiles, Games, Backups, Cloud, Settings, Confirm, Working, Message };
struct Profile { std::array<uint64_t,2> uid{}; std::string name,key; bool shared{}; };
struct Game { uint64_t title{},saveId{}; std::array<uint64_t,2> uid{}; int type{},rank{},index{}; std::string name; };
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
inline std::string slot(const Game& g) { return "TYPE_"+std::to_string(g.type)+"_RANK_"+std::to_string(g.rank)+"_INDEX_"+std::to_string(g.index); }
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
inline bool sameSlot(const Game& a,const Game& b) { return a.title==b.title&&a.uid==b.uid&&a.type==b.type&&a.rank==b.rank&&a.index==b.index; }
inline int count(const Model& m)
{
    switch(m.screen) { case Screen::Home:return 4; case Screen::Profiles:return m.profiles.size(); case Screen::Games:return m.games.size();
        case Screen::Backups:return m.backups.size();case Screen::Cloud:return 2;case Screen::Settings:return 2;case Screen::Confirm:return 2;default:return 0; }
}
inline void move(Model& m,int delta) { int n=count(m); m.selected=n?(m.selected+delta+n)%n:0; }
inline void show(Model& m,Screen s) { m.screen=s;m.selected=0; }
struct Draw { bool text{}; int x{},y{},w{},h{},size{}; uint32_t color{}; std::string value; };
inline std::vector<Draw> frame(const Model& m)
{
    std::vector<Draw> d;
    auto box=[&](int x,int y,int w,int h,uint32_t c){d.push_back({false,x,y,w,h,0,c,{}});};
    auto text=[&](int x,int y,int w,int size,std::string s,uint32_t c=0xe7edf7){d.push_back({true,x,y,w,size+12,size,c,std::move(s)});};
    constexpr uint32_t bg=0x101827,panel=0x19263b,muted=0x9aaac1,teal=0x37dec0;
    box(0,0,1280,720,bg); box(0,0,230,720,0x0b1220);box(252,99,1000,1,0x293950);
    box(28,32,42,42,teal);text(38,37,28,28,"S",0x0b1220);text(82,41,140,27,"SaveNX");
    const char* nav[]={"Início","Jogos e perfis","Google Drive","Ajustes"};
    int active=m.screen==Screen::Home?0:(m.screen==Screen::Cloud?2:(m.screen==Screen::Settings?3:1));
    for(int i=0;i<4;i++){if(i==active)box(14,142+i*65,202,48,panel);text(30,151+i*65,180,21,nav[i],i==active?teal:muted);}
    text(28,644,190,16,"Seus saves, com você.",muted);text(28,679,190,15,std::string("SaveNX ")+VERSION,muted);
    text(280,35,650,30,m.screen==Screen::Home?"Seu progresso, protegido.":m.screen==Screen::Profiles?"Escolha um perfil":m.screen==Screen::Games?"Seus jogos":m.screen==Screen::Backups?"Backups deste jogo":m.screen==Screen::Cloud?"Sua nuvem":m.screen==Screen::Settings?"Do seu jeito":m.screen==Screen::Confirm?"Restaurar este backup?":m.screen==Screen::Working?"Cuidando dos seus saves":"SaveNX");
    text(970,46,270,18,m.connected?"Google Drive conectado":"Google Drive desconectado",m.connected?teal:muted);
    if(m.screen==Screen::Home)
    {
        text(282,130,900,22,"Uma cópia no seu Switch. Outra, na sua conta Google.",muted);
        const char* title[]={"Meus jogos","Proteger todos","Google Drive","Ajustes"};
        const char* sub[]={"Backup e restauração por jogo.","Crie uma cópia dos saves deste console.","Autorize sua conta e envie seus backups.","Escolha como seus backups são enviados."};
        for(int i=0;i<4;i++){int x=280+(i%2)*482,y=196+(i/2)*163;box(x,y,456,139,i==m.selected?0x214d55:panel);
            if(i==m.selected)box(x,y,5,139,teal);text(x+24,y+23,405,27,title[i]);text(x+24,y+75,400,18,sub[i],muted);}
        text(282,557,920,20,"Backups locais: "+std::to_string(m.localCount)+"     Envios pendentes: "+std::to_string(m.pending),muted);
    }
    else if(m.screen==Screen::Profiles||m.screen==Screen::Games||m.screen==Screen::Backups)
    {
        text(282,124,920,20,m.screen==Screen::Profiles?"Cada perfil mantém seus próprios backups.":m.screen==Screen::Games?m.profile.name:m.game.name,muted);
        int n=count(m),first=(m.selected/6)*6;
        if(n==0)text(300,235,880,25,m.screen==Screen::Backups?"Nenhum backup encontrado para este jogo.":"Nenhum save encontrado.");
        for(int i=first;i<n&&i<first+6;i++)
        {
            int y=180+(i-first)*66;box(280,y,940,55,i==m.selected?0x214d55:panel);
            std::string s=m.screen==Screen::Profiles?m.profiles[i].name:m.screen==Screen::Games?m.games[i].name:m.backups[i].name;
            if(m.screen==Screen::Backups)s=std::string(m.backups[i].cloud?"Nuvem  ·  ":"Local  ·  ")+s;
            text(300,y+11,900,23,s);
        }
        if(n)text(282,585,900,18,std::to_string(m.selected+1)+" de "+std::to_string(n),muted);
        if(!m.message.empty())text(282,618,935,16,m.message,teal);
    }
    else if(m.screen==Screen::Cloud)
    {
        box(280,140,940,430,panel);text(310,168,870,28,m.connected?"Sua conta está conectada":"Conecte seu Google Drive",teal);
        if(!m.authCode.empty()) {text(310,230,870,21,"No celular ou computador, abra:",muted);text(310,273,870,27,m.authUrl);text(310,327,870,44,m.authCode,teal);text(310,394,870,21,"Digite esse código e autorize o SaveNX.",muted);}
        else {text(310,235,870,22,"Você autoriza pelo site oficial do Google.",muted);text(310,279,870,22,"A senha da sua conta fica com você.",muted);}
        if(m.authCode.empty())for(int i=0;i<2;i++){box(310+i*440,475,410,58,i==m.selected?0x214d55:0x101827);text(330+i*440,490,370,21,i?"Enviar pendentes":m.connected?"Verificar conexão":"Conectar Google Drive");}
        else text(310,491,870,21,"Aguardando sua autorização...     B: cancelar",teal);
        if(!m.message.empty())text(282,600,935,18,m.message,muted);
    }
    else if(m.screen==Screen::Settings)
    {
        const std::string labels[]={std::string("Enviar backups à nuvem: ")+(m.autoUpload?"ativado":"desativado"),"Sobre o SaveNX"};
        for(int i=0;i<2;i++){box(280,170+i*82,940,64,i==m.selected?0x214d55:panel);text(304,187+i*82,890,23,labels[i]);}
        text(282,385,920,20,"As cópias locais e a proteção antes de restaurar ficam sempre ativas.",muted);
    }
    else if(m.screen==Screen::Confirm)
    {
        box(280,145,940,420,panel);text(310,180,870,25,m.game.name);text(310,238,870,20,m.target.name,muted);
        text(310,302,870,22,"O save atual será copiado antes da restauração.",muted);
        const char* labels[]={"Voltar","Restaurar — segure A"};
        for(int i=0;i<2;i++){box(310+i*440,428,410,70,m.selected==i?(i?0x713b45:0x214d55):bg);text(330+i*440,451,365,22,labels[i]);}
    }
    else
    {
        box(280,160,940,350,panel);text(310,206,870,25,m.message);
        if(m.screen==Screen::Working){box(310,300,860,12,0x293950);box(310,300,int(860*std::clamp(m.progress,0.0,1.0)),12,teal);text(310,353,870,20,m.canCancel?"B para cancelar. Sua cópia local será preservada.":"Finalizando a gravação. Aguarde nesta tela.",muted);}
        else text(310,360,870,21,"Pressione A ou B para voltar.",teal);
    }
    box(252,652,1000,1,0x293950);
    text(280,676,950,18,m.screen==Screen::Games?"A: ver backups     X: novo backup     B: voltar":m.screen==Screen::Backups?"A: restaurar     X: novo backup     B: voltar":"A: selecionar     B: voltar     +: sair",muted);
    return d;
}
}
