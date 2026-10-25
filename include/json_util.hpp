#pragma once
#include <json-c/json.h>
#include <memory>
#include <string>
#include <stdexcept>
namespace nx {
struct JsonDrop { void operator()(json_object*p)const{if(p)json_object_put(p);} };
using Json=std::unique_ptr<json_object,JsonDrop>;
inline Json object(){return Json(json_object_new_object());}
inline Json parse(const std::string&s){return Json(json_tokener_parse(s.c_str()));}
inline json_object* field(json_object*p,const char*k){json_object*v=nullptr;if(p)json_object_object_get_ex(p,k,&v);return v;}
inline std::string str(json_object*p,const char*k){auto*v=field(p,k);const char*s=v?json_object_get_string(v):nullptr;return s?s:"";}
inline int64_t num(json_object*p,const char*k){auto*v=field(p,k);return v?json_object_get_int64(v):0;}
inline void put(json_object*p,const char*k,const std::string&v){json_object_object_add(p,k,json_object_new_string(v.c_str()));}
inline void put(json_object*p,const char*k,int64_t v){json_object_object_add(p,k,json_object_new_int64(v));}
inline std::string encode(json_object*p){return json_object_to_json_string_ext(p,JSON_C_TO_STRING_PLAIN);}
}
