#pragma once

#include <string>
#include <map>

namespace incant {

enum class EffectType {
    EQ,
    Compressor,
    Reverb,
    Distortion,
    Delay,
    Glitch,
    Overdrive,
    Chorus,
    Phaser,
    Tremolo,
    Filter
};

// Normalized parameters (0.0 to 1.0) for each effect type
struct EQParams {
    float lowGain = 0.5f;      // -12 to +12 dB, 0.5 = 0dB
    float midGain = 0.5f;
    float highGain = 0.5f;
    float airGain = 0.5f;
    float dryWet = 1.0f;
};

struct CompressorParams {
    float threshold = 0.5f;    // -60 to 0 dB
    float ratio = 0.25f;       // 1:1 to 20:1
    float attack = 0.1f;       // 0.1 to 100 ms
    float release = 0.3f;      // 10 to 1000 ms
    float makeup = 0.5f;       // 0 to 24 dB
};

struct ReverbParams {
    float size = 0.5f;         // Room size
    float decay = 0.5f;        // Decay time
    float damping = 0.5f;      // High frequency damping
    float predelay = 0.1f;     // Pre-delay time
    float dryWet = 0.3f;       // Mix
};

struct DistortionParams {
    float drive = 0.5f;        // Drive amount
    float tone = 0.5f;         // Post-filter tone
    float dryWet = 0.5f;       // Mix
    float curveType = 0.0f;    // 0=soft, 0.33=hard, 0.66=tube, 1=fuzz
};

struct DelayParams {
    float time = 0.3f;         // Delay time (0-1000ms, or tempo sync)
    float feedback = 0.4f;     // Feedback amount (0=none, 1=infinite)
    float filter = 0.7f;       // Feedback filter (0=dark, 1=bright)
    float pingPong = 0.0f;     // Stereo ping-pong amount
    float dryWet = 0.5f;       // Mix
};

struct GlitchParams {
    float rate = 0.5f;         // Glitch rate (how often glitches occur)
    float stutter = 0.5f;      // Stutter/repeat depth
    float crush = 0.0f;        // Bit crush amount (0=off, 1=extreme)
    float reverse = 0.3f;      // Probability of reverse chunks
    float dryWet = 0.7f;       // Mix
};

struct OverdriveParams {
    float drive = 0.5f;        // Drive/gain amount
    float tone = 0.5f;         // Tone control (dark to bright)
    float level = 0.5f;        // Output level
    float midBoost = 0.6f;     // Mid-frequency boost (TS character)
    float tightness = 0.5f;    // Low-end tightness/cut
};

struct ChorusParams {
    float rate = 0.4f;         // LFO rate (0.1 to 10 Hz)
    float depth = 0.5f;        // Modulation depth
    float delay = 0.3f;        // Base delay time
    float feedback = 0.0f;     // Feedback amount
    float dryWet = 0.5f;       // Mix
};

struct PhaserParams {
    float rate = 0.3f;         // LFO rate
    float depth = 0.7f;        // Sweep depth
    float feedback = 0.5f;     // Resonance/feedback
    float stages = 0.5f;       // Number of stages (4/6/8/12)
    float dryWet = 0.5f;       // Mix
};

struct TremoloParams {
    float rate = 0.5f;         // LFO rate (1 to 20 Hz)
    float depth = 0.7f;        // Modulation depth
    float shape = 0.0f;        // Waveform (0=sine, 0.5=triangle, 1=square)
    float stereo = 0.0f;       // Stereo phase offset
    float dryWet = 1.0f;       // Mix (usually 100%)
};

struct FilterParams {
    float cutoff = 0.5f;       // Filter cutoff frequency
    float resonance = 0.3f;    // Filter resonance/Q
    float lfoRate = 0.3f;      // LFO modulation rate
    float lfoDepth = 0.0f;     // LFO modulation depth
    float filterType = 0.0f;   // 0=lowpass, 0.33=highpass, 0.66=bandpass, 1=notch
};

// JSON keys for LLM communication
inline const char* getEffectTypeName(EffectType type) {
    switch (type) {
        case EffectType::EQ: return "eq";
        case EffectType::Compressor: return "compressor";
        case EffectType::Reverb: return "reverb";
        case EffectType::Distortion: return "distortion";
        case EffectType::Delay: return "delay";
        case EffectType::Glitch: return "glitch";
        case EffectType::Overdrive: return "overdrive";
        case EffectType::Chorus: return "chorus";
        case EffectType::Phaser: return "phaser";
        case EffectType::Tremolo: return "tremolo";
        case EffectType::Filter: return "filter";
    }
    return "unknown";
}

inline const char* getPromptTemplate(EffectType type) {
    switch (type) {
        case EffectType::EQ:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: EQ (4-band equalizer)
Description: "%s"

Output parameters as floats 0.0-1.0:
- lowGain: bass boost/cut (0.5=neutral)
- midGain: midrange (0.5=neutral)
- highGain: treble (0.5=neutral)
- airGain: upper harmonics (0.5=neutral)
- dryWet: effect amount (1.0=full)

JSON:)";

        case EffectType::Compressor:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Compressor
Description: "%s"

Output parameters as floats 0.0-1.0:
- threshold: compression threshold (0=heavy, 1=light)
- ratio: compression ratio (0=subtle, 1=limiting)
- attack: attack speed (0=fast, 1=slow)
- release: release speed (0=fast, 1=slow)
- makeup: makeup gain (0=none, 1=max)

JSON:)";

