# Text-to-VST

A VST3 audio plugin that generates DSP effect parameters from natural language descriptions using a local LLM.

## Features

- **4 Effect Types**: EQ, Compressor, Reverb, Distortion
- **Local AI Inference**: Uses llama.cpp for 100% offline, zero-latency parameter generation
- **Keyword Fallback**: Works even without a model file using intelligent keyword matching
- **Standard DSP**: Industry-standard algorithms (biquad filters, Freeverb, waveshaping)

## Build Requirements

- CMake 3.22+
- C++17 compiler (Clang/GCC/MSVC)
- macOS 10.13+ or Windows 10+

## Building

```bash
# Clone with submodules
git clone --recursive https://github.com/Vheissu/txt-vst.git
cd txt-vst

# Or if already cloned:
git submodule update --init --recursive

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Plugin will be in:
# macOS: build/Incant_artefacts/Release/VST3/Incant.vst3
# Windows: build/Incant_artefacts/Release/VST3/Incant.vst3
```

## Model Setup (Optional)

Download a GGUF model for AI-powered parameter generation (Phi-4-mini-instruct recommended):

```bash
curl -L -o models/Phi-4-mini-instruct.Q4_K_M.gguf \
  https://huggingface.co/unsloth/Phi-4-mini-instruct-GGUF/resolve/main/Phi-4-mini-instruct-Q4_K_M.gguf
```

Without a model, the plugin uses keyword-based parameter matching (still functional).

## Run Without a DAW (Standalone)

The build also produces a standalone app you can open directly:

```bash
open build/Incant_artefacts/Release/Standalone/Incant.app
```

## Usage

1. Load the plugin in your DAW
2. Select an effect type (EQ, Compressor, Reverb, Distortion)
3. Type a description: "warm analog saturation", "bright digital hall", "punchy snare compression"
4. Click Generate
5. Adjust parameters manually if needed

## Architecture

```
User Text Input -> LLM (llama.cpp) -> JSON Parameters -> DSP Engine -> Processed Audio
                      |
                      v (fallback)
               Keyword Matching
```

## Technical Notes

- DSP runs on audio thread, LLM inference on background thread
- Parameter changes are smoothed to avoid clicks
- Uses JUCE framework for cross-platform audio plugin development
- llama.cpp built as static library for self-contained distribution

## License

MIT
