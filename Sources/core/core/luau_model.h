#pragma once

#include <memory>
#include <string>

#include "brain.h"
#include "components.h"
#include "log.h"
#include "luau_engine.h"
#include "neuron.h"
#include "region.h"

namespace LuauModel
{

class LuauNeuron : public Neuron
{
public:
    static const char* Type;

    LuauNeuron(NeuronBase& base, json& params);
    virtual ~LuauNeuron();

    virtual void pup(PUP::er& p) override;
    virtual const char* GetType() const override;
    virtual void Control(size_t brainStep) override;
    virtual size_t ContributeToRegion(uint8_t*& contribution) override;

private:
    static int LuaLogInfo(lua_State* L);
    static int LuaSendSpike(lua_State* L);

    std::string mScript;
    std::unique_ptr<LuauEngine> mEngine;
    bool mLoaded;
};

class LuauRegion : public Region
{
public:
    static const char* Type;

    LuauRegion(RegionBase& base, json& params);
    virtual ~LuauRegion();

    virtual void pup(PUP::er& p) override;
    virtual const char* GetType() const override;
    virtual void Control(size_t brainStep) override;
    virtual void AcceptContributionFromNeuron(NeuronId neuronId, const uint8_t* contribution, size_t size) override;
    virtual size_t ContributeToBrain(uint8_t*& contribution) override;

private:
    std::string mScript;
    std::unique_ptr<LuauEngine> mEngine;
    bool mLoaded;
};

class LuauBrain : public Brain
{
public:
    static const char* Type;

    LuauBrain(BrainBase& base, json& params);
    virtual ~LuauBrain();

    virtual void pup(PUP::er& p) override;
    virtual const char* GetType() const override;
    virtual void Control(size_t brainStep) override;
    virtual void AcceptContributionFromRegion(RegionIndex regIdx, const uint8_t* contribution, size_t size) override;

private:
    std::string mScript;
    std::unique_ptr<LuauEngine> mEngine;
    bool mLoaded;
};

void Init(NeuronFactory* neuronFactory, RegionFactory* regionFactory, BrainFactory* brainFactory);

}
