#include "pluginprocessor.h"
#include "mandelbrot_shaper.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <numeric>
#include <random>
#include <string>
#include <sstream>
#include <cstdio>
#ifdef _WIN32
#include <objbase.h>
#include <sapi.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace VSTVibe2 {

// Each entry carries the word, Windows LCID (for SAPI), and BCP-47 tag (for macOS AVFoundation).
struct ProfanityEntry { const wchar_t* word; const wchar_t* lcid; const char* bcp47; };

// TODO - change spelling to work around TTS mispronunciations

static const ProfanityEntry kProfanities[] = {
    // English (US)
    { L"fuck",             L"409", "en-US" }, { L"shit",            L"409", "en-US" },
    { L"bastard",          L"409", "en-US" }, { L"ass",             L"409", "en-US" },
    { L"bollocks",         L"409", "en-US" }, { L"cunt",            L"409", "en-US" },
    { L"ass clown",        L"409", "en-US" }, { L"knob jockey",     L"409", "en-US" },
    // Spanish (accents removed for TTS compatibility)
    { L"mierda",           L"C0A", "es-MX" }, { L"konyo",           L"C0A", "es-MX" },
    { L"joder",            L"C0A", "es-MX" }, { L"cabron",          L"C0A", "es-MX" },
    { L"hostia",           L"C0A", "es-MX" }, { L"puta madre",      L"C0A", "es-MX" },
    // French (accents removed)
    { L"merde",            L"40C", "fr-FR" }, { L"putain",          L"40C", "fr-FR" },
    { L"con",              L"40C", "fr-FR" }, { L"bordel",          L"40C", "fr-FR" },
    { L"sacre bleu",       L"40C", "fr-FR" },
    // German (umlauts/eszett removed)
    { L"Scheisse",         L"407", "de-DE" }, { L"verdammt",        L"407", "de-DE" },
    { L"Arschloch",        L"407", "de-DE" }, { L"Mist",            L"407", "de-DE" },
    { L"Teufel",           L"407", "de-DE" },
    // Italian
    { L"cazzo",            L"410", "it-IT" }, { L"stronzo",         L"410", "it-IT" },
    { L"vaffanculo",       L"410", "it-IT" }, { L"porco dio",       L"410", "it-IT" },
    // Portuguese (BR)
    { L"merda",            L"416", "pt-BR" }, { L"porra",           L"416", "pt-BR" },
    { L"caralho",          L"416", "pt-BR" }, { L"foda se",         L"416", "pt-BR" },
    // Dutch
    { L"godverdomme",      L"413", "nl-NL" }, { L"klootzak",        L"413", "nl-NL" },
    { L"kut",              L"413", "nl-NL" }, { L"tering",          L"413", "nl-NL" },
    // Swedish (umlauts removed)
    { L"fan",              L"41D", "sv-SE" }, { L"javlar",          L"41D", "sv-SE" },
    { L"helvete",          L"41D", "sv-SE" }, { L"skit",            L"41D", "sv-SE" },
    // Finnish
    { L"perkele",          L"40B", "fi-FI" }, { L"saatana",         L"40B", "fi-FI" },
    { L"vittu",            L"40B", "fi-FI" }, { L"jumalauta",       L"40B", "fi-FI" },
    // Polish
    { L"kurwa",            L"415", "pl-PL" }, { L"chuj",            L"415", "pl-PL" },
    { L"dupek",            L"415", "pl-PL" }, { L"psiakrew",        L"415", "pl-PL" },
    // Russian (romanized)
    { L"govno",            L"419", "ru-RU" }, { L"yebat",           L"419", "ru-RU" },
    { L"suka blat",        L"419", "ru-RU" }, { L"idi nah hoy",     L"419", "ru-RU" },
    // Japanese (romanized)
    { L"kuso",             L"411", "ja-JP" }, { L"chikusho",        L"411", "ja-JP" },
    { L"kichiku",          L"411", "ja-JP" }, { L"shimatta",        L"411", "ja-JP" },
    // Jamaican
    { L"bumbo clot",       L"358", "en-JM" }, { L"raas clot",       L"358", "en-JM" },
    { L"pussy clot",       L"358", "en-JM" }, { L"blood clot",      L"358", "en-JM" },
    { L"suck yuh mother",  L"358", "en-JM" }, { L"batty boy",       L"358", "en-JM" },
    // Chinese Simplified (romanized)
    { L"ta ma de",         L"804", "zh-CN" }, { L"wo cao",          L"804", "zh-CN" },
    { L"sha bi",           L"804", "zh-CN" },
    // Korean (romanized)
    { L"ssibal",           L"412", "ko-KR" }, { L"gaesaekki",       L"412", "ko-KR" },
    { L"byeonsaeki",       L"412", "ko-KR" },
    // Arabic (romanized)
    { L"ibn el sharmouta", L"401", "ar-SA" }, { L"yil an abu deen", L"401", "ar-SA" },
    { L"kuss",             L"401", "ar-SA" },
    // Greek (romanized)
    { L"malaka",           L"408", "el-GR" }, { L"poutana",         L"408", "el-GR" },
    { L"skatos",           L"408", "el-GR" },
    // Hindi (romanized)
    { L"bhenchod",         L"439", "hi-IN" }, { L"mardarchode",     L"439", "hi-IN" },
    { L"chutiya",          L"439", "hi-IN" },
    // Turkish
    { L"siktir",           L"41F", "tr-TR" }, { L"orospu",          L"41F", "tr-TR" },
    { L"kahpe",            L"41F", "tr-TR" },
    // Hungarian
    { L"kurva",            L"40E", "hu-HU" }, { L"bazdmeg",         L"40E", "hu-HU" },
    { L"faszom",           L"40E", "hu-HU" },
    // Czech
    { L"kurva",            L"405", "cs-CZ" }, { L"hovno",           L"405", "cs-CZ" },
    { L"pica",             L"405", "cs-CZ" },
    // Norwegian (ligatures removed)
    { L"faen",             L"414", "nb-NO" }, { L"jaevla",          L"414", "nb-NO" },
    { L"kukk",             L"414", "nb-NO" },
    // Romanian
    { L"pula",             L"418", "ro-RO" }, { L"futu i",          L"418", "ro-RO" },
    { L"muie",             L"418", "ro-RO" },
};
static constexpr int kProfanitiesCount = static_cast<int>(sizeof(kProfanities) / sizeof(kProfanities[0]));

