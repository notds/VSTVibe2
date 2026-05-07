#pragma once
#include <vector>

// Render word to float PCM using macOS AVSpeechSynthesizer.
// bcp47: BCP-47 language tag (e.g. "en-US", "fr-FR") for voice matching.
// Alternates gender each call. Falls back gracefully if no matching voice.
std::vector<float> renderTTSWordMac(const wchar_t* word, const char* bcp47, float sampleRate);
