//
// Created on 2026/7/19.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include <cstdint>
#include <thread>
#define TSF_IMPLEMENTATION

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200
#define LOG_TAG "MIDENG"

#include "MidiEngine.h"

// 定义Engine
std::unique_ptr<MidiEngine> engine; // 引擎指针

// 构造
MidiEngine::MidiEngine() {
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Midi Engine has been created");
    IsRendering = false; // 默认不渲染
}

// 析构
MidiEngine::~MidiEngine() {
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Midi Engine has been destroyed");
}

bool MidiEngine::LoadSF(void *data, size_t dataSize) {
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Try to load SF data");
    
    if (data == nullptr) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE: Soundfont data is null");
        return false;
    }
    
    MainSF = tsf_load_memory(data, dataSize);
    
    if (MainSF == nullptr) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE: Cannot load Soundfont data");
        return false;
    }
    
    tsf_set_output(MainSF, TSF_STEREO_INTERLEAVED, SAMPLE_RATE);
    
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Success to load Soundfont data");
    return true;
}

void MidiEngine::PlayMidi(int32_t preset, int32_t key, int32_t vel) {
    OH_LOG_DEBUG(
                 LOG_APP, 
                 "MIDI_ENGINE: Try to play a note, PRESET:%{public}d KEY:%{public}d VEL:%{public}d", 
                 preset,
                 key,
                 vel);
    
    if (MainSF == nullptr) {
        OH_LOG_INFO(LOG_APP, "MIDI_ENGINE: Cannot find MainSF data");
        return;
    }
    
    std::lock_guard<std::mutex> lock(MainMutex); //锁线程 防止意外
    
    if (vel == 0) {
        tsf_note_off(MainSF, preset, key);
        OH_LOG_DEBUG(
                 LOG_APP, 
                 "MIDI_ENGINE: played a note, PRESET:%{public}d KEY:%{public}d VEL:%{public}d", 
                 preset,
                 key,
                 vel);
    } else {
        tsf_note_on(MainSF, preset, key, vel);
        OH_LOG_DEBUG(
                 LOG_APP, 
                 "MIDI_ENGINE: stoped a note, PRESET:%{public}d KEY:%{public}d", 
                 preset,
                 key);
    }
}

