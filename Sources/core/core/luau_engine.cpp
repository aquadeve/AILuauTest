#include "luau_engine.h"

#include <cstdlib>
#include <stdexcept>

extern "C"
{
#include "../../ThirdParty/luau/VM/include/lua.h"
#include "../../ThirdParty/luau/VM/include/lauxlib.h"
#include "../../ThirdParty/luau/VM/include/lualib.h"
#include "../../ThirdParty/luau/Compiler/include/luacode.h"
}

LuauEngine::LuauEngine() : mState(luaL_newstate())
{
    if (!mState) {
        throw std::runtime_error("Failed to create Luau state");
    }

    luaL_openlibs(mState);
    luaL_sandbox(mState);
}

LuauEngine::~LuauEngine()
{
    if (mState) {
        lua_close(mState);
    }
}

void LuauEngine::Load(const std::string& script, const std::string& chunkName)
{
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(script.c_str(), script.size(), nullptr, &bytecodeSize);

    const int result = luau_load(mState, chunkName.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);

    if (result != 0) {
        const char* message = lua_tostring(mState, -1);
        std::string error = "Failed to load Luau script";
        if (message) {
            error += ": ";
            error += message;
        }

        lua_pop(mState, 1);
        throw std::runtime_error(error);
    }

    const int callResult = lua_pcall(mState, 0, 0, 0);
    if (callResult != 0) {
        const char* message = lua_tostring(mState, -1);
        std::string error = "Failed to execute Luau script";
        if (message) {
            error += ": ";
            error += message;
        }

        lua_pop(mState, 1);
        throw std::runtime_error(error);
    }
}

bool LuauEngine::HasFunction(const char* name)
{
    lua_getglobal(mState, name);
    const bool hasFunction = lua_isfunction(mState, -1);
    lua_pop(mState, 1);
    return hasFunction;
}

void LuauEngine::Call(const char* name, int args, int returns)
{
    lua_getglobal(mState, name);
    if (!lua_isfunction(mState, -1)) {
        lua_pop(mState, 1);
        throw std::runtime_error(std::string("Luau function not found: ") + name);
    }

    if (args > 0) {
        lua_insert(mState, -args - 1);
    }

    const int result = lua_pcall(mState, args, returns, 0);
    if (result != 0) {
        const char* message = lua_tostring(mState, -1);
        std::string error = "Failed to call Luau function";
        if (message) {
            error += ": ";
            error += message;
        }

        lua_pop(mState, 1);
        throw std::runtime_error(error);
    }
}

lua_State* LuauEngine::GetState()
{
    return mState;
}
