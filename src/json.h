#pragma once
#include "common.h"
namespace rc {
struct Json { enum class Type { Null, Bool, Number, String, Array, Object }; Type type{Type::Null}; bool boolean{}; double number{}; std::wstring string; std::vector<Json> array; std::map<std::wstring,Json,std::less<>> object;
  static std::optional<Json> Parse(std::wstring_view input, std::wstring* error=nullptr); std::wstring Dump() const;
  const Json* Find(std::wstring_view key) const; Json& operator[](const std::wstring& key){type=Type::Object;return object[key];}
  std::wstring StringOr(const std::wstring& fallback={}) const{return type==Type::String?string:fallback;} bool BoolOr(bool fallback=false)const{return type==Type::Bool?boolean:fallback;}
};
}
