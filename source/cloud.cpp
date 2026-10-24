#include "cloud.hpp"
#include "json_util.hpp"
#include <curl/curl.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <cctype>

namespace nx {

namespace {
struct Response { long code{};std::string body;std::map<std::string,std::string>headers; };
struct IO { FILE*file{};const Context*ctx{}; };
size_t receive(char*p,size_t a,size_t b,void*u){auto&r=*static_cast<std::string*>(u);if(r.size()+a*b>8*1024*1024)return 0;r.append(p,a*b);return a*b;}
size_t header(char*p,size_t a,size_t b,void*u){auto&h=*static_cast<std::map<std::string,std::string>*>(u);std::string s(p,a*b);auto c=s.find(':');if(c!=s.npos){auto key=s.substr(0,c);std::transform(key.begin(),key.end(),key.begin(),[](unsigned char x){return std::tolower(x);});auto v=s.substr(c+1);while(!v.empty()&&std::isspace(static_cast<unsigned char>(v.front())))v.erase(v.begin());while(!v.empty()&&std::isspace(static_cast<unsigned char>(v.back())))v.pop_back();h[key]=v;}return a*b;}
size_t readFile(char*p,size_t a,size_t b,void*u){return std::fread(p,a,b,static_cast<IO*>(u)->file)*a;}
size_t writeFile(char*p,size_t a,size_t b,void*u){return std::fwrite(p,a,b,static_cast<IO*>(u)->file)*a;}
int progress(void*u,curl_off_t,curl_off_t,curl_off_t,curl_off_t){auto*c=static_cast<const Context*>(u);return c&&c->cancel&&c->cancel->load()?1:0;}
Response request(const std::string&method,const std::string&url,const std::vector<std::string>&headers,const std::string&body,const Context&ctx,const std::string&upload={},const std::string&download={})
{
    ctx.check();if(!url.starts_with("https://"))throw std::runtime_error("Endereço de conexão inválido.");
    CURL*c=curl_easy_init();if(!c)throw std::runtime_error("Não foi possível iniciar a conexão.");curl_slist*list=nullptr;FILE*f=nullptr;Response r;IO io;
    for(auto&s:headers)list=curl_slist_append(list,s.c_str());
    curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_HTTPHEADER,list);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(c,CURLOPT_CAINFO,"romfs:/certs/cacert.pem");curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,15L);curl_easy_setopt(c,CURLOPT_LOW_SPEED_LIMIT,1L);curl_easy_setopt(c,CURLOPT_LOW_SPEED_TIME,30L);
    curl_easy_setopt(c,CURLOPT_TIMEOUT,(upload.empty()&&download.empty())?60L:0L);curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_USERAGENT,"SaveNX/0.4.1");
    curl_easy_setopt(c,CURLOPT_PROTOCOLS,long(CURLPROTO_HTTPS));curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,header);curl_easy_setopt(c,CURLOPT_HEADERDATA,&r.headers);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(c,CURLOPT_WRITEDATA,&r.body);curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(c,CURLOPT_XFERINFODATA,&ctx);
    if(method=="POST"){curl_easy_setopt(c,CURLOPT_POST,1L);curl_easy_setopt(c,CURLOPT_POSTFIELDS,body.c_str());curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE,body.size());}
    else if(method!="GET")curl_easy_setopt(c,CURLOPT_CUSTOMREQUEST,method.c_str());
    if(!upload.empty()){f=std::fopen(upload.c_str(),"rb");if(f){io.file=f;io.ctx=&ctx;curl_easy_setopt(c,CURLOPT_UPLOAD,1L);curl_easy_setopt(c,CURLOPT_READFUNCTION,readFile);curl_easy_setopt(c,CURLOPT_READDATA,&io);curl_easy_setopt(c,CURLOPT_INFILESIZE_LARGE,curl_off_t(std::filesystem::file_size(upload)));}}
    if(!download.empty()){f=std::fopen(download.c_str(),"wb");if(f){io.file=f;io.ctx=&ctx;curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,writeFile);curl_easy_setopt(c,CURLOPT_WRITEDATA,&io);}}
    bool needsFile=!upload.empty()||!download.empty();CURLcode rc=needsFile&&!f?CURLE_READ_ERROR:curl_easy_perform(c);curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&r.code);
    bool closed=true;if(f){if(!download.empty())closed=std::fflush(f)==0;closed=(std::fclose(f)==0)&&closed;}curl_slist_free_all(list);curl_easy_cleanup(c);
    if(rc!=CURLE_OK||!closed){ctx.check();throw std::runtime_error("Não foi possível concluir a conexão. Verifique a internet.");}return r;
}
void require(const Response&r){if(r.code<200||r.code>=300){log("Google API HTTP "+std::to_string(r.code));throw std::runtime_error(r.code==401||r.code==403?"Reconecte sua conta Google para continuar.":"O Google Drive não concluiu a solicitação.");}}
std::string form(const std::vector<std::pair<std::string,std::string>>&p){std::string r;for(auto&[k,v]:p){if(!r.empty())r+='&';r+=urlEncode(k)+"="+urlEncode(v);}return r;}
std::string query(const std::string&q,const std::string&fields){return "https://www.googleapis.com/drive/v3/files?q="+urlEncode(q)+"&fields="+urlEncode(fields)+"&pageSize=1000";}
std::string quote(std::string s){std::string r;for(char c:s){if(c=='\\'||c=='\'')r+='\\';r+=c;}return r;}
}
void Cloud::loadClient()
{
    auto c=parse(readText("romfs:/application/google-client.json"));if(c){client=str(c.get(),"client_id");secret=str(c.get(),"client_secret");}
    if(client.empty())throw std::runtime_error("A conexão Google ainda não está disponível neste pacote.");
    auto s=parse(readText(std::string(ROOT)+"/config/cloud-session.json"));
    if(!s){s=parse(readText(std::string(ROOT)+"/config/google-drive.json"));if(s&&field(s.get(),"installed")){auto i=field(s.get(),"installed");if(str(i,"client_id")==client)refresh=str(i,"refresh_token");}}
    else if(str(s.get(),"client_id")==client)refresh=str(s.get(),"refresh_token");
}
bool Cloud::resume(const Context&ctx)
{
    loadClient();if(refresh.empty())return false;renew(ctx);linked=true;return true;
}
void Cloud::saveSession(){auto j=object();put(j.get(),"client_id",client);put(j.get(),"refresh_token",refresh);writeText(std::string(ROOT)+"/config/cloud-session.json",encode(j.get()));}
void Cloud::renew(const Context&ctx)
{
    if(!token.empty()&&std::time(nullptr)<expires-45)return;
    if(refresh.empty())throw std::runtime_error("Conecte sua conta Google.");
    auto r=request("POST","https://oauth2.googleapis.com/token",{"Content-Type: application/x-www-form-urlencoded"},form({{"client_id",client},{"client_secret",secret},{"refresh_token",refresh},{"grant_type","refresh_token"}}),ctx);require(r);auto j=parse(r.body);token=str(j.get(),"access_token");
    if(token.empty())throw std::runtime_error("A autorização expirou. Conecte novamente.");expires=std::time(nullptr)+num(j.get(),"expires_in");if(!str(j.get(),"refresh_token").empty()){refresh=str(j.get(),"refresh_token");saveSession();}
}
void Cloud::connect(const Context&ctx)
{
    loadClient();if(!refresh.empty()){try{renew(ctx);linked=true;return;}catch(const std::exception&e){ctx.check();log(std::string("Session renewal: ")+e.what());token.clear();refresh.clear();}}
    auto r=request("POST","https://oauth2.googleapis.com/device/code",{"Content-Type: application/x-www-form-urlencoded"},form({{"client_id",client},{"scope","https://www.googleapis.com/auth/drive.file"}}),ctx);require(r);auto j=parse(r.body);
    auto device=str(j.get(),"device_code"),code=str(j.get(),"user_code");if(device.empty()||code.empty())throw std::runtime_error("Não foi possível iniciar a autorização Google.");
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(std::clamp<int64_t>(num(j.get(),"expires_in"),30,1800));int wait=std::clamp<int64_t>(num(j.get(),"interval"),2,60);
    if(ctx.report)ctx.report({"Autorize sua conta no celular.",code,"https://www.google.com/device",0.1,true});
    while(std::chrono::steady_clock::now()<end){for(int i=0;i<wait*10;i++){ctx.check();std::this_thread::sleep_for(std::chrono::milliseconds(100));}
        auto p=request("POST","https://oauth2.googleapis.com/token",{"Content-Type: application/x-www-form-urlencoded"},form({{"client_id",client},{"client_secret",secret},{"device_code",device},{"grant_type","urn:ietf:params:oauth:grant-type:device_code"}}),ctx);auto v=parse(p.body);auto err=str(v.get(),"error");
        if(err=="authorization_pending")continue;if(err=="slow_down"){wait=std::min(wait+5,60);continue;}if(!err.empty())throw std::runtime_error(err=="access_denied"?"Autorização cancelada no Google.":"A autorização não foi concluída. Tente conectar novamente.");require(p);
        token=str(v.get(),"access_token");refresh=str(v.get(),"refresh_token");expires=std::time(nullptr)+num(v.get(),"expires_in");if(token.empty()||refresh.empty())throw std::runtime_error("O Google não confirmou a autorização.");saveSession();linked=true;return;
    }throw std::runtime_error("O código expirou. Escolha conectar novamente.");
}
std::string Cloud::folder(const std::string&parent,const std::string&name,const Context&ctx,bool create)
{
    renew(ctx);auto h=std::vector<std::string>{"Authorization: Bearer "+token,"Content-Type: application/json"};
    auto r=request("GET",query("trashed=false and '"+quote(parent)+"' in parents and name='"+quote(name)+"' and mimeType='application/vnd.google-apps.folder'","files(id)"),h,{},ctx);require(r);auto j=parse(r.body);auto a=field(j.get(),"files");
    if(a&&json_object_array_length(a))return str(json_object_array_get_idx(a,0),"id");if(!create)return {};
    auto body=object();put(body.get(),"name",name);put(body.get(),"mimeType",std::string("application/vnd.google-apps.folder"));auto parents=json_object_new_array();json_object_array_add(parents,json_object_new_string(parent.c_str()));json_object_object_add(body.get(),"parents",parents);
    r=request("POST","https://www.googleapis.com/drive/v3/files?fields=id",h,encode(body.get()),ctx);require(r);j=parse(r.body);auto id=str(j.get(),"id");if(id.empty())throw std::runtime_error("O Google não confirmou a pasta do backup.");return id;
}
std::string Cloud::targetFolder(const std::string&user,const std::string&title,const std::string&slotKey,const Context&ctx,bool create)
{
    std::string id="root";for(auto&s:std::vector<std::string>{"SaveNX","Backups-v2",user,title,slotKey}){id=folder(id,s,ctx,create);if(id.empty())break;}return id;
}
int Cloud::flush(const Context&ctx)
{
    if(!linked)throw std::runtime_error("Conecte sua conta Google primeiro.");int sent=0;
    for(auto&job:pending()) {ctx.check();const auto parent=targetFolder(job.user,job.title,job.slot,ctx,true);bool completed=false;
        for(int attempt=0;attempt<3&&!completed;attempt++)try{renew(ctx);std::vector<std::string> h={"Authorization: Bearer "+token,"Content-Type: application/json"};
            auto r=request("GET",query("trashed=false and '"+quote(parent)+"' in parents and appProperties has { key='saveNxBackup' and value='"+quote(job.id)+"' }","files(id,size)"),h,{},ctx);require(r);auto existing=parse(r.body);auto files=field(existing.get(),"files");
            if(files&&json_object_array_length(files)){auto f=json_object_array_get_idx(files,0);completed=confirmedUpload(r.code,str(f,"id"),uint64_t(num(f,"size")),job.size);if(!completed)throw std::runtime_error("O arquivo encontrado na nuvem não corresponde ao backup.");}
            else{auto body=object();put(body.get(),"name",job.name);put(body.get(),"mimeType",std::string("application/zip"));auto parents=json_object_new_array();json_object_array_add(parents,json_object_new_string(parent.c_str()));json_object_object_add(body.get(),"parents",parents);auto prop=json_object_new_object();put(prop,"saveNxBackup",job.id);json_object_object_add(body.get(),"appProperties",prop);
                h.push_back("X-Upload-Content-Type: application/zip");h.push_back("X-Upload-Content-Length: "+std::to_string(job.size));
                r=request("POST","https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable&fields=id,size",h,encode(body.get()),ctx);require(r);auto url=r.headers["location"];
                if(!url.starts_with("https://www.googleapis.com/upload/drive/"))throw std::runtime_error("O Google retornou uma sessão de envio inválida.");
                if(ctx.report)ctx.report({"Enviando o backup ao Google Drive...",{},{},0.7,true});r=request("PUT",url,{"Authorization: Bearer "+token,"Content-Type: application/zip"},{},ctx,job.file);require(r);auto result=parse(r.body);
                completed=confirmedUpload(r.code,str(result.get(),"id"),uint64_t(num(result.get(),"size")),job.size);}
            if(!completed)throw std::runtime_error("O Google não confirmou o backup completo.");
        }catch(const std::exception&e){ctx.check();if(attempt==2)throw;log(std::string("Upload retry: ")+e.what());std::this_thread::sleep_for(std::chrono::milliseconds(500*(1<<attempt)));}
        if(completed){acknowledge(job);sent++;}}
    return sent;
}
std::vector<Backup> Cloud::list(const Profile&,const Game&g,const Context&ctx)
{
    std::vector<Backup> out;if(!linked)return out;auto parent=targetFolder(ownerKey(g),"TITLE_"+hex(g.title),slot(g),ctx,false);if(parent.empty())return out;std::string next;
    do{renew(ctx);auto url=query("trashed=false and '"+quote(parent)+"' in parents and mimeType='application/zip'","nextPageToken,files(id,name,size)");if(!next.empty())url+="&pageToken="+urlEncode(next);
        auto r=request("GET",url,{"Authorization: Bearer "+token},{},ctx);require(r);auto j=parse(r.body);auto files=field(j.get(),"files");if(files)for(size_t i=0;i<json_object_array_length(files);i++){auto f=json_object_array_get_idx(files,i);out.push_back({str(f,"name"),{},str(f,"id"),uint64_t(num(f,"size")),true});}next=str(j.get(),"nextPageToken");}while(!next.empty());return out;
}
Backup Cloud::download(const Backup&b,const Context&ctx)
{
    renew(ctx);Backup local=b;local.path=std::string(ROOT)+"/temp/cloud-"+timestamp()+".zip";local.cloud=false;const auto temp=local.path+".partial";
    try{auto r=request("GET","https://www.googleapis.com/drive/v3/files/"+urlEncode(b.id)+"?alt=media",{"Authorization: Bearer "+token},{},ctx,{},temp);require(r);if(r.code!=200||std::filesystem::file_size(temp)!=b.size)throw std::runtime_error("O download ficou incompleto.");std::filesystem::rename(temp,local.path);return local;}catch(...){std::error_code ec;std::filesystem::remove(temp,ec);throw;}
}
}
