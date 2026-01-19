# Model Files

This directory should contain the LLM model file for text-to-parameter generation.

## Recommended Model (tiny, local)

Default recommendation: **Phi-4-mini-instruct (3.8B)**, using a community GGUF build.

Example (Q4_K_M quantization, good quality/size balance):

```bash
curl -L -o models/Phi-4-mini-instruct.Q4_K_M.gguf \
  https://huggingface.co/unsloth/Phi-4-mini-instruct-GGUF/resolve/main/Phi-4-mini-instruct-Q4_K_M.gguf
```

Model size: ~300MB–2GB depending on quantization.
License: Phi-4-mini-instruct is MIT licensed; include license attribution when distributing.

## Model discovery

The plugin looks for a GGUF model in this order:
1) `INCANT_LLM_MODEL` (full path to a `.gguf` file)
2) `INCANT_LLM_MODEL_DIR` (first `.gguf` in a directory)
3) `./models/Phi-4-mini-instruct.Q4_K_M.gguf` (if present)
4) `./models/*phi-4-mini*.gguf` (first match, case-insensitive)
5) `./models/*.gguf` (first match)

Set the environment variable before launching your DAW or plugin host.

## Alternative Models

Any GGUF-format model compatible with llama.cpp will work. Smaller models run faster but may produce less accurate parameters.

## Fallback Mode

If no model is found, the plugin will use keyword-based parameter generation (still functional, just not "AI").