// Nominal F0 of the TTS voice (A2). Playing A2 = natural speed; higher notes speed up.
static constexpr float kTTSBasePitch = 110.0f;

#ifdef _WIN32
// GUID for WaveFormatEx data format in SAPI streams (SPDFID_WaveFormatEx)
static const GUID kSpWaveFmtGuid =
    { 0xC31ADBAE, 0x527F, 0x4FF5, { 0xA2, 0x30, 0xF6, 0x2B, 0xB6, 0x1F, 0xF7, 0x0C } };

#ifndef SPCAT_VOICES
#define SPCAT_VOICES L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices"
#endif

// Selects a voice token matching (in priority order):
//   1. Correct language + preferred gender
//   2. Correct language + any gender
//   3. Any language + preferred gender
//   4. Any installed voice
// Updates s_lastGender so each successive call alternates gender.
static ISpObjectToken* pickVoice(IEnumSpObjectTokens* pEnum, const wchar_t* lcid,
                                 std::mt19937& rng) {
    static std::wstring s_lastGender = L"Female";
    const std::wstring  wantGender   = (s_lastGender == L"Male") ? L"Female" : L"Male";

    ULONG count = 0;
    pEnum->GetCount(&count);

    std::vector<ISpObjectToken*> langGender, langAny, anyGender, anyAny;
    const std::wstring langQuery   = std::wstring(L"Language=") + lcid;
    const std::wstring genderQuery = std::wstring(L"Gender=")   + wantGender;

    for (ULONG i = 0; i < count; ++i) {
        ISpObjectToken* pT = NULL;
        if (FAILED(pEnum->Item(i, &pT)) || !pT) continue;

        BOOL hasLang = FALSE, hasGender = FALSE;
        pT->MatchesAttributes(langQuery.c_str(),   &hasLang);
        pT->MatchesAttributes(genderQuery.c_str(), &hasGender);

        if (hasLang && hasGender) langGender.push_back(pT);
        else if (hasLang)         langAny.push_back(pT);
        else if (hasGender)       anyGender.push_back(pT);
        else                      anyAny.push_back(pT);
    }

    // Pick from best non-empty tier
    auto pick = [&](std::vector<ISpObjectToken*>& v) -> ISpObjectToken* {
        return v.empty() ? nullptr : v[rng() % v.size()];
    };
    ISpObjectToken* chosen = pick(langGender);
    if (!chosen) chosen = pick(langAny);
    if (!chosen) chosen = pick(anyGender);
    if (!chosen) chosen = pick(anyAny);

    if (chosen) {
        chosen->AddRef();
        BOOL isMale = FALSE;
        chosen->MatchesAttributes(L"Gender=Male", &isMale);
        s_lastGender = isMale ? L"Male" : L"Female";
    }

    // Release all collected tokens
    for (auto* t : langGender) t->Release();
    for (auto* t : langAny)    t->Release();
    for (auto* t : anyGender)  t->Release();
    for (auto* t : anyAny)     t->Release();

    return chosen;  // Caller must Release()
}

