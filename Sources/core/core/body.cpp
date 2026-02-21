#include "charm.h"

#include "random.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <sstream>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif


#include "spike.h"
#include "body.h"
#include "log.h"
#include "mnist_body.h"

Body *Body::CreateBody(const std::string &type, json &params)
{
    if (type == RandomBody::Type) {
        return new RandomBody(params);
    } else if (type == MnistBody::Type) {
        return new MnistBody(params);
    } else if (type == PicoLmBody::Type) {
        return new PicoLmBody(params);
    } else {
        return nullptr;
    }
}

Body::Body(json &params)
{
    if (!params.empty()) {
        json sensors, actuators;
        for (auto itParams = params.begin(); itParams != params.end(); ++itParams) {
            if (itParams.key() == "sensors" && itParams->is_array()) {
                sensors = itParams.value();
            } else if (itParams.key() == "actuators" && itParams->is_array()) {
                actuators = itParams.value();
            }
        }

        for (auto itSensor = sensors.begin(); itSensor != sensors.end(); ++itSensor) {
            if (itSensor->is_object()) {
                const json &sensor = itSensor.value();

                std::string sensorName, spikeType;
                size_t elementSize = 0;
                size_t elementCount = 0;
                size_t spikeAllocCount = 0;

                sensorName = sensor["name"].get<std::string>();
                spikeType = sensor["spikeType"].get<std::string>();
                if (sensor.find("spikeAllocCount") != sensor.end()) {
                    spikeAllocCount = sensor["spikeAllocCount"].get<size_t>();
                }
                elementCount = sensor["size"].get<size_t>();

                Spike::Data dummySpike;
                Spike::Initialize(Spike::ParseType(spikeType), 0, dummySpike, spikeAllocCount);
                elementSize = Spike::Edit(dummySpike)->AllBytes(dummySpike);

                if (!sensorName.empty()) {
                    mSensorsInfo.insert(std::make_pair(sensorName,
                        ExpectedDataInfo(elementSize, elementCount)));
                }
            }
        }

        for (auto itActuator = actuators.begin(); itActuator != actuators.end(); ++itActuator) {
            if (itActuator->is_object()) {
                const json &actuator = itActuator.value();

                std::string actuatorName, spikeType;
                size_t elementSize = 0;
                size_t elementCount = 0;
                size_t spikeAllocCount = 0;

                actuatorName = actuator["name"].get<std::string>();
                spikeType = actuator["spikeType"].get<std::string>();
                if (actuator.find("spikeAllocCount") != actuator.end()) {
                    spikeAllocCount = actuator["spikeAllocCount"].get<size_t>();
                }
                elementCount = actuator["size"].get<size_t>();

                Spike::Data dummySpike;
                Spike::Initialize(Spike::ParseType(spikeType), 0, dummySpike, spikeAllocCount);
                elementSize = Spike::Edit(dummySpike)->AllBytes(dummySpike);

                if (!actuatorName.empty()) {
                    mActuatorsInfo.insert(std::make_pair(actuatorName,
                        ExpectedDataInfo(elementSize, elementCount)));
                }
            }
        }
    }
}

void Body::pup(PUP::er &p)
{
    p | mSensorsInfo;
    p | mActuatorsInfo;
}

const char *RandomBody::Type = "RandomBody";

RandomBody::RandomBody(json &params) : Body(params)
{
}

RandomBody::~RandomBody()
{
}

void RandomBody::pup(PUP::er &p)
{
    Body::pup(p);
}

const char *RandomBody::GetType()
{
    return Type;
}

void RandomBody::Simulate(
    size_t bodyStep,
    std::function<void(const std::string &, std::vector<uint8_t> &)> pushSensoMotoricData,
    std::function<void(const std::string &, std::vector<uint8_t> &)> pullSensoMotoricData)
{
    Random::Engine &engine = Random::GetThreadEngine();
    std::uniform_real_distribution<double> randDouble(0.0, 1.0);

    for (auto it = mActuatorsInfo.begin(); it != mActuatorsInfo.end(); ++it) {
        size_t elemSize = it->second.first;
        size_t elemCount = it->second.second;

        if (elemSize != sizeof(double)) continue;
        
        std::vector<uint8_t> data;
        pullSensoMotoricData(it->first, data);

        if (data.size() == elemSize * elemCount) {
            CkPrintf("%s: ", it->first.c_str());
            for (size_t i = 0; i < elemCount; ++i) {
                CkPrintf("%4.2f ", data.data() + i * elemSize);
            }
            CkPrintf("\n");
        }
    }

    // Here, world would react to motoric output, simulate next timestep and prepare sensoric input.

    for (auto it = mSensorsInfo.begin(); it != mSensorsInfo.end(); ++it) {
        size_t elemSize = it->second.first;
        size_t elemCount = it->second.second;

        if (elemSize != sizeof(double)) continue;
        
        std::vector<uint8_t> data;
        data.resize(elemSize * elemCount);
        
        for (size_t i = 0; i < elemCount; ++i) {
            uint8_t *dataPtr = data.data() + i * elemSize;
            double rd = randDouble(engine);
            memcpy(dataPtr, &rd, elemSize);
        }

        pushSensoMotoricData(it->first, data);
    }
}

const char *PicoLmBody::Type = "PicoLmBody";

namespace {

std::string ShellEscape(const std::string &text)
{
    std::string escaped;
    escaped.reserve(text.size() + 2);
    escaped.push_back('\'');

    for (char c : text) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(c);
        }
    }

    escaped.push_back('\'');
    return escaped;
}

}

PicoLmBody::PicoLmBody(json &params) : Body(params),
    mPromptActuatorName("Prompt"),
    mResponseSensorName("Response"),
    mPicoLmBinaryPath("./picolm"),
    mFallbackPrefix("PicoLM offline"),
    mMaxGeneratedTokens(96),
    mThreadCount(2),
    mContextSize(0),
    mTemperature(0.7),
    mTopP(0.9)
{
    if (!params.empty()) {
        if (params.count("promptActuator") != 0) {
            mPromptActuatorName = params["promptActuator"].get<std::string>();
        }

        if (params.count("responseSensor") != 0) {
            mResponseSensorName = params["responseSensor"].get<std::string>();
        }

        if (params.count("picolmBinary") != 0) {
            mPicoLmBinaryPath = params["picolmBinary"].get<std::string>();
        }

        if (params.count("picolmModel") != 0) {
            mPicoLmModelPath = params["picolmModel"].get<std::string>();
        }

        if (params.count("fallbackPrefix") != 0) {
            mFallbackPrefix = params["fallbackPrefix"].get<std::string>();
        }

        if (params.count("maxGeneratedTokens") != 0) {
            mMaxGeneratedTokens = params["maxGeneratedTokens"].get<size_t>();
        }

        if (params.count("threads") != 0) {
            mThreadCount = params["threads"].get<size_t>();
        }

        if (params.count("context") != 0) {
            mContextSize = params["context"].get<size_t>();
        }

        if (params.count("temperature") != 0) {
            mTemperature = params["temperature"].get<double>();
        }

        if (params.count("topP") != 0) {
            mTopP = params["topP"].get<double>();
        }
    }
}

PicoLmBody::~PicoLmBody()
{
}

void PicoLmBody::pup(PUP::er &p)
{
    Body::pup(p);

    p | mPromptActuatorName;
    p | mResponseSensorName;
    p | mPicoLmBinaryPath;
    p | mPicoLmModelPath;
    p | mFallbackPrefix;
    p | mMaxGeneratedTokens;
    p | mThreadCount;
    p | mContextSize;
    p | mTemperature;
    p | mTopP;
}

const char *PicoLmBody::GetType()
{
    return Type;
}

void PicoLmBody::Simulate(
    size_t bodyStep,
    std::function<void(const std::string &, std::vector<uint8_t> &)> pushSensoMotoricData,
    std::function<void(const std::string &, std::vector<uint8_t> &)> pullSensoMotoricData)
{
    (void)bodyStep;

    auto promptActuatorInfoIt = mActuatorsInfo.find(mPromptActuatorName);
    auto responseSensorInfoIt = mSensorsInfo.find(mResponseSensorName);
    if (promptActuatorInfoIt == mActuatorsInfo.end() || responseSensorInfoIt == mSensorsInfo.end()) {
        Log(LogLevel::Warn, "PicoLmBody terminal config invalid: actuator '%s', sensor '%s'",
            mPromptActuatorName.c_str(), mResponseSensorName.c_str());
        return;
    }

    std::vector<uint8_t> promptData;
    pullSensoMotoricData(mPromptActuatorName, promptData);
    if (promptData.empty()) {
        return;
    }

    std::string prompt = ReadText(promptData);
    if (prompt.empty()) {
        return;
    }

    std::string response = TryGenerateWithPicoLm(prompt);

    size_t responseSize = responseSensorInfoIt->second.first * responseSensorInfoIt->second.second;
    std::vector<uint8_t> responseData = EncodeText(response, responseSize);
    pushSensoMotoricData(mResponseSensorName, responseData);
}

std::string PicoLmBody::ReadText(const std::vector<uint8_t> &data) const
{
    std::string text(data.begin(), data.end());

    auto nullIt = std::find(text.begin(), text.end(), '\0');
    text.erase(nullIt, text.end());

    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.pop_back();
    }

    return text;
}

std::vector<uint8_t> PicoLmBody::EncodeText(const std::string &text, size_t size) const
{
    std::vector<uint8_t> data(size, 0);
    size_t copySize = (std::min)(size, text.size());
    std::copy(text.begin(), text.begin() + copySize, data.begin());
    return data;
}

std::string PicoLmBody::TryGenerateWithPicoLm(const std::string &prompt) const
{
    if (!mPicoLmModelPath.empty() && !mPicoLmBinaryPath.empty()) {
        std::ostringstream cmd;
        cmd << ShellEscape(mPicoLmBinaryPath) << " " << ShellEscape(mPicoLmModelPath)
            << " -p " << ShellEscape(prompt)
            << " -n " << mMaxGeneratedTokens
            << " -j " << mThreadCount
            << " -t " << mTemperature
            << " -k " << mTopP;

        if (mContextSize > 0) {
            cmd << " -c " << mContextSize;
        }

        FILE *pipe = POPEN(cmd.str().c_str(), "r");
        if (pipe != nullptr) {
            std::array<char, 1024> buffer;
            std::string output;
            while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
                output += buffer.data();
            }

            int rc = PCLOSE(pipe);
            if (rc == 0 && !output.empty()) {
                return output;
            }
        }
    }

    return mFallbackPrefix + ": " + prompt;
}
