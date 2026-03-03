#pragma once

#include <functional>
#include <string>
#include <vector>

struct lua_State;

class LuauEngine
{
public:
    LuauEngine();
    ~LuauEngine();

    LuauEngine(const LuauEngine& other) = delete;
    LuauEngine& operator=(const LuauEngine& other) = delete;

    void Load(const std::string& script, const std::string& chunkName);

    bool HasFunction(const char* name);
    void Call(const char* name, int args = 0, int returns = 0);

    lua_State* GetState();

private:
    lua_State* mState;
};