static std::vector<float> renderTTSWord(const ProfanityEntry& entry, float targetSampleRate) {
    std::vector<float> result;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    ISpVoice* pVoice = NULL;
    if (FAILED(CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice))) {
        CoUninitialize(); return result;
    }

    // Fresh nanosecond-seeded RNG for this word
    using clock = std::chrono::high_resolution_clock;
    const auto t = clock::now().time_since_epoch().count();
    std::mt19937 rng(static_cast<uint32_t>(t ^ (t >> 32) ^ (t >> 16)));

    // Enumerate voices and pick best match for language + alternating gender
    ISpObjectTokenCategory* pCat = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL,
                                   IID_ISpObjectTokenCategory, (void**)&pCat))) {
        if (SUCCEEDED(pCat->SetId(SPCAT_VOICES, FALSE))) {
            IEnumSpObjectTokens* pEnum = NULL;
            if (SUCCEEDED(pCat->EnumTokens(NULL, NULL, &pEnum))) {
                ISpObjectToken* pToken = pickVoice(pEnum, entry.lcid, rng);
                if (pToken) { pVoice->SetVoice(pToken); pToken->Release(); }
                pEnum->Release();
            }
        }
        pCat->Release();
    }

    IStream* pMemStream = NULL;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &pMemStream))) {
        pVoice->Release(); CoUninitialize(); return result;
    }

    ISpStream* pSpStream = NULL;
    if (FAILED(CoCreateInstance(CLSID_SpStream, NULL, CLSCTX_ALL, IID_ISpStream, (void**)&pSpStream))) {
        pMemStream->Release(); pVoice->Release(); CoUninitialize(); return result;
    }

    WAVEFORMATEX wfx    = {};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = 1;
    wfx.nSamplesPerSec  = 22050;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = 2;
    wfx.nAvgBytesPerSec = 44100;

    if (SUCCEEDED(pSpStream->SetBaseStream(pMemStream, kSpWaveFmtGuid, &wfx))) {
        pVoice->SetOutput(pSpStream, TRUE);
        pVoice->Speak(entry.word, SPF_DEFAULT, NULL);

        ULARGE_INTEGER size = {};
        LARGE_INTEGER  zero = {};
        pMemStream->Seek(zero, STREAM_SEEK_END, &size);
        pMemStream->Seek(zero, STREAM_SEEK_SET, NULL);

        const size_t numSamples = static_cast<size_t>(size.QuadPart / 2);
        std::vector<int16_t> pcm(numSamples);
        ULONG bytesRead = 0;
        pMemStream->Read(pcm.data(), static_cast<ULONG>(size.QuadPart), &bytesRead);

        const double ratio  = targetSampleRate / 22050.0;
        const size_t outLen = static_cast<size_t>(numSamples * ratio);
        result.resize(outLen);
        for (size_t i = 0; i < outLen; ++i) {
            const double srcPos = i / ratio;
            const size_t si     = static_cast<size_t>(srcPos);
            const float  frac   = static_cast<float>(srcPos - si);
            const float  s0     = si     < numSamples ? pcm[si]     / 32768.0f : 0.0f;
            const float  s1     = si + 1 < numSamples ? pcm[si + 1] / 32768.0f : 0.0f;
            result[i]           = s0 + frac * (s1 - s0);
        }
    }

    pSpStream->Release();
    pMemStream->Release();
    pVoice->Release();
    CoUninitialize();
    return result;
}

