#include "LLMEngine.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <vector>

#ifdef USE_LLAMA_CPP
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

#ifdef USE_LLAMA_CPP
#include "llama.h"
#endif

namespace incant {

namespace {
#ifdef USE_LLAMA_CPP
constexpr int kDefaultMaxTokens = 192;
constexpr int kDefaultContextSize = 2048;
constexpr int kDefaultBatchSize = 512;
constexpr int kDefaultGpuLayers = 0;

const char* const kModelEnvVar = "INCANT_LLM_MODEL";
const char* const kModelDirEnvVar = "INCANT_LLM_MODEL_DIR";
const char* const kDefaultModelFilename = "Phi-4-mini-instruct.Q4_K_M.gguf";

std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value;
}

bool isPhiMiniModel(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    auto lower = toLowerCopy(path);
    return lower.find("phi-4-mini") != std::string::npos;
}

std::filesystem::path getModulePath() {
#if defined(_WIN32)
    HMODULE moduleHandle = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&getModulePath), &moduleHandle)) {
        return {};
    }
    char path[MAX_PATH] = {};
    DWORD size = GetModuleFileNameA(moduleHandle, path, MAX_PATH);
    if (size == 0) {
        return {};
    }
    return std::filesystem::path(std::string(path, size));
#else
    Dl_info info = {};
    if (dladdr(reinterpret_cast<void*>(&getModulePath), &info) == 0 || !info.dli_fname) {
        return {};
    }
    return std::filesystem::path(info.dli_fname);
#endif
}

std::filesystem::path getModuleDirectory() {
    auto modulePath = getModulePath();
    if (modulePath.empty()) {
        return {};
    }
    return modulePath.parent_path();
}

std::string formatPrompt(const char* templateStr, const std::string& description) {
    if (!templateStr) {
        return description;
    }
    int size = std::snprintf(nullptr, 0, templateStr, description.c_str());
    if (size <= 0) {
        return description;
    }
    std::string result(static_cast<size_t>(size), '\0');
    std::snprintf(result.data(), result.size() + 1, templateStr, description.c_str());
    return result;
}

std::string wrapPhiChatPrompt(const std::string& userPrompt) {
    static const std::string kSystem = "You are a helpful assistant.";
    std::string result;
    result.reserve(userPrompt.size() + kSystem.size() + 64);
    result.append("<|system|>");
    result.append(kSystem);
    result.append("<|end|><|user|>");
    result.append(userPrompt);
    result.append("<|end|><|assistant|>");
    return result;
}

std::string extractJsonObject(const std::string& text) {
    auto start = text.find('{');
    auto end = text.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        return {};
    }
    return text.substr(start, end - start + 1);
}

bool extractFloat(const std::string& json, const std::string& key, float& value) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return false;
    }
    try {
        value = std::stof(match[1].str());
    } catch (...) {
        return false;
    }
    value = std::clamp(value, 0.0f, 1.0f);
    return true;
}

std::string findGgufInDirectory(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return {};
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".gguf") {
            return entry.path().string();
        }
    }
    return {};
}

std::string findPreferredGgufInDirectory(const std::filesystem::path& dir,
                                         const std::string& preferredSubstring) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return {};
    }

    auto preferred = toLowerCopy(preferredSubstring);
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".gguf") {
            continue;
        }
        auto name = toLowerCopy(entry.path().filename().string());
        if (name.find(preferred) != std::string::npos) {
            return entry.path().string();
        }
    }
    return {};
}

