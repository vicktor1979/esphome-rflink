
#pragma once
// HOST TEST DOUBLE ONLY. Implements just flat fixture objects. Production uses
// ESPHome/ArduinoJson; this is NOT shipped as a firmware component.
#include <map>
#include <string>
#include <functional>
#include <cctype>
#include <type_traits>
struct JsonToken {
 std::string text; bool string_type=false, is_null=true;
 template<class T> bool is()const{return std::is_same<T,const char*>::value && string_type && !is_null;}
 bool isNull()const{return is_null;}
 const char* operator|(const char* fallback)const{return string_type && !is_null ? text.c_str() : fallback;}
};
struct JsonObject {std::map<std::string,JsonToken> fields; JsonToken operator[](const char*k)const{auto p=fields.find(k);return p==fields.end()?JsonToken{}:p->second;}};
namespace esphome { namespace json {
inline bool parse_json(const std::string&s,const std::function<bool(JsonObject)>&callback){
 size_t pos=0;auto ws=[&](){while(pos<s.size() && std::isspace(static_cast<unsigned char>(s[pos])))++pos;};
 auto text=[&](std::string &v)->bool{if(pos>=s.size() || s[pos++]!='"')return false;while(pos<s.size()){char c=s[pos++];if(c=='"')return true;if(c=='\\'){if(pos==s.size())return false;c=s[pos++];}v+=c;}return false;};
 ws();if(pos==s.size()||s[pos++]!='{')return false;JsonObject root;ws();
 while(pos<s.size()&&s[pos]!='}'){
   std::string k;ws();if(!text(k))return false;ws();if(pos==s.size()||s[pos++]!=':')return false;ws();
   JsonToken token;
   if(pos<s.size()&&s[pos]=='"'){if(!text(token.text))return false;token.string_type=true;token.is_null=false;}
   else{while(pos<s.size()&&s[pos]!=','&&s[pos]!='}')token.text+=s[pos++];token.is_null=token.text=="null";}
   if(root.fields.count(k)) return false;
   root.fields.emplace(k,std::move(token));ws();
   if(pos<s.size()&&s[pos]==','){++pos;ws();continue;}break;
 }
 if(pos==s.size()||s[pos++]!='}') return false;
 ws();if(pos!=s.size())return false;return callback(root);
}
} }