#else  // !_WIN32
#include <dlfcn.h>
#include "tts_mac.h"
static std::vector<float> renderTTSWord(const ProfanityEntry& entry, float sr) {
    return renderTTSWordMac(entry.word, entry.bcp47, sr);
}
#endif  // _WIN32

// ---- Custom word list loaded from MONODUCKWORDS.TXT ----
static std::vector<std::wstring>   s_customWordStorage;
static std::vector<ProfanityEntry> s_customProfanities;
static bool                        s_useCustomWords = false;
static std::once_flag              s_wordFileOnce;

static void loadCustomWordFile() {
    // Locate the directory that contains this plugin binary
    std::wstring dir;
#ifdef _WIN32
    HMODULE hMod = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(static_cast<void*>(&loadCustomWordFile)),
        &hMod);
    if (hMod) {
        wchar_t buf[MAX_PATH] = {};
        GetModuleFileNameW(hMod, buf, MAX_PATH);
        std::wstring full(buf);
        const auto sep = full.rfind(L'\\');
        dir = (sep != std::wstring::npos) ? full.substr(0, sep + 1) : L".\\";
    }
#else
    Dl_info dlInfo{};
    if (dladdr(reinterpret_cast<void*>(&loadCustomWordFile), &dlInfo) && dlInfo.dli_fname) {
        std::string p(dlInfo.dli_fname);
        const auto sep = p.rfind('/');
        std::string narrow = (sep != std::string::npos) ? p.substr(0, sep + 1) : "./";
        dir.assign(narrow.begin(), narrow.end());
    }
#endif
    if (dir.empty()) return;

    const std::wstring filePath       = dir + L"MONODUCKWORDS.TXT";
    const std::string  defaultContent = "Edit this file to customize word phrases. Put each on a new line";

    // Open existing file
#ifdef _WIN32
    FILE* f = nullptr;
    _wfopen_s(&f, filePath.c_str(), L"rb");
#else
    std::string narrowPath(filePath.begin(), filePath.end());
    FILE* f = fopen(narrowPath.c_str(), "rb");
#endif

    if (!f) {
        // Create default placeholder
#ifdef _WIN32
        FILE* fw = nullptr;
        _wfopen_s(&fw, filePath.c_str(), L"wb");
#else
        FILE* fw = fopen(narrowPath.c_str(), "wb");
#endif
        if (fw) {
            fwrite(defaultContent.c_str(), 1, defaultContent.size(), fw);
            fclose(fw);
        }
        return;
    }

    // Read full file
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content(static_cast<size_t>(sz > 0 ? sz : 0), '\0');
    if (sz > 0) fread(&content[0], 1, static_cast<size_t>(sz), f);
    fclose(f);

    // If it's still the unedited placeholder, use the built-in list
    if (content == defaultContent) return;

    // Parse lines into custom word storage
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        if (line.empty()) continue;
        s_customWordStorage.emplace_back(line.begin(), line.end());
    }
    if (s_customWordStorage.empty()) return;

    // Build ProfanityEntry pointers into the stable storage vector
    s_customProfanities.reserve(s_customWordStorage.size());
    for (const auto& w : s_customWordStorage)
        s_customProfanities.push_back({ w.c_str(), L"409", "en-US" });

    s_useCustomWords = true;
}

VSTVibe2Processor::~VSTVibe2Processor() {
    if (ttsThread.joinable()) ttsThread.join();
}

