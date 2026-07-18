#include "napi/native_api.h"
#include "types/player/MidiEngine.h"

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "initEngine", nullptr, na_init, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "loadSF", nullptr, na_load_sf, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "playMidi", nullptr, na_play_midi, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "startPlayback", nullptr, na_start_playback, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "stopPlayback", nullptr, na_stop_playback, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "destroyEngine", nullptr, na_destroy, nullptr, nullptr, nullptr, napi_default, nullptr }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "midiplayer",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterMidiplayerModule(void)
{
    napi_module_register(&demoModule);
}