        case EffectType::Reverb:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Reverb
Description: "%s"

Output parameters as floats 0.0-1.0:
- size: room size (0=small, 1=huge)
- decay: decay time (0=short, 1=infinite)
- damping: high frequency absorption (0=bright, 1=dark)
- predelay: initial delay (0=none, 1=long)
- dryWet: wet/dry mix (0=dry, 1=wet)

JSON:)";

        case EffectType::Distortion:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Distortion
Description: "%s"

Output parameters as floats 0.0-1.0:
- drive: distortion amount (0=clean, 1=destroyed)
- tone: brightness (0=dark, 1=bright)
- dryWet: wet/dry mix (0=clean, 1=full distortion)
- curveType: distortion character (0=soft, 0.33=hard, 0.66=tube, 1=fuzz)

JSON:)";

        case EffectType::Delay:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Delay
Description: "%s"

Output parameters as floats 0.0-1.0:
- time: delay time (0=short ~10ms, 1=long ~1000ms)
- feedback: repeat amount (0=single echo, 1=infinite)
- filter: feedback brightness (0=dark/dub, 1=bright/clean)
- pingPong: stereo spread (0=mono, 1=full ping-pong)
- dryWet: wet/dry mix (0=dry, 1=wet)

JSON:)";

        case EffectType::Glitch:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Glitch
Description: "%s"

Output parameters as floats 0.0-1.0:
- rate: glitch frequency (0=sparse, 1=constant chaos)
- stutter: repeat/stutter intensity (0=subtle, 1=extreme)
- crush: bit crushing amount (0=clean, 1=lo-fi destruction)
- reverse: reverse probability (0=never, 1=always)
- dryWet: wet/dry mix (0=dry, 1=wet)

JSON:)";

        case EffectType::Overdrive:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Overdrive (Tubescreamer-style)
Description: "%s"

Output parameters as floats 0.0-1.0:
- drive: gain/saturation amount (0=clean, 1=heavy crunch)
- tone: brightness (0=dark, 1=bright)
- level: output volume (0=quiet, 1=loud)
- midBoost: mid-frequency emphasis (0=flat, 1=honky mids)
- tightness: low-end cut (0=loose/full, 1=tight/focused)

JSON:)";

        case EffectType::Chorus:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Chorus
Description: "%s"

Output parameters as floats 0.0-1.0:
- rate: modulation speed (0=slow, 1=fast)
- depth: modulation intensity (0=subtle, 1=seasick)
- delay: base delay time (0=short/flanger, 1=long/doubling)
- feedback: resonance (0=none, 1=metallic)
- dryWet: wet/dry mix (0=dry, 1=wet)

JSON:)";

        case EffectType::Phaser:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Phaser
Description: "%s"

Output parameters as floats 0.0-1.0:
- rate: sweep speed (0=slow, 1=fast)
- depth: sweep range (0=subtle, 1=extreme)
- feedback: resonance/intensity (0=mild, 1=intense)
- stages: complexity (0=4-stage, 0.5=8-stage, 1=12-stage)
- dryWet: wet/dry mix (0=dry, 1=wet)

JSON:)";

        case EffectType::Tremolo:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Tremolo
Description: "%s"

Output parameters as floats 0.0-1.0:
- rate: speed (0=slow pulse, 1=fast helicopter)
- depth: intensity (0=subtle, 1=full chop)
- shape: waveform (0=smooth sine, 0.5=triangle, 1=hard square)
- stereo: stereo spread (0=mono, 1=ping-pong)
- dryWet: effect amount (1.0=full effect)

JSON:)";

        case EffectType::Filter:
            return R"(You are an audio effect parameter generator. Output only valid JSON, no explanation.

Effect: Filter (Resonant)
Description: "%s"

Output parameters as floats 0.0-1.0:
- cutoff: filter frequency (0=low, 1=high)
- resonance: filter peak/Q (0=smooth, 1=squealy)
- lfoRate: modulation speed (0=slow, 1=fast)
- lfoDepth: modulation amount (0=static, 1=full sweep)
- filterType: type (0=lowpass, 0.33=highpass, 0.66=bandpass, 1=notch)

JSON:)";
    }
    return "";
}

} // namespace incant