std::string resolveModelPath(const std::string& preferredPath) {
    std::error_code ec;
    if (!preferredPath.empty()) {
        if (std::filesystem::exists(preferredPath, ec)) {
            return preferredPath;
        }
    }

    if (const char* envPath = std::getenv(kModelEnvVar)) {
        if (*envPath != '\0' && std::filesystem::exists(envPath, ec)) {
            return envPath;
        }
    }

    if (const char* envDir = std::getenv(kModelDirEnvVar)) {
        if (*envDir != '\0') {
            auto fromDir = findPreferredGgufInDirectory(std::filesystem::path(envDir), "phi-4-mini");
            if (fromDir.empty()) {
                fromDir = findGgufInDirectory(std::filesystem::path(envDir));
            }
            if (!fromDir.empty()) {
                return fromDir;
            }
        }
    }

    auto moduleDir = getModuleDirectory();
    if (!moduleDir.empty()) {
        auto resourcesDir = moduleDir.parent_path() / "Resources";
        auto bundledDefault = resourcesDir / "models" / kDefaultModelFilename;
        if (std::filesystem::exists(bundledDefault, ec)) {
            return bundledDefault.string();
        }

        auto bundledPreferred = findPreferredGgufInDirectory(resourcesDir / "models", "phi-4-mini");
        if (!bundledPreferred.empty()) {
            return bundledPreferred;
        }

        auto bundledAny = findGgufInDirectory(resourcesDir / "models");
        if (!bundledAny.empty()) {
            return bundledAny;
        }

        auto siblingDefault = moduleDir / "models" / kDefaultModelFilename;
        if (std::filesystem::exists(siblingDefault, ec)) {
            return siblingDefault.string();
        }

        auto siblingPreferred = findPreferredGgufInDirectory(moduleDir / "models", "phi-4-mini");
        if (!siblingPreferred.empty()) {
            return siblingPreferred;
        }

        auto siblingAny = findGgufInDirectory(moduleDir / "models");
        if (!siblingAny.empty()) {
            return siblingAny;
        }
    }

    auto modelsDir = std::filesystem::current_path() / "models";
    auto fromDefault = modelsDir / kDefaultModelFilename;
    if (std::filesystem::exists(fromDefault, ec)) {
        return fromDefault.string();
    }

    auto fromModelsPreferred = findPreferredGgufInDirectory(modelsDir, "phi-4-mini");
    if (!fromModelsPreferred.empty()) {
        return fromModelsPreferred;
    }

    auto fromModels = findGgufInDirectory(modelsDir);
    if (!fromModels.empty()) {
        return fromModels;
    }

    return {};
}
#endif
} // namespace

LLMEngine::LLMEngine() {
#ifdef USE_LLAMA_CPP
    llama_backend_init();
    ggml_backend_load_all();
#endif
}

LLMEngine::~LLMEngine() {
    cancelGeneration();
    unloadModel();
#ifdef USE_LLAMA_CPP
    llama_backend_free();
#endif
}

bool LLMEngine::loadModel(const std::string& modelPath) {
#ifdef USE_LLAMA_CPP
    std::lock_guard<std::mutex> lock(modelMutex_);

    if (model_) {
        status_ = Status::Ready;
        return true;
    }

    auto resolvedPath = resolveModelPath(modelPath);
    if (resolvedPath.empty()) {
        lastError_ = "No GGUF model found (set INCANT_LLM_MODEL or INCANT_LLM_MODEL_DIR).";
        status_ = Status::Unloaded;
        return false;
    }

    status_ = Status::Loading;

    llama_model_params modelParams = llama_model_default_params();
    modelParams.n_gpu_layers = kDefaultGpuLayers;

    model_ = llama_model_load_from_file(resolvedPath.c_str(), modelParams);
    if (!model_) {
        lastError_ = "Failed to load GGUF model: " + resolvedPath;
        status_ = Status::Error;
        return false;
    }

    vocab_ = llama_model_get_vocab(model_);
    modelPath_ = resolvedPath;
    status_ = Status::Ready;
    return true;
#else
    status_ = Status::Ready;
    return true;
#endif
}

void LLMEngine::unloadModel() {
#ifdef USE_LLAMA_CPP
    std::lock_guard<std::mutex> lock(modelMutex_);

    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    vocab_ = nullptr;
#endif
    status_ = Status::Unloaded;
}

void LLMEngine::generateParameters(EffectType effectType,
                                   const std::string& description,
                                   ResultCallback callback) {
    cancelGeneration();

    status_ = Status::Processing;
    cancelRequested_ = false;

    inferenceThread_ = std::thread([this, effectType, description, callback]() {
        bool usedLLM = false;
        ParameterResult result = parseKeywords(effectType, description);

#ifdef USE_LLAMA_CPP
        if (!model_) {
            loadModel(modelPath_);
        }

        llama_model* model = nullptr;
        const llama_vocab* vocab = nullptr;
        std::string modelPath;
        {
            std::lock_guard<std::mutex> lock(modelMutex_);
            model = model_;
            vocab = vocab_;
            modelPath = modelPath_;
        }

        if (model && vocab) {
            std::string userPrompt = formatPrompt(getPromptTemplate(effectType), description);
            std::string prompt = userPrompt;
            if (isPhiMiniModel(modelPath)) {
                prompt = wrapPhiChatPrompt(userPrompt);
            }

            const int n_prompt = -llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                nullptr, 0, true, true);
            if (n_prompt > 0) {
                std::vector<llama_token> promptTokens(static_cast<size_t>(n_prompt));
                if (llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                        promptTokens.data(), promptTokens.size(), true, true) >= 0) {

                    llama_context_params ctxParams = llama_context_default_params();
                    ctxParams.n_ctx = std::max(kDefaultContextSize,
                        n_prompt + kDefaultMaxTokens + 8);
                    ctxParams.n_batch = std::min(kDefaultBatchSize, n_prompt);

                    llama_context* ctx = llama_init_from_model(model, ctxParams);
                    if (ctx) {
                        auto sparams = llama_sampler_chain_default_params();
                        llama_sampler* sampler = llama_sampler_chain_init(sparams);

                        llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
                        llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
                        llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.2f));
                        llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

                        llama_batch batch = llama_batch_get_one(promptTokens.data(), promptTokens.size());

                        bool decodeOk = true;
                        if (llama_model_has_encoder(model)) {
                            decodeOk = (llama_encode(ctx, batch) == 0);
                            if (decodeOk) {
                                llama_token decoderStart = llama_model_decoder_start_token(model);
                                if (decoderStart == LLAMA_TOKEN_NULL) {
                                    decoderStart = llama_vocab_bos(vocab);
                                }
                                batch = llama_batch_get_one(&decoderStart, 1);
                            }
                        }

                        std::string output;
                        if (decodeOk) {
                            for (int i = 0; i < kDefaultMaxTokens; ++i) {
                                if (llama_decode(ctx, batch) != 0) {
                                    break;
                                }
                                llama_token token = llama_sampler_sample(sampler, ctx, -1);
                                if (llama_vocab_is_eog(vocab, token)) {
                                    break;
                                }

                                char buf[256];
                                int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, true);
                                if (n > 0) {
                                    output.append(buf, static_cast<size_t>(n));
                                }

                                if (cancelRequested_) {
                                    break;
                                }

                                batch = llama_batch_get_one(&token, 1);
                            }
                        }

                        llama_sampler_free(sampler);
                        llama_free(ctx);

                        std::string json = extractJsonObject(output);
                        if (!json.empty()) {
                            usedLLM = true;
                            switch (effectType) {
                                case EffectType::EQ: {
                                    EQParams params = std::get<EQParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "lowGain", params.lowGain);
                                    any |= extractFloat(json, "midGain", params.midGain);
                                    any |= extractFloat(json, "highGain", params.highGain);
                                    any |= extractFloat(json, "airGain", params.airGain);
                                    any |= extractFloat(json, "dryWet", params.dryWet);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Compressor: {
                                    CompressorParams params = std::get<CompressorParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "threshold", params.threshold);
                                    any |= extractFloat(json, "ratio", params.ratio);
                                    any |= extractFloat(json, "attack", params.attack);
                                    any |= extractFloat(json, "release", params.release);
                                    any |= extractFloat(json, "makeup", params.makeup);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Reverb: {
                                    ReverbParams params = std::get<ReverbParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "size", params.size);
                                    any |= extractFloat(json, "decay", params.decay);
                                    any |= extractFloat(json, "damping", params.damping);
                                    any |= extractFloat(json, "predelay", params.predelay);
                                    any |= extractFloat(json, "dryWet", params.dryWet);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Distortion: {
                                    DistortionParams params = std::get<DistortionParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "drive", params.drive);
                                    any |= extractFloat(json, "tone", params.tone);
                                    any |= extractFloat(json, "dryWet", params.dryWet);
                                    any |= extractFloat(json, "curveType", params.curveType);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Delay: {
                                    DelayParams params = std::get<DelayParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "time", params.time);
                                    any |= extractFloat(json, "feedback", params.feedback);
                                    any |= extractFloat(json, "filter", params.filter);
                                    any |= extractFloat(json, "pingPong", params.pingPong);
                                    any |= extractFloat(json, "dryWet", params.dryWet);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Glitch: {
                                    GlitchParams params = std::get<GlitchParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "rate", params.rate);
                                    any |= extractFloat(json, "stutter", params.stutter);
                                    any |= extractFloat(json, "crush", params.crush);
                                    any |= extractFloat(json, "reverse", params.reverse);
                                    any |= extractFloat(json, "dryWet", params.dryWet);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Overdrive: {
                                    OverdriveParams params = std::get<OverdriveParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "drive", params.drive);
                                    any |= extractFloat(json, "tone", params.tone);
                                    any |= extractFloat(json, "level", params.level);
                                    any |= extractFloat(json, "midBoost", params.midBoost);
                                    any |= extractFloat(json, "tightness", params.tightness);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Chorus: {
                                    ChorusParams params = std::get<ChorusParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "rate", params.rate);
                                    any |= extractFloat(json, "depth", params.depth);
                                    any |= extractFloat(json, "delay", params.delay);
                                    any |= extractFloat(json, "feedback", params.feedback);
                                    any |= extractFloat(json, "dryWet", params.dryWet);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Phaser: {
                                    PhaserParams params = std::get<PhaserParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "rate", params.rate);
                                    any |= extractFloat(json, "depth", params.depth);
                                    any |= extractFloat(json, "feedback", params.feedback);
                                    any |= extractFloat(json, "stages", params.stages);
                                    any |= extractFloat(json, "dryWet", params.dryWet);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Tremolo: {
                                    TremoloParams params = std::get<TremoloParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "rate", params.rate);
                                    any |= extractFloat(json, "depth", params.depth);
                                    any |= extractFloat(json, "shape", params.shape);
                                    any |= extractFloat(json, "stereo", params.stereo);
                                    any |= extractFloat(json, "dryWet", params.dryWet);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                                case EffectType::Filter: {
                                    FilterParams params = std::get<FilterParams>(getDefaultParams(effectType));
                                    bool any = false;
                                    any |= extractFloat(json, "cutoff", params.cutoff);
                                    any |= extractFloat(json, "resonance", params.resonance);
                                    any |= extractFloat(json, "lfoRate", params.lfoRate);
                                    any |= extractFloat(json, "lfoDepth", params.lfoDepth);
                                    any |= extractFloat(json, "filterType", params.filterType);
                                    if (any) {
                                        result = params;
                                    } else {
                                        usedLLM = false;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
#endif

        status_ = Status::Ready;
        callback(usedLLM, result);
    });
}

void LLMEngine::cancelGeneration() {
    cancelRequested_ = true;

    if (inferenceThread_.joinable()) {
        inferenceThread_.join();
    }
}

std::string LLMEngine::getLastError() const {
    return lastError_;
}

ParameterResult LLMEngine::getDefaultParams(EffectType type) {
    switch (type) {
        case EffectType::EQ: return EQParams{};
        case EffectType::Compressor: return CompressorParams{};
        case EffectType::Reverb: return ReverbParams{};
        case EffectType::Distortion: return DistortionParams{};
        case EffectType::Delay: return DelayParams{};
        case EffectType::Glitch: return GlitchParams{};
        case EffectType::Overdrive: return OverdriveParams{};
        case EffectType::Chorus: return ChorusParams{};
        case EffectType::Phaser: return PhaserParams{};
        case EffectType::Tremolo: return TremoloParams{};
        case EffectType::Filter: return FilterParams{};
    }
    return EQParams{};
}

ParameterResult LLMEngine::parseKeywords(EffectType type, const std::string& description) {
    // Intelligent keyword-based parameter generation
    // This is the "AI" - pattern matching against descriptive terms
    std::string lower = description;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto contains = [&lower](const std::string& word) {
        return lower.find(word) != std::string::npos;
    };

    auto containsAny = [&lower](const std::initializer_list<std::string>& words) {
        for (const auto& word : words) {
            if (lower.find(word) != std::string::npos) return true;
        }
        return false;
    };

    switch (type) {
        case EffectType::EQ: {
            EQParams params;

            // Low frequencies
            if (containsAny({"bass", "low", "sub", "boom", "thump", "weight", "bottom"})) {
                params.lowGain = 0.7f;
            }
            if (containsAny({"warm", "full", "thick", "fat"})) {
                params.lowGain = 0.65f;
                params.midGain = 0.55f;
            }
            if (containsAny({"thin", "tinny", "hollow", "weak"})) {
                params.lowGain = 0.35f;
            }

            // Mid frequencies
            if (containsAny({"mid", "presence", "vocal", "punch", "body"})) {
                params.midGain = 0.65f;
            }
            if (containsAny({"nasal", "honky", "boxy"})) {
                params.midGain = 0.7f;
            }
            if (containsAny({"scooped", "smile", "v-curve"})) {
                params.midGain = 0.3f;
                params.lowGain = 0.65f;
                params.highGain = 0.65f;
            }

            // High frequencies
            if (containsAny({"bright", "crisp", "sharp", "treble", "clear", "definition"})) {
                params.highGain = 0.7f;
            }
            if (containsAny({"dark", "muffled", "dull", "muted", "smooth"})) {
                params.highGain = 0.3f;
                params.airGain = 0.3f;
            }

            // Air/sparkle
            if (containsAny({"air", "shimmer", "sparkle", "airy", "open", "ethereal"})) {
                params.airGain = 0.75f;
            }
            if (containsAny({"digital", "hi-fi", "modern", "crystal"})) {
                params.highGain = 0.7f;
                params.airGain = 0.7f;
            }
            if (containsAny({"analog", "vintage", "retro", "lo-fi"})) {
                params.highGain = 0.4f;
                params.airGain = 0.35f;
                params.lowGain = 0.6f;
            }

            return params;
        }

        case EffectType::Compressor: {
            CompressorParams params;

            // Compression intensity
            if (containsAny({"heavy", "squash", "pumping", "aggressive", "smash", "crushed"})) {
                params.threshold = 0.2f;
                params.ratio = 0.8f;
                params.makeup = 0.6f;
            }
            if (containsAny({"gentle", "subtle", "light", "transparent", "natural"})) {
                params.threshold = 0.65f;
                params.ratio = 0.15f;
                params.makeup = 0.3f;
            }

            // Character
            if (containsAny({"punchy", "snappy", "tight", "controlled"})) {
                params.attack = 0.15f;
                params.release = 0.25f;
                params.ratio = 0.4f;
            }
            if (containsAny({"glue", "cohesive", "together", "bus", "mix"})) {
                params.attack = 0.5f;
                params.release = 0.6f;
                params.ratio = 0.2f;
                params.threshold = 0.55f;
            }
            if (containsAny({"limiting", "brick", "loud", "maximized"})) {
                params.ratio = 1.0f;
                params.threshold = 0.3f;
                params.attack = 0.05f;
            }
            if (containsAny({"slow", "breathing", "relaxed"})) {
                params.attack = 0.7f;
                params.release = 0.8f;
            }
            if (containsAny({"fast", "quick", "transient"})) {
                params.attack = 0.1f;
                params.release = 0.15f;
            }
            if (containsAny({"vocal", "voice", "dialogue"})) {
                params.threshold = 0.45f;
                params.ratio = 0.3f;
                params.attack = 0.3f;
                params.release = 0.4f;
            }
            if (containsAny({"drum", "percussion", "snare", "kick"})) {
                params.attack = 0.2f;
                params.release = 0.2f;
                params.ratio = 0.5f;
            }

            return params;
        }

        case EffectType::Reverb: {
            ReverbParams params;

            // Size/space
            if (containsAny({"room", "small", "tight", "intimate", "close"})) {
                params.size = 0.25f;
                params.decay = 0.2f;
            }
            if (containsAny({"hall", "large", "big", "spacious", "concert"})) {
                params.size = 0.7f;
                params.decay = 0.6f;
            }
            if (containsAny({"cathedral", "huge", "massive", "epic", "cinematic"})) {
                params.size = 0.9f;
                params.decay = 0.85f;
            }
            if (containsAny({"plate", "studio", "classic"})) {
                params.size = 0.5f;
                params.decay = 0.45f;
                params.damping = 0.35f;
            }
            if (containsAny({"chamber", "medium"})) {
                params.size = 0.45f;
                params.decay = 0.4f;
            }

            // Character
            if (containsAny({"bright", "shimmer", "sparkle", "airy"})) {
                params.damping = 0.2f;
            }
            if (containsAny({"dark", "warm", "mellow", "vintage"})) {
                params.damping = 0.7f;
            }
            if (containsAny({"infinite", "endless", "frozen", "pad"})) {
                params.decay = 0.95f;
            }

            // Mix
            if (containsAny({"wet", "drenched", "wash", "drowned", "swimming"})) {
                params.dryWet = 0.7f;
            }
            if (containsAny({"subtle", "ambient", "touch", "hint"})) {
                params.dryWet = 0.2f;
            }
            if (containsAny({"digital", "pristine", "clean"})) {
                params.damping = 0.25f;
                params.predelay = 0.2f;
            }
            if (containsAny({"spring"})) {
                params.size = 0.35f;
                params.decay = 0.3f;
                params.damping = 0.5f;
            }

            return params;
        }

        case EffectType::Distortion: {
            DistortionParams params;

            // Intensity
            if (containsAny({"light", "subtle", "touch", "warm", "gentle", "mild"})) {
                params.drive = 0.3f;
                params.curveType = 0.0f; // soft
            }
            if (containsAny({"crunch", "overdrive", "gritty", "edge"})) {
                params.drive = 0.5f;
                params.curveType = 0.5f; // tube
            }
            if (containsAny({"heavy", "hard", "aggressive", "metal", "brutal"})) {
                params.drive = 0.8f;
                params.curveType = 0.25f; // hard
            }
            if (containsAny({"fuzz", "destroyed", "chaos", "broken", "lo-fi"})) {
                params.drive = 0.9f;
                params.curveType = 1.0f; // fuzz
            }

            // Character
            if (containsAny({"saturate", "saturation", "tape", "analog", "warm"})) {
                params.drive = 0.35f;
                params.curveType = 0.0f; // soft
                params.tone = 0.45f;
            }
            if (containsAny({"tube", "valve", "amp", "preamp"})) {
                params.curveType = 0.5f; // tube
                params.drive = 0.45f;
            }
            if (containsAny({"digital", "harsh", "bit", "aliasing"})) {
                params.curveType = 0.25f; // hard
                params.tone = 0.6f;
            }

            // Tone
            if (containsAny({"bright", "presence", "cut", "sharp"})) {
                params.tone = 0.7f;
            }
            if (containsAny({"dark", "smooth", "mellow", "rounded"})) {
                params.tone = 0.3f;
            }

            // Mix
            if (containsAny({"parallel", "blend", "layer"})) {
                params.dryWet = 0.5f;
            }
            if (containsAny({"full", "committed", "100"})) {
                params.dryWet = 1.0f;
            }

            return params;
        }

        case EffectType::Delay: {
            DelayParams params;

            // Time/tempo
            if (containsAny({"short", "slapback", "slap", "quick", "tight", "doubling"})) {
                params.time = 0.1f;
                params.feedback = 0.2f;
            }
            if (containsAny({"medium", "eighth", "groove"})) {
                params.time = 0.35f;
                params.feedback = 0.4f;
            }
            if (containsAny({"long", "quarter", "spacious", "ambient"})) {
                params.time = 0.6f;
                params.feedback = 0.5f;
            }
            if (containsAny({"dotted", "triplet", "syncopated"})) {
                params.time = 0.45f;
                params.feedback = 0.55f;
            }

            // Feedback/repeats
            if (containsAny({"single", "one", "once", "echo"})) {
                params.feedback = 0.15f;
            }
            if (containsAny({"repeating", "multiple", "trail", "tail"})) {
                params.feedback = 0.55f;
            }
            if (containsAny({"infinite", "endless", "runaway", "self-oscillate"})) {
                params.feedback = 0.9f;
            }

            // Character
            if (containsAny({"dub", "reggae", "analog", "tape", "warm", "dark"})) {
                params.filter = 0.25f;
                params.feedback = 0.6f;
            }
            if (containsAny({"digital", "clean", "pristine", "bright", "hi-fi"})) {
                params.filter = 0.9f;
            }
            if (containsAny({"lo-fi", "degraded", "worn", "vintage"})) {
                params.filter = 0.35f;
            }

            // Stereo
            if (containsAny({"ping-pong", "pingpong", "stereo", "wide", "spread"})) {
                params.pingPong = 0.8f;
            }
            if (containsAny({"mono", "center", "focused"})) {
                params.pingPong = 0.0f;
            }

            // Mix
            if (containsAny({"subtle", "touch", "hint", "background"})) {
                params.dryWet = 0.25f;
            }
            if (containsAny({"prominent", "obvious", "wet", "drowned"})) {
                params.dryWet = 0.7f;
            }

            return params;
        }

        case EffectType::Glitch: {
            GlitchParams params;

            // Intensity/rate
            if (containsAny({"subtle", "occasional", "sparse", "light", "gentle"})) {
                params.rate = 0.2f;
                params.stutter = 0.3f;
            }
            if (containsAny({"moderate", "rhythmic", "groovy"})) {
                params.rate = 0.5f;
                params.stutter = 0.5f;
            }
            if (containsAny({"heavy", "intense", "chaos", "crazy", "extreme", "broken"})) {
                params.rate = 0.85f;
                params.stutter = 0.8f;
            }
            if (containsAny({"constant", "nonstop", "relentless"})) {
                params.rate = 0.95f;
            }

            // Stutter character
            if (containsAny({"stutter", "repeat", "retrigger", "buffer"})) {
                params.stutter = 0.7f;
            }
            if (containsAny({"long", "stretch", "granular"})) {
                params.stutter = 0.2f;
            }
            if (containsAny({"short", "tight", "rapid", "machine-gun"})) {
                params.stutter = 0.9f;
            }

            // Bit crush
            if (containsAny({"bitcrush", "8-bit", "retro", "nintendo", "chiptune"})) {
                params.crush = 0.6f;
            }
            if (containsAny({"lo-fi", "lofi", "degraded", "crushed"})) {
                params.crush = 0.45f;
            }
            if (containsAny({"destroyed", "annihilated", "demolished"})) {
                params.crush = 0.85f;
            }
            if (containsAny({"clean", "pristine", "no-crush"})) {
                params.crush = 0.0f;
            }

            // Reverse
            if (containsAny({"reverse", "backwards", "rewound"})) {
                params.reverse = 0.7f;
            }
            if (containsAny({"random", "unpredictable", "chaotic"})) {
                params.reverse = 0.5f;
                params.rate = 0.7f;
            }
            if (containsAny({"forward", "no-reverse"})) {
                params.reverse = 0.0f;
            }

            // Mix
            if (containsAny({"subtle", "blend", "parallel"})) {
                params.dryWet = 0.5f;
            }
            if (containsAny({"full", "committed", "wet", "100"})) {
                params.dryWet = 1.0f;
            }

            return params;
        }

        case EffectType::Overdrive: {
            OverdriveParams params;

            // Drive amount
            if (containsAny({"clean", "edge", "touch", "slight", "hint"})) {
                params.drive = 0.25f;
            }
            if (containsAny({"crunch", "medium", "moderate", "rhythm"})) {
                params.drive = 0.5f;
            }
            if (containsAny({"hot", "lead", "singing", "sustain"})) {
                params.drive = 0.7f;
            }
            if (containsAny({"heavy", "saturated", "thick", "screaming"})) {
                params.drive = 0.85f;
            }

            // Tone
            if (containsAny({"dark", "warm", "smooth", "mellow", "jazz"})) {
                params.tone = 0.3f;
            }
            if (containsAny({"bright", "cutting", "presence", "bite"})) {
                params.tone = 0.7f;
            }
            if (containsAny({"ice", "piercing", "shrill"})) {
                params.tone = 0.85f;
            }

            // TS-style characteristics
            if (containsAny({"tubescreamer", "ts", "ts9", "ts808", "ibanez", "screamer"})) {
                params.midBoost = 0.7f;
                params.tightness = 0.6f;
                params.drive = 0.5f;
            }
            if (containsAny({"mid", "honky", "midrange", "vocal", "nasal"})) {
                params.midBoost = 0.8f;
            }
            if (containsAny({"flat", "transparent", "neutral"})) {
                params.midBoost = 0.3f;
            }

            // Tightness
            if (containsAny({"tight", "focused", "palm", "metal", "djent", "chug"})) {
                params.tightness = 0.8f;
            }
            if (containsAny({"loose", "full", "bass", "fat", "thick"})) {
                params.tightness = 0.2f;
            }
            if (containsAny({"blues", "bluesy", "bb", "king"})) {
                params.drive = 0.4f;
                params.tone = 0.55f;
                params.midBoost = 0.5f;
                params.tightness = 0.3f;
            }

            return params;
        }

        case EffectType::Chorus: {
            ChorusParams params;

            // Rate
            if (containsAny({"slow", "gentle", "subtle", "lush"})) {
                params.rate = 0.2f;
            }
            if (containsAny({"fast", "vibrato", "leslie", "rotary"})) {
                params.rate = 0.7f;
            }

            // Depth
            if (containsAny({"subtle", "light", "touch", "mild"})) {
                params.depth = 0.3f;
            }
            if (containsAny({"deep", "heavy", "thick", "rich", "lush"})) {
                params.depth = 0.7f;
            }
            if (containsAny({"seasick", "extreme", "wobble", "warped"})) {
                params.depth = 0.9f;
            }

            // Character
            if (containsAny({"80s", "eighties", "juno", "synth", "pad"})) {
                params.rate = 0.35f;
                params.depth = 0.6f;
                params.dryWet = 0.5f;
            }
            if (containsAny({"12-string", "doubling", "thickening", "double"})) {
                params.delay = 0.5f;
                params.depth = 0.4f;
            }
            if (containsAny({"flanger", "jet", "metallic"})) {
                params.delay = 0.1f;
                params.feedback = 0.5f;
            }
            if (containsAny({"clean", "guitar", "classic", "roland", "boss", "ce"})) {
                params.rate = 0.4f;
                params.depth = 0.5f;
                params.dryWet = 0.4f;
            }

            return params;
        }

        case EffectType::Phaser: {
            PhaserParams params;

            // Rate
            if (containsAny({"slow", "sweep", "gentle", "ambient"})) {
                params.rate = 0.15f;
            }
            if (containsAny({"medium", "groove", "funk"})) {
                params.rate = 0.4f;
            }
            if (containsAny({"fast", "quick", "hyper", "vibrato"})) {
                params.rate = 0.75f;
            }

            // Depth
            if (containsAny({"subtle", "mild", "touch"})) {
                params.depth = 0.4f;
            }
            if (containsAny({"deep", "wide", "dramatic", "sweep"})) {
                params.depth = 0.85f;
            }

            // Feedback/resonance
            if (containsAny({"resonant", "squelch", "intense", "aggressive"})) {
                params.feedback = 0.8f;
            }
            if (containsAny({"smooth", "mild", "subtle"})) {
                params.feedback = 0.3f;
            }

            // Stages
            if (containsAny({"simple", "basic", "4-stage"})) {
                params.stages = 0.0f;
            }
            if (containsAny({"complex", "rich", "12-stage"})) {
                params.stages = 1.0f;
            }

            // Classic sounds
            if (containsAny({"evh", "van halen", "eruption", "brown"})) {
                params.rate = 0.35f;
                params.depth = 0.7f;
                params.feedback = 0.6f;
            }
            if (containsAny({"small stone", "phase90", "mxr", "classic"})) {
                params.stages = 0.25f;
                params.feedback = 0.5f;
            }

            return params;
        }

        case EffectType::Tremolo: {
            TremoloParams params;

            // Rate
            if (containsAny({"slow", "pulse", "throb", "ambient"})) {
                params.rate = 0.2f;
            }
            if (containsAny({"medium", "moderate"})) {
                params.rate = 0.45f;
            }
            if (containsAny({"fast", "helicopter", "stutter", "machine"})) {
                params.rate = 0.8f;
            }

            // Depth
            if (containsAny({"subtle", "gentle", "mild", "touch"})) {
                params.depth = 0.4f;
            }
            if (containsAny({"deep", "heavy", "dramatic", "choppy"})) {
                params.depth = 0.9f;
            }

            // Shape
            if (containsAny({"smooth", "sine", "soft", "gentle"})) {
                params.shape = 0.0f;
            }
            if (containsAny({"triangle", "vintage", "classic", "fender"})) {
                params.shape = 0.5f;
            }
            if (containsAny({"square", "choppy", "hard", "stutter", "gate"})) {
                params.shape = 1.0f;
            }

            // Stereo
            if (containsAny({"stereo", "pan", "auto-pan", "ping-pong", "wide"})) {
                params.stereo = 0.8f;
            }
            if (containsAny({"mono", "center", "classic"})) {
                params.stereo = 0.0f;
            }

            // Styles
            if (containsAny({"surf", "spring", "reverb-trem", "vintage"})) {
                params.rate = 0.5f;
                params.depth = 0.7f;
                params.shape = 0.5f;
            }
            if (containsAny({"optical", "amp", "brownface", "blackface"})) {
                params.shape = 0.3f;
                params.depth = 0.6f;
            }

            return params;
        }

        case EffectType::Filter: {
            FilterParams params;

            // Cutoff
            if (containsAny({"dark", "muffled", "closed", "low"})) {
                params.cutoff = 0.25f;
            }
            if (containsAny({"bright", "open", "high", "sharp"})) {
                params.cutoff = 0.75f;
            }

            // Resonance
            if (containsAny({"smooth", "warm", "subtle"})) {
                params.resonance = 0.2f;
            }
            if (containsAny({"resonant", "squelch", "acid", "303"})) {
                params.resonance = 0.7f;
            }
            if (containsAny({"screaming", "self-oscillate", "extreme"})) {
                params.resonance = 0.9f;
            }

            // LFO
            if (containsAny({"static", "fixed", "manual"})) {
                params.lfoDepth = 0.0f;
            }
            if (containsAny({"sweep", "auto", "wah", "envelope"})) {
                params.lfoDepth = 0.6f;
            }
            if (containsAny({"wobble", "dubstep", "bass"})) {
                params.lfoRate = 0.5f;
                params.lfoDepth = 0.8f;
                params.resonance = 0.5f;
            }

            // LFO rate
            if (containsAny({"slow", "ambient", "evolving"})) {
                params.lfoRate = 0.15f;
            }
            if (containsAny({"fast", "quick", "rhythmic"})) {
                params.lfoRate = 0.7f;
            }

            // Filter type
            if (containsAny({"lowpass", "lp", "moog", "warm", "fat"})) {
                params.filterType = 0.0f;
            }
            if (containsAny({"highpass", "hp", "thin", "telephone"})) {
                params.filterType = 0.33f;
            }
            if (containsAny({"bandpass", "bp", "vocal", "wah", "cocked"})) {
                params.filterType = 0.66f;
            }
            if (containsAny({"notch", "phaser", "hollow"})) {
                params.filterType = 1.0f;
            }

            // Classic sounds
            if (containsAny({"acid", "303", "tb303", "squelch"})) {
                params.cutoff = 0.4f;
                params.resonance = 0.75f;
                params.lfoDepth = 0.0f;
                params.filterType = 0.0f;
            }
            if (containsAny({"wah", "cry", "funky"})) {
                params.filterType = 0.66f;
                params.resonance = 0.5f;
                params.lfoDepth = 0.7f;
            }

            return params;
        }
    }

    return getDefaultParams(type);
}

} // namespace incant
