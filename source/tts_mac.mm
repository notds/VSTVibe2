#import <AVFoundation/AVFoundation.h>
#import <dispatch/dispatch.h>
#include "tts_mac.h"
#include <cstring>
#include <cstdlib>
#include <chrono>

std::vector<float> renderTTSWordMac(const wchar_t* word, const char* bcp47, float sampleRate) {
    std::vector<float> result;

    // Convert wchar_t* → NSString (macOS: wchar_t is 4-byte UTF-32 LE)
    NSString* nsWord = [[NSString alloc]
        initWithBytes:word
               length:wcslen(word) * sizeof(wchar_t)
             encoding:NSUTF32LittleEndianStringEncoding];
    if (!nsWord) return result;

    NSString* langCode = bcp47 ? [NSString stringWithUTF8String:bcp47] : @"en-US";
    // Match on primary language subtag only (first 2 chars, e.g. "fr" from "fr-FR")
    NSString* langPrefix = langCode.length >= 2 ? [langCode substringToIndex:2] : langCode;

    // Gender alternation (persists across calls)
    static AVSpeechSynthesisVoiceGender lastGender = AVSpeechSynthesisVoiceGenderFemale;
    AVSpeechSynthesisVoiceGender wantGender = (lastGender == AVSpeechSynthesisVoiceGenderMale)
        ? AVSpeechSynthesisVoiceGenderFemale : AVSpeechSynthesisVoiceGenderMale;

    NSArray<AVSpeechSynthesisVoice*>* allVoices = [AVSpeechSynthesisVoice speechVoices];

    // Sort into tiers: language+gender → language → gender → any
    NSMutableArray* langGender = [NSMutableArray array];
    NSMutableArray* langAny    = [NSMutableArray array];
    NSMutableArray* anyGender  = [NSMutableArray array];

    for (AVSpeechSynthesisVoice* v in allVoices) {
        BOOL ml = [v.language hasPrefix:langPrefix];
        BOOL mg = (v.gender == wantGender);
        if (ml && mg)  [langGender addObject:v];
        else if (ml)   [langAny    addObject:v];
        else if (mg)   [anyGender  addObject:v];
    }

    NSArray* candidates = langGender.count > 0 ? langGender
                        : langAny.count    > 0 ? langAny
                        : anyGender.count  > 0 ? anyGender
                        : allVoices;

    // Nanosecond-seeded random pick
    using clock = std::chrono::high_resolution_clock;
    const auto t = clock::now().time_since_epoch().count();
    const NSUInteger pick = (NSUInteger)(t % (unsigned long long)MAX(1, (int)candidates.count));
    AVSpeechSynthesisVoice* chosen = candidates.count > 0 ? candidates[pick] : nil;
    if (chosen) lastGender = chosen.gender;

    // Synthesize to PCM buffer(s)
    AVSpeechUtterance* utterance = [[AVSpeechUtterance alloc] initWithString:nsWord];
    utterance.rate   = AVSpeechUtteranceDefaultSpeechRate;
    utterance.volume = 1.0f;
    if (chosen) utterance.voice = chosen;

    __block std::vector<float>   pcm;
    __block float                srcRate = 22050.0f;
    dispatch_semaphore_t         sem     = dispatch_semaphore_create(0);
    AVSpeechSynthesizer*         synth   = [[AVSpeechSynthesizer alloc] init];

    [synth writeUtterance:utterance toBufferCallback:^(AVAudioBuffer* _Nonnull buf) {
        if (![buf isKindOfClass:[AVAudioPCMBuffer class]]) {
            dispatch_semaphore_signal(sem); return;
        }
        AVAudioPCMBuffer* pcmBuf = (AVAudioPCMBuffer*)buf;
        if (pcmBuf.frameLength == 0) {
            dispatch_semaphore_signal(sem); return;
        }
        srcRate = (float)pcmBuf.format.sampleRate;
        const float* ch = pcmBuf.floatChannelData[0];
        for (AVAudioFrameCount i = 0; i < pcmBuf.frameLength; ++i)
            pcm.push_back(ch[i]);
    }];

    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC));

    if (pcm.empty()) return result;

    // Resample to host sample rate
    if (std::abs(sampleRate - srcRate) < 1.0f) {
        result = std::move(pcm);
    } else {
        const double ratio  = sampleRate / srcRate;
        const size_t outLen = static_cast<size_t>(pcm.size() * ratio);
        result.resize(outLen);
        for (size_t i = 0; i < outLen; ++i) {
            const double srcPos = i / ratio;
            const size_t si     = static_cast<size_t>(srcPos);
            const float  frac   = static_cast<float>(srcPos - si);
            const float  s0     = pcm[si];
            const float  s1     = si + 1 < pcm.size() ? pcm[si + 1] : 0.0f;
            result[i]           = s0 + frac * (s1 - s0);
        }
    }

    return result;
}