bool MidiEngine::StartPlayback() {
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Try to start the audio rendering");
    
    if (Renderer != nullptr) {
        OH_LOG_INFO(LOG_APP, "MIDI_ENGINE: Audio rendering has already started");
        return false;
    }
    
    // FRAMES_PER_BUFFER的渲染缓冲大小 SAMPLE_RATE的采样率 双声道 float格式，RAW解码
    OH_AudioStreamBuilder_Create(&Builder, AUDIOSTREAM_TYPE_RENDERER);
    OH_AudioStreamBuilder_SetFrameSizeInCallback(Builder, FRAMES_PER_BUFFER);
    OH_AudioStreamBuilder_SetSamplingRate(Builder, SAMPLE_RATE);
    OH_AudioStreamBuilder_SetChannelCount(Builder, CHANNELS);
    OH_AudioStreamBuilder_SetSampleFormat(Builder, AUDIOSTREAM_SAMPLE_F32LE);
    OH_AudioStreamBuilder_SetEncodingType(Builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
    
    // 构建循环渲染
    OH_AudioRenderer_OnWriteDataCallback onWriteCb = OnWrite;
    OH_AudioStreamBuilder_SetRendererWriteDataCallback(Builder, onWriteCb, GetUserData());
    
    // 生成渲染器
    int32_t ret = OH_AudioStreamBuilder_GenerateRenderer(Builder, &Renderer);
    if (ret != AUDIOSTREAM_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE: Cannot generate audio renderer");
        OH_AudioStreamBuilder_Destroy(Builder);
        Builder = nullptr;
        return false;
    }
    
    // 正式开启渲染器
    ret = OH_AudioRenderer_Start(Renderer);
    if (ret != AUDIOSTREAM_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE: Cannot start audio renderer");
        OH_AudioRenderer_Stop(Renderer);
        OH_AudioRenderer_Release(Renderer);
        Renderer = nullptr;
        OH_AudioStreamBuilder_Destroy(Builder);
        Builder = nullptr;
        return false;
    }
    
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: rendering started");
    return true;
}

void MidiEngine::StopPlayback() {
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Try to stop rendering");
    
    // 等待渲染工作结束
    int count = 0;
    while (IsRendering.load() && count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        count++;
    }
    
    if (count == 100) {
        OH_LOG_WARN(LOG_APP, "MIDI_ENGINE: Rendering maybe stuck");
    }
    
    if (Renderer) {
        OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Try to clear Renderer");
        OH_AudioRenderer_Stop(Renderer);
        OH_AudioRenderer_Release(Renderer);
        Renderer = nullptr;
        OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Renderer cleared");
    }
    
    if (Builder) {
        OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Try to clear Builder");
        OH_AudioStreamBuilder_Destroy(Builder);
        Builder = nullptr;
        OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Builder cleared");
    }
    
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Rendering stopped");
}

bool MidiEngine::RenderMidi(float* buffer, size_t bufferSize) {
    IsRendering = true; // 开始渲染
    
    int32_t samples = bufferSize / sizeof(float); // 采样数量
    int32_t frames = samples / CHANNELS; // 渲染帧数量
    
    // 检查采样是否对齐，不对齐进行警告，返回（需要完整双声道）
    if (samples % CHANNELS != 0) {
        OH_LOG_WARN(LOG_APP, "MIDI_ENGINE: Samples number cannot align with channels");
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(MainMutex); //锁线程 防止意外
        
        if (MainSF) {
            tsf_render_float(MainSF, buffer, frames);
        } else {
            OH_LOG_WARN(LOG_APP, "MIDI_ENGINE: Cannot find MainSF data");
            memcpy(buffer, 0, bufferSize); // 没有就静音，跑个警告
        }
    }
    
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Finished a time rendering");
    IsRendering = false; // 停止渲染
    
    return true;
}

void* MidiEngine::GetUserData() {
    return this;
}

OH_AudioData_Callback_Result MidiEngine::OnWrite(
    OH_AudioRenderer *renderer, 
    void *userData, void *audioData, 
    int32_t audioDataSize
) {
    OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Render Write");
    
    auto* g_engine = static_cast<MidiEngine*>(userData);
    
    bool filled = engine->RenderMidi(static_cast<float*>(audioData), audioDataSize);
    
    if (filled) {
        OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Render Valid");
        return AUDIO_DATA_CALLBACK_RESULT_VALID;
    } else {
        OH_LOG_DEBUG(LOG_APP, "MIDI_ENGINE: Render Invalid");
        return AUDIO_DATA_CALLBACK_RESULT_INVALID;
    }
}

napi_value na_init(napi_env env, napi_callback_info info) {
    if (!engine) {
        engine = std::make_unique<MidiEngine>();
    }
    
    napi_value result;
    napi_get_boolean(env, true, &result);
    
    return result;
}

napi_value na_destroy(napi_env env, napi_callback_info info) {
    if (engine) {
        engine.reset();
        if (engine->MainSF) {
            tsf_note_off_all(engine->MainSF);
        }
    }
    
    return nullptr;
}

napi_value na_load_sf(napi_env env, napi_callback_info info) {
    if (!engine) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE_NAPI: No Engine");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE_NAPI_LOADSF: The number of arguments is wrong");
    }
    
    void* data = nullptr;
    size_t dataSize = 0;
    
    bool is_arraybuffer = false;
    napi_status status = napi_is_arraybuffer(env, args[0], &is_arraybuffer);
    
    if (!is_arraybuffer) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE_NAPI_LOADSF: The type of argument is wrong");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    napi_get_arraybuffer_info(env, args[0], &data, &dataSize);
    
    if (status != napi_ok || !data || dataSize <= 0) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE_NAPI_LOADSF: Memory data wrong");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    bool ok = engine->LoadSF(data, dataSize);
    
    napi_value result;
    napi_get_boolean(env, ok, &result);
    return result;
}

napi_value na_play_midi(napi_env env, napi_callback_info info) {
    if (!engine) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE_NAPI: No Engine");
        return nullptr;
    }
    
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 3) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE_NAPI_PLAY_MIDI: The number of arguments is wrong");
    }
    
    int32_t preset;
    int32_t key;
    int32_t vel;
    
    napi_get_value_int32(env, args[0], &preset);
    napi_get_value_int32(env, args[1], &key);
    napi_get_value_int32(env, args[2], &vel);
    
    engine->PlayMidi(preset, key, vel);
    
    return nullptr;
}

napi_value na_start_playback(napi_env env, napi_callback_info info) {
    if (!engine) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE_NAPI: No Engine");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    bool ok = engine->StartPlayback();
    
    napi_value result;
    napi_get_boolean(env, ok, &result);
    return result;
}

napi_value na_stop_playback(napi_env env, napi_callback_info info) {
    if (!engine) {
        OH_LOG_ERROR(LOG_APP, "MIDI_ENGINE_NAPI: No Engine");
        return nullptr;
    }
    
    engine->StopPlayback();
    
    return nullptr;
}