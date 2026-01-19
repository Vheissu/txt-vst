#pragma once

#include "ParameterSchema.h"
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <variant>

// Uncomment to enable llama.cpp integration (requires stable API)
// #define USE_LLAMA_CPP

#ifdef USE_LLAMA_CPP
struct llama_model;
struct llama_vocab;
#endif

namespace incant {

using ParameterResult = std::variant<EQParams, CompressorParams, ReverbParams, DistortionParams,
    DelayParams, GlitchParams, OverdriveParams, ChorusParams, PhaserParams, TremoloParams, FilterParams>;

class LLMEngine {
public:
    enum class Status {
        Unloaded,
        Loading,
        Ready,
        Processing,
        Error
    };

    using ResultCallback = std::function<void(bool success, const ParameterResult& result)>;

    LLMEngine();
    ~LLMEngine();

    // Model loading (currently a no-op, uses keyword matching)
    bool loadModel(const std::string& modelPath);
    void unloadModel();

    // Async parameter generation
    void generateParameters(EffectType effectType,
                           const std::string& description,
                           ResultCallback callback);

    // Cancel pending generation
    void cancelGeneration();

    Status getStatus() const { return status_.load(); }
    std::string getLastError() const;

    // Keyword-based parameter generation (primary method)
    static ParameterResult getDefaultParams(EffectType type);
    static ParameterResult parseKeywords(EffectType type, const std::string& description);

private:
#ifdef USE_LLAMA_CPP
    llama_model* model_ = nullptr;
    const llama_vocab* vocab_ = nullptr;
#endif

    std::atomic<Status> status_{Status::Unloaded};
    std::atomic<bool> cancelRequested_{false};

    std::thread inferenceThread_;
    std::mutex modelMutex_;
    std::string lastError_;
    std::string modelPath_;
};

} // namespace incant
