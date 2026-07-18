//
// Created on 2026/7/19.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef MIDIPLAYER_MIDIENGINE_H
#define MIDIPLAYER_MIDIENGINE_H

#include "napi/native_api.h"
#include "types/tsf/tsf.h"
#include "ohaudio/native_audiorenderer.h"
#include "ohaudio/native_audiostreambuilder.h"
#include "hilog/log.h"
#include <cstdint>
#include <mutex>

// MIDI播放引擎
class MidiEngine {
public:
    MidiEngine(); // 构造函数
    ~MidiEngine(); // 析构函数
    
private:
    OH_AudioStreamBuilder* Builder = nullptr; // 音频数据构建器
    OH_AudioRenderer* Renderer = nullptr; // 音频渲染器
    
    std::mutex MainMutex; // 主要线程Mutex 用于锁线程防止意外
    std::atomic<bool> IsRendering;
    
public:
    tsf* MainSF = nullptr; // 主sf2文件
    
    // 音频参数
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int CHANNELS = 2;
    static constexpr int FRAMES_PER_BUFFER = 128;   // 每次合成的帧数
    static constexpr size_t BUFFER_SIZE = FRAMES_PER_BUFFER * CHANNELS * sizeof(float);
    
public:
    bool LoadSF(void* data, size_t dataSize); // 加载soundfont文件
    
    void PlayMidi(int32_t preset, int32_t key, int32_t vel); // 播放一个单音
    
    bool StartPlayback(); // 开启声音渲染
    
    void StopPlayback(); // 关闭声音渲染
    
    bool RenderMidi(float* buffer, size_t bufferSize); // 渲染函数，用于最终的渲染
    
    void* GetUserData(); // 用于获取引擎指针的函数
    
    static OH_AudioData_Callback_Result OnWrite(
        OH_AudioRenderer* renderer,
        void* userData,
        void* audioData,
        int32_t audioDataSize
    );
    
private:
    
};

// 完全公开函数和变量
extern std::unique_ptr<MidiEngine> engine; // 引擎指针

napi_value na_init(napi_env env, napi_callback_info info);

napi_value na_load_sf(napi_env env, napi_callback_info info);

napi_value na_play_midi(napi_env env, napi_callback_info info);

napi_value na_start_playback(napi_env env, napi_callback_info info);

napi_value na_stop_playback(napi_env env, napi_callback_info info);

napi_value na_destroy(napi_env env, napi_callback_info info);

#endif //MIDIPLAYER_MIDIENGINE_H
