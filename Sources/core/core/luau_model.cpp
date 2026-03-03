#include "luau_model.h"

#include <cstring>

extern "C"
{
#include "../../ThirdParty/luau/VM/include/lua.h"
#include "../../ThirdParty/luau/VM/include/lauxlib.h"
}

#include "components.h"
#include "spike.h"

namespace LuauModel
{

namespace
{
const char* ReadScript(const json& params)
{
    if (params.find("script") == params.end() || !params["script"].is_string()) {
        return "";
    }

    return params["script"].get_ref<const std::string&>().c_str();
}
}

const char* LuauNeuron::Type = "LuauNeuron";
const char* LuauRegion::Type = "LuauRegion";
const char* LuauBrain::Type = "LuauBrain";

LuauNeuron::LuauNeuron(NeuronBase& base, json& params) :
    Neuron(base, params),
    mScript(ReadScript(params)),
    mEngine(),
    mLoaded(false)
{
    if (!mScript.empty()) {
        mEngine.reset(new LuauEngine());
        lua_State* state = mEngine->GetState();

        lua_pushlightuserdata(state, this);
        lua_pushcclosure(state, LuaLogInfo, 1);
        lua_setglobal(state, "log_info");

        lua_pushlightuserdata(state, this);
        lua_pushcclosure(state, LuaSendSpike, 1);
        lua_setglobal(state, "send_continuous_spike");

        mEngine->Load(mScript, "LuauNeuron");
        mLoaded = true;
    }
}

LuauNeuron::~LuauNeuron()
{
}

void LuauNeuron::pup(PUP::er& p)
{
    p | mScript;
    p | mLoaded;

    if (p.isUnpacking() && !mScript.empty()) {
        mEngine.reset(new LuauEngine());
        lua_State* state = mEngine->GetState();

        lua_pushlightuserdata(state, this);
        lua_pushcclosure(state, LuaLogInfo, 1);
        lua_setglobal(state, "log_info");

        lua_pushlightuserdata(state, this);
        lua_pushcclosure(state, LuaSendSpike, 1);
        lua_setglobal(state, "send_continuous_spike");

        mEngine->Load(mScript, "LuauNeuron");
    }
}

const char* LuauNeuron::GetType() const
{
    return Type;
}

void LuauNeuron::Control(size_t brainStep)
{
    if (!mLoaded || !mEngine->HasFunction("on_control")) {
        return;
    }

    lua_State* state = mEngine->GetState();
    lua_pushnumber(state, static_cast<double>(brainStep));
    mEngine->Call("on_control", 1, 0);
}

size_t LuauNeuron::ContributeToRegion(uint8_t*& contribution)
{
    contribution = nullptr;
    return 0;
}

int LuauNeuron::LuaLogInfo(lua_State* L)
{
    const char* message = luaL_checkstring(L, 1);
    Log(LogLevel::Info, "[LuauNeuron] %s", message);
    return 0;
}

int LuauNeuron::LuaSendSpike(lua_State* L)
{
    LuauNeuron* self = static_cast<LuauNeuron*>(lua_touserdata(L, lua_upvalueindex(1)));

    const NeuronId receiver = static_cast<NeuronId>(luaL_checknumber(L, 1));
    const int directionInt = static_cast<int>(luaL_checknumber(L, 2));
    const double intensity = luaL_checknumber(L, 3);

    Direction direction = directionInt == 0 ? Direction::Forward : Direction::Backward;

    Spike::Data data;
    Spike::Initialize(SpikeEditorCache::GetInstance()->GetToken("Continuous"), self->mBase.GetId(), data);
    auto* spike = static_cast<ContinuousSpike*>(Spike::Edit(data));
    spike->SetDelay(data, 0);
    spike->SetIntensity(data, intensity);

    self->mBase.SendSpike(receiver, direction, data);
    return 0;
}

LuauRegion::LuauRegion(RegionBase& base, json& params) :
    Region(base, params),
    mScript(ReadScript(params)),
    mEngine(),
    mLoaded(false)
{
    if (!mScript.empty()) {
        mEngine.reset(new LuauEngine());
        mEngine->Load(mScript, "LuauRegion");
        mLoaded = true;
    }
}

LuauRegion::~LuauRegion()
{
}

void LuauRegion::pup(PUP::er& p)
{
    p | mScript;
    p | mLoaded;

    if (p.isUnpacking() && !mScript.empty()) {
        mEngine.reset(new LuauEngine());
        mEngine->Load(mScript, "LuauRegion");
    }
}

const char* LuauRegion::GetType() const
{
    return Type;
}

void LuauRegion::Control(size_t brainStep)
{
    if (!mLoaded || !mEngine->HasFunction("on_control")) {
        return;
    }

    lua_State* state = mEngine->GetState();
    lua_pushnumber(state, static_cast<double>(brainStep));
    mEngine->Call("on_control", 1, 0);
}

void LuauRegion::AcceptContributionFromNeuron(NeuronId neuronId, const uint8_t* contribution, size_t size)
{
    (void)neuronId;
    (void)contribution;
    (void)size;
}

size_t LuauRegion::ContributeToBrain(uint8_t*& contribution)
{
    contribution = nullptr;
    return 0;
}

LuauBrain::LuauBrain(BrainBase& base, json& params) :
    Brain(base, params),
    mScript(ReadScript(params)),
    mEngine(),
    mLoaded(false)
{
    if (!mScript.empty()) {
        mEngine.reset(new LuauEngine());
        mEngine->Load(mScript, "LuauBrain");
        mLoaded = true;
    }
}

LuauBrain::~LuauBrain()
{
}

void LuauBrain::pup(PUP::er& p)
{
    p | mScript;
    p | mLoaded;

    if (p.isUnpacking() && !mScript.empty()) {
        mEngine.reset(new LuauEngine());
        mEngine->Load(mScript, "LuauBrain");
    }
}

const char* LuauBrain::GetType() const
{
    return Type;
}

void LuauBrain::Control(size_t brainStep)
{
    if (!mLoaded || !mEngine->HasFunction("on_control")) {
        return;
    }

    lua_State* state = mEngine->GetState();
    lua_pushnumber(state, static_cast<double>(brainStep));
    mEngine->Call("on_control", 1, 0);
}

void LuauBrain::AcceptContributionFromRegion(RegionIndex regIdx, const uint8_t* contribution, size_t size)
{
    (void)regIdx;
    (void)contribution;
    (void)size;
}

void Init(NeuronFactory* neuronFactory, RegionFactory* regionFactory, BrainFactory* brainFactory)
{
    neuronFactory->Register(LuauNeuron::Type, NeuronBuilder<LuauNeuron>);
    regionFactory->Register(LuauRegion::Type, RegionBuilder<LuauRegion>);
    brainFactory->Register(LuauBrain::Type, BrainBuilder<LuauBrain>);
}

}
