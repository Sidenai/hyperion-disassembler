#include "lua_engine.h"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace hype {

namespace {

LuaEngine* get_engine(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "__hype_engine");
    auto* eng = static_cast<LuaEngine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return eng;
}

AnalysisDB* get_db(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "__hype_db");
    auto* db = static_cast<AnalysisDB*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return db;
}

PEImage* get_img(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "__hype_img");
    auto* img = static_cast<PEImage*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return img;
}

int l_get_name(lua_State* L) {
    auto* db = get_db(L);
    if (!db) { lua_pushnil(L); return 1; }
    va_t addr = static_cast<va_t>(luaL_checkinteger(L, 1));
    auto it = db->names.find(addr);
    if (it != db->names.end())
        lua_pushstring(L, it->second.c_str());
    else
        lua_pushnil(L);
    return 1;
}

int l_set_name(lua_State* L) {
    auto* db = get_db(L);
    if (!db) return 0;
    va_t addr = static_cast<va_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    db->set_name(addr, name);
    return 0;
}

int l_get_func(lua_State* L) {
    auto* db = get_db(L);
    if (!db) { lua_pushnil(L); return 1; }
    va_t addr = static_cast<va_t>(luaL_checkinteger(L, 1));
    auto it = db->funcs.find(addr);
    if (it == db->funcs.end()) { lua_pushnil(L); return 1; }

    lua_newtable(L);
    lua_pushstring(L, it->second.name.c_str());
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, static_cast<lua_Integer>(it->second.entry));
    lua_setfield(L, -2, "entry");
    lua_pushinteger(L, static_cast<lua_Integer>(it->second.blocks.size()));
    lua_setfield(L, -2, "block_count");
    lua_pushboolean(L, it->second.noreturn);
    lua_setfield(L, -2, "noreturn");
    return 1;
}

int l_get_insn(lua_State* L) {
    auto* db = get_db(L);
    if (!db) { lua_pushnil(L); return 1; }
    va_t addr = static_cast<va_t>(luaL_checkinteger(L, 1));
    auto it = db->insns.find(addr);
    if (it == db->insns.end()) { lua_pushnil(L); return 1; }

    lua_newtable(L);
    lua_pushstring(L, it->second.mnemonic);
    lua_setfield(L, -2, "mnemonic");
    lua_pushstring(L, it->second.op_str);
    lua_setfield(L, -2, "op_str");
    lua_pushinteger(L, it->second.len);
    lua_setfield(L, -2, "len");
    lua_pushinteger(L, static_cast<lua_Integer>(it->second.addr));
    lua_setfield(L, -2, "addr");
    return 1;
}

int l_get_bytes(lua_State* L) {
    auto* img = get_img(L);
    if (!img) { lua_pushnil(L); return 1; }
    va_t addr = static_cast<va_t>(luaL_checkinteger(L, 1));
    int len = static_cast<int>(luaL_checkinteger(L, 2));
    if (len <= 0 || len > 4096) { lua_pushnil(L); return 1; }

    for (auto& seg : img->segments) {
        if (!seg.contains(addr)) continue;
        size_t off = static_cast<size_t>(addr - seg.va);
        size_t avail = seg.data.size() - off;
        size_t n = static_cast<size_t>(len) < avail ? static_cast<size_t>(len) : avail;
        lua_pushlstring(L, reinterpret_cast<const char*>(seg.data.data() + off), n);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

int l_set_comment(lua_State* L) {
    auto* db = get_db(L);
    if (!db) return 0;
    va_t addr = static_cast<va_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    std::lock_guard lk(db->mtx);
    db->comments[addr] = text;
    return 0;
}

int l_get_xrefs_to(lua_State* L) {
    auto* db = get_db(L);
    if (!db) { lua_pushnil(L); return 1; }
    va_t addr = static_cast<va_t>(luaL_checkinteger(L, 1));
    auto it = db->xrefs_to.find(addr);
    if (it == db->xrefs_to.end()) { lua_newtable(L); return 1; }

    lua_newtable(L);
    int idx = 1;
    for (auto& xr : it->second) {
        lua_pushinteger(L, static_cast<lua_Integer>(xr.from));
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int l_get_functions(lua_State* L) {
    auto* db = get_db(L);
    if (!db) { lua_newtable(L); return 1; }
    lua_newtable(L);
    int idx = 1;
    for (auto& [entry, _] : db->funcs) {
        lua_pushinteger(L, static_cast<lua_Integer>(entry));
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int l_print(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "__hype_output");
    auto* out = static_cast<std::string*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!out) return 0;

    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        if (i > 1) *out += "\t";
        const char* s = luaL_tolstring(L, i, nullptr);
        if (s) *out += s;
        lua_pop(L, 1);
    }
    *out += "\n";
    return 0;
}

int l_goto(lua_State* L) {
    auto* eng = get_engine(L);
    if (!eng) return 0;
    va_t addr = static_cast<va_t>(luaL_checkinteger(L, 1));
    lua_getfield(L, LUA_REGISTRYINDEX, "__hype_nav");
    auto* cb = static_cast<std::function<void(va_t)>*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (cb && *cb) (*cb)(addr);
    return 0;
}

} // anon

LuaEngine::LuaEngine() {
    L_ = luaL_newstate();
    luaL_openlibs(L_);
}

LuaEngine::~LuaEngine() {
    if (L_) lua_close(L_);
}

void LuaEngine::init(AnalysisDB* db, PEImage* img) {
    db_ = db;
    img_ = img;
    register_api();
}

void LuaEngine::register_api() {
    lua_pushlightuserdata(L_, this);
    lua_setfield(L_, LUA_REGISTRYINDEX, "__hype_engine");
    lua_pushlightuserdata(L_, db_);
    lua_setfield(L_, LUA_REGISTRYINDEX, "__hype_db");
    lua_pushlightuserdata(L_, img_);
    lua_setfield(L_, LUA_REGISTRYINDEX, "__hype_img");
    lua_pushlightuserdata(L_, &output_);
    lua_setfield(L_, LUA_REGISTRYINDEX, "__hype_output");
    lua_pushlightuserdata(L_, &nav_cb_);
    lua_setfield(L_, LUA_REGISTRYINDEX, "__hype_nav");

    lua_register(L_, "get_name", l_get_name);
    lua_register(L_, "set_name", l_set_name);
    lua_register(L_, "get_func", l_get_func);
    lua_register(L_, "get_insn", l_get_insn);
    lua_register(L_, "get_bytes", l_get_bytes);
    lua_register(L_, "set_comment", l_set_comment);
    lua_register(L_, "get_xrefs_to", l_get_xrefs_to);
    lua_register(L_, "get_functions", l_get_functions);
    lua_register(L_, "print", l_print);
    lua_register(L_, "goto_addr", l_goto);
}

std::string LuaEngine::execute(const std::string& code) {
    output_.clear();
    int err = luaL_dostring(L_, code.c_str());
    if (err) {
        const char* msg = lua_tostring(L_, -1);
        std::string result = msg ? msg : "unknown error";
        lua_pop(L_, 1);
        return "[error] " + result;
    }
    return output_;
}

}