VSTVibe2Processor::VSTVibe2Processor() {
    setControllerClass(kVSTVibe2ControllerUID);
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::initialize(Steinberg::FUnknown* context) {
    Steinberg::tresult result = AudioEffect::initialize(context);
    if (result != Steinberg::kResultOk) {
        return result;
    }

    std::call_once(s_wordFileOnce, loadCustomWordFile);

    addEventInput(STR16("MIDI Input"), 1);
    addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::setState(Steinberg::IBStream* /*state*/) {
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::getState(Steinberg::IBStream* /*state*/) {
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::setupProcessing(Steinberg::Vst::ProcessSetup& setup) {
    sampleRate = setup.sampleRate;
    return AudioEffect::setupProcessing(setup);
}

float VSTVibe2Processor::generateSineWave(double phaseVal) {
    return static_cast<float>(std::sin(2.0 * M_PI * phaseVal));
}

float VSTVibe2Processor::generateSquareWave(double phaseVal) {
    double normalizedPhase = phaseVal - std::floor(phaseVal);
    return (normalizedPhase < 0.5) ? 1.0f : -1.0f;
}

float VSTVibe2Processor::generateTriangleWave(double phaseVal) {
    double normalizedPhase = phaseVal - std::floor(phaseVal);
    if (normalizedPhase < 0.25) {
        return -1.0f + 4.0f * normalizedPhase;
    } else if (normalizedPhase < 0.75) {
        return 1.0f - 4.0f * (normalizedPhase - 0.25f);
    } else {
        return -1.0f + 4.0f * (normalizedPhase - 0.75f);
    }
}

float VSTVibe2Processor::generateSawWave(double phaseVal) {
    double normalizedPhase = phaseVal - std::floor(phaseVal);
    return -1.0f + 2.0f * normalizedPhase;
}

// Feedforward peak compressor. atkCoeff/relCoeff are precomputed per block.
// ratio scales 1:1 → 20:1 with squeeze; threshold fixed at -18 dBFS.
// Auto makeup gain (+9 dB max) keeps perceived loudness stable.
static float applyCompressor(float sample, float squeeze,
                              float& env, float atkCoeff, float relCoeff) {
    if (squeeze < 1e-4f) return sample;

    const float level = std::abs(sample);
    env = (level > env)
        ? atkCoeff * env + (1.0f - atkCoeff) * level
        : relCoeff * env + (1.0f - relCoeff) * level;

    const float thresholdDb     = -18.0f;
    const float ratio           = 1.0f + squeeze * 19.0f;
    const float levelDb         = 20.0f * std::log10(env + 1e-7f);
    const float overDb          = std::max(0.0f, levelDb - thresholdDb);
    const float gainReductionDb = overDb * (1.0f - 1.0f / ratio);
    const float makeupDb        = squeeze * 9.0f;

    return sample * std::pow(10.0f, (-gainReductionDb + makeupDb) / 20.0f);
}

// Foldback distortion. Pre-gain drives signal above 1.0; iterative fold reflects back into [-1,1].
// Above 50%: adds subtle white noise (max ~4% of full scale), gated by noiseEnv so it
// tails off after note release rather than playing indefinitely.
static float applyDistortion(float sample, float amount, float noiseEnv) {
    if (amount < 1e-4f) return sample;

    float x = sample * (1.0f + amount * 7.0f);
    for (int i = 0; i < 8; ++i) {
        if      (x >  1.0f) x =  2.0f - x;
        else if (x < -1.0f) x = -2.0f - x;
        else break;
    }

    float out = sample * (1.0f - amount) + x * amount;

    if (amount > 0.5f && noiseEnv > 1e-4f) {
        static uint32_t ns = 2463534242u;
        ns ^= ns << 13; ns ^= ns >> 17; ns ^= ns << 5;
        const float noise    = static_cast<float>(ns) * (1.0f / 4294967296.0f) * 2.0f - 1.0f;
        const float noiseAmt = (amount - 0.5f) * 2.0f * 0.04f * noiseEnv;
        out += noise * noiseAmt;
    }

    return out;
}

// Strips leading and trailing silence.  Fade-in/out is handled at mix time
// (output-sample domain) so pitch-shifting never skips over the ramp.
static void trimSilence(std::vector<float>& buf, float threshold = 0.008f) {
    if (buf.empty()) return;
    size_t first = 0;
    while (first < buf.size() && std::abs(buf[first]) < threshold) ++first;
    size_t last = buf.size();
    while (last > first && std::abs(buf[last - 1]) < threshold) --last;
    if (first >= last) { buf.clear(); return; }
    buf.erase(buf.begin() + static_cast<std::ptrdiff_t>(last), buf.end());
    if (first > 0)
        buf.erase(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(first));
}

float VSTVibe2Processor::mixOscillators(double phaseVal) {
    float sine     = generateSineWave(phaseVal)     * oscillatorVolumes[0];
    float square   = generateSquareWave(phaseVal)   * oscillatorVolumes[1];
    float triangle = generateTriangleWave(phaseVal) * oscillatorVolumes[2];
    float saw      = generateSawWave(phaseVal)      * oscillatorVolumes[3];
    return (sine + square + triangle + saw) * 0.25f;
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::process(Steinberg::Vst::ProcessData& data) {
    if (data.inputParameterChanges) {
        Steinberg::int32 numParams = data.inputParameterChanges->getParameterCount();
        for (Steinberg::int32 i = 0; i < numParams; ++i) {
            Steinberg::Vst::IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue) continue;
            Steinberg::Vst::ParamID id = queue->getParameterId();
            Steinberg::int32 numPoints = queue->getPointCount();
            if (numPoints == 0) continue;
            Steinberg::int32 sampleOffset;
            Steinberg::Vst::ParamValue value;
            if (queue->getPoint(numPoints - 1, sampleOffset, value) != Steinberg::kResultOk) continue;

            if      (id == 0) { oscillatorVolumes[0] = static_cast<float>(value); }
            else if (id == 1) { oscillatorVolumes[1] = static_cast<float>(value); }
            else if (id == 2) { oscillatorVolumes[2] = static_cast<float>(value); }
            else if (id == 3) { oscillatorVolumes[3] = static_cast<float>(value); }
            else if (id == 4)  { spice      = static_cast<float>(value); }
            else if (id == 7)  { squeeze    = static_cast<float>(value); }
            else if (id == 9)  { glide      = static_cast<float>(value); }
            else if (id == 10) { distortion = static_cast<float>(value); }
            else if (id == 11) { sassiness   = static_cast<float>(value); }
            else if (id == 15) { frassiness   = static_cast<float>(value); }
            else if (id == 16) { chattiness   = static_cast<float>(value); }
            else if (id == 12) { xorRand   = static_cast<float>(value); }
            else if (id == 13) { attack    = static_cast<float>(value); }
            else if (id == 14) { release   = static_cast<float>(value); }
            else if (id == 5) {
                pitchBendSemitones = (static_cast<float>(value) - 0.5f) * 24.0f;
            }
        }
    }

    bool noteStateChanged = false;
    if (data.inputEvents) {
        Steinberg::int32 eventCount = data.inputEvents->getEventCount();
        for (Steinberg::int32 i = 0; i < eventCount; ++i) {
            Steinberg::Vst::Event event;
            if (data.inputEvents->getEvent(i, event) != Steinberg::kResultOk) continue;

            if (event.type == Steinberg::Vst::Event::kNoteOnEvent) {
                const int   pitch   = event.noteOn.pitch;
                const float newFreq = 440.0f * std::pow(2.0f, (pitch - 105) / 12.0f);
                const float vel     = event.noteOn.velocity;

                heldNotes.push_back({pitch, newFreq, vel});
                currentVelocity = vel;

                if (noteActive && currentFrequency > 0.0f && glide > 1e-4f) {
                    // Glide to new pitch — don't retrigger phase
                    targetFrequency   = newFreq;
                    const int slides  = std::max(1, static_cast<int>(sampleRate * glide * 2.0f));
                    portamentoRatio   = std::pow(targetFrequency / currentFrequency, 1.0f / slides);
                    portamentoSamples = slides;
                } else {
                    // Instant jump — retrigger phase
                    currentFrequency  = newFreq;
                    targetFrequency   = newFreq;
                    portamentoRatio   = 1.0f;
                    portamentoSamples = 0;
                    phase             = 0.0;
                }

                if (!noteActive) {
                    noteActive = true;
                    noteStateChanged = true;
                    // Reseed XOR-shift from nanosecond time so each note is unique
                    using clock = std::chrono::high_resolution_clock;
                    const auto nt = clock::now().time_since_epoch().count();
                    xorRandState = static_cast<uint32_t>(nt ^ (nt >> 32) ^ (nt >> 16));
                    if (xorRandState == 0) xorRandState = 0xDEADBEEFu;
                }

            } else if (event.type == Steinberg::Vst::Event::kNoteOffEvent) {
                const int pitch = event.noteOff.pitch;
                auto it = std::find_if(heldNotes.begin(), heldNotes.end(),
                    [pitch](const HeldNote& n) { return n.pitch == pitch; });
                if (it != heldNotes.end())
                    heldNotes.erase(it);

                if (heldNotes.empty()) {
                    if (noteActive) {
                        noteActive = false;
                        noteStateChanged = true;
                        ttsReadPos           = static_cast<double>(ttsBuffer.size());
                        ttsSilenceRemaining  = 0.0;  // next note-on plays immediately
                    }
                } else {
                    // Return to most recently pressed still-held note
                    const HeldNote& ret = heldNotes.back();
                    targetFrequency = ret.frequency;
                    currentVelocity = ret.velocity;

                    if (glide > 1e-4f && currentFrequency > 0.0f) {
                        const int slides  = std::max(1, static_cast<int>(sampleRate * glide * 2.0f));
                        portamentoRatio   = std::pow(targetFrequency / currentFrequency, 1.0f / slides);
                        portamentoSamples = slides;
                    } else {
                        currentFrequency  = targetFrequency;
                        portamentoRatio   = 1.0f;
                        portamentoSamples = 0;
                    }
                }
            }
        }
    }

    if (noteStateChanged && data.outputParameterChanges) {
        Steinberg::int32 index;
        auto* queue = data.outputParameterChanges->addParameterData(6, index);
        if (queue) {
            Steinberg::int32 pointIndex;
            queue->addPoint(0, noteActive ? 1.0 : 0.0, pointIndex);
        }
    }

    if (data.numOutputs == 0) return Steinberg::kResultOk;

    Steinberg::Vst::AudioBusBuffers& output = data.outputs[0];
    float** channels = output.channelBuffers32;
    if (!channels) return Steinberg::kResultOk;

    Steinberg::int32 numSamples  = data.numSamples;
    Steinberg::int32 numChannels = output.numChannels;

    const float pitchBendMult  = std::pow(2.0f, pitchBendSemitones / 12.0f);
    const float cAtk           = std::exp(-1.0f / (sampleRate * 0.010f));
    const float cRel           = std::exp(-1.0f / (sampleRate * 0.150f));
    const float noiseRelCoeff  = std::exp(-1.0f / (sampleRate * 0.150f));
    const float ampAtkCoeff    = std::exp(-1.0f / (sampleRate * std::max(0.001f, attack * 2.0f)));
    const float ampRelCoeff    = std::exp(-1.0f / (sampleRate * (0.010f + release * 2.990f)));
    // frassiness 0.5 = default pitch, 0.0 = -2 oct, 1.0 = +2 oct
    const float ttsBasePitch   = kTTSBasePitch * std::pow(2.0f, (frassiness - 0.5f) * 4.0f);

    for (Steinberg::int32 sample = 0; sample < numSamples; ++sample) {
        if (portamentoSamples > 0) {
            currentFrequency *= portamentoRatio;
            if (--portamentoSamples == 0)
                currentFrequency = targetFrequency;
        }

        const float effectiveFrequency = currentFrequency * pitchBendMult;
        float sampleValue = 0.0f;

        if (noteActive) {
            ampEnv = ampAtkCoeff * ampEnv + (1.0f - ampAtkCoeff);  // toward 1
            noiseEnv = 1.0f;
        } else {
            ampEnv   *= ampRelCoeff;
            noiseEnv *= noiseRelCoeff;
        }

        if (ampEnv > 1e-4f) {
            sampleValue = mixOscillators(phase) * currentVelocity * ampEnv;
            sampleValue = applyMandelbrot(sampleValue, spice, mandelbrotState);
            phase += effectiveFrequency / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
        }

        sampleValue = applyCompressor(sampleValue, squeeze, compressorEnv, cAtk, cRel);

        if (sassiness > 1e-4f && noteActive) {
            const size_t ttsIdx = static_cast<size_t>(ttsReadPos);

            // Arm silence gap the moment a word finishes (ttsSilenceRemaining == -1 means
            // "not yet armed for this gap"). 100% chattiness = 0 s gap, 0% = 10 s gap.
            if (ttsIdx >= ttsBuffer.size() && ttsSilenceRemaining < 0.0)
                ttsSilenceRemaining = static_cast<double>(1.0f - chattiness) * 10.0 * sampleRate;

            // Count down the gap
            if (ttsSilenceRemaining > 0.0)
                ttsSilenceRemaining -= 1.0;

            // Swap pre-rendered word in once silence has elapsed
            if (ttsIdx >= ttsBuffer.size() && ttsSilenceRemaining <= 0.0
                    && ttsPendingReady.load(std::memory_order_acquire)) {
                ttsBuffer           = std::move(ttsPending);
                ttsReadPos          = 0.0;
                ttsWordFadeIn       = 441;   // 10 ms fade-in in output-sample time
                ttsSilenceRemaining = -1.0;
                ttsPendingReady.store(false, std::memory_order_release);
            }

            // Pre-render next word in background whenever nothing is queued or rendering.
            // This runs as soon as the current word starts, hiding latency entirely.
            if (!ttsPendingReady.load(std::memory_order_acquire)
                    && !ttsRunning.exchange(true, std::memory_order_acq_rel)) {
                if (ttsThread.joinable()) ttsThread.join();
                const float sr = sampleRate;
                ttsThread = std::thread([this, sr]() {
                    static std::vector<int> s_bag;
                    static int              s_lastIdx    = -1;
                    static int              s_lastListSz = -1;

                    using clock = std::chrono::high_resolution_clock;
                    const auto t = clock::now().time_since_epoch().count();
                    std::mt19937 rng(static_cast<uint32_t>(t ^ (t >> 32)));

                    const bool            useCustom = s_useCustomWords && !s_customProfanities.empty();
                    const int             listSize  = useCustom ? static_cast<int>(s_customProfanities.size())
                                                                : kProfanitiesCount;
                    const ProfanityEntry* list      = useCustom ? s_customProfanities.data() : kProfanities;

                    if (s_bag.empty() || s_lastListSz != listSize) {
                        s_lastListSz = listSize;
                        s_bag.resize(listSize);
                        std::iota(s_bag.begin(), s_bag.end(), 0);
                        std::shuffle(s_bag.begin(), s_bag.end(), rng);
                        if (s_bag.back() == s_lastIdx && s_bag.size() > 1)
                            std::swap(s_bag.back(), s_bag[rng() % (s_bag.size() - 1)]);
                    }
                    const int widx = s_bag.back();
                    s_bag.pop_back();
                    s_lastIdx = widx;

                    auto buf = renderTTSWord(list[widx], sr);
                    trimSilence(buf);
                    ttsPending     = std::move(buf);
                    ttsPendingReady.store(true,  std::memory_order_release);
                    ttsRunning.store(false, std::memory_order_release);
                });
            }

            // Mix with pitch-shift
            if (ttsIdx < ttsBuffer.size()) {
                const float  frac = static_cast<float>(ttsReadPos - ttsIdx);
                const float  s0   = ttsBuffer[ttsIdx];
                const float  s1   = ttsIdx + 1 < ttsBuffer.size() ? ttsBuffer[ttsIdx + 1] : 0.0f;

                // Output-domain fade-in: ramps 0→1 over 10ms regardless of pitch ratio
                float ttsEnvGain = 1.0f;
                if (ttsWordFadeIn > 0) {
                    ttsEnvGain = 1.0f - static_cast<float>(ttsWordFadeIn) / 441.0f;
                    --ttsWordFadeIn;
                }
                // Output-domain fade-out: ramp 1→0 over last 10ms of buffer
                constexpr size_t kFadeOut = 441;
                if (ttsBuffer.size() > kFadeOut * 2 && ttsIdx + kFadeOut >= ttsBuffer.size()) {
                    const size_t samplesLeft = ttsBuffer.size() - ttsIdx;
                    ttsEnvGain *= static_cast<float>(samplesLeft) / static_cast<float>(kFadeOut);
                }

                sampleValue += (s0 + frac * (s1 - s0)) * sassiness * ttsEnvGain;

                const double pitchRatio = effectiveFrequency > 0.0f
                    ? static_cast<double>(effectiveFrequency) / ttsBasePitch : 1.0;
                ttsReadPos += pitchRatio;
            }
        }

        if (xorRand > 1e-4f && noiseEnv > 1e-4f) {
            xorRandState ^= xorRandState << 13;
            xorRandState ^= xorRandState >> 17;
            xorRandState ^= xorRandState << 5;
            const float xnoise = static_cast<float>(xorRandState) * (1.0f / 2147483648.0f) - 1.0f;
            sampleValue += xnoise * xorRand * noiseEnv;
        }

        sampleValue = applyDistortion(sampleValue, distortion, noiseEnv);

        for (Steinberg::int32 channel = 0; channel < numChannels; ++channel) {
            if (channels[channel])
                channels[channel][sample] = sampleValue;
        }
    }

    return Steinberg::kResultOk;
}

Steinberg::FUnknown* createProcessorInstance(void*) {
    return static_cast<Steinberg::Vst::IAudioProcessor*>(new VSTVibe2Processor());
}

} // namespace VSTVibe2
