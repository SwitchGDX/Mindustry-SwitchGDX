#include "Clearwing.h"
#include "java/nio/Buffer.h"
#include <arc/audio/Soloud.h>
#include <arc/util/ArcRuntimeException.h>

#include "soloud.h"
#include "soloud_file.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"
#include "soloud_bus.h"
#include "soloud_thread.h"
#include "soloud_filter.h"
#include "soloud_biquadresonantfilter.h"
#include "soloud_echofilter.h"
#include "soloud_lofifilter.h"
#include "soloud_flangerfilter.h"
#include "soloud_waveshaperfilter.h"
#include "soloud_bassboostfilter.h"
#include "soloud_robotizefilter.h"
#include "soloud_freeverbfilter.h"

using namespace SoLoud;

Soloud soloud;

void throwError(jcontext ctx, int result) {
    constructAndThrowMsg<&class_arc_util_ArcRuntimeException, &init_arc_util_ArcRuntimeException_java_lang_String>(ctx, soloud.getErrorString(result));
}


void SM_arc_audio_Soloud_init(jcontext ctx) {
    int result = soloud.init();

    if (result != 0) throwError(ctx, result);
}

void SM_arc_audio_Soloud_deinit(jcontext ctx) {
    soloud.deinit();
}

jobject SM_arc_audio_Soloud_backendString_R_java_lang_String(jcontext ctx) {
    return (jobject)stringFromNative(ctx, soloud.getBackendString());
}

jint SM_arc_audio_Soloud_backendId_R_int(jcontext ctx) {
    return soloud.getBackendId();
}

jint SM_arc_audio_Soloud_backendChannels_R_int(jcontext ctx) {
    return soloud.getBackendChannels();
}

jint SM_arc_audio_Soloud_backendSamplerate_R_int(jcontext ctx) {
    return soloud.getBackendSamplerate();
}

jint SM_arc_audio_Soloud_backendBufferSize_R_int(jcontext ctx) {
    return soloud.getBackendBufferSize();
}

jint SM_arc_audio_Soloud_version_R_int(jcontext ctx) {
    return soloud.getVersion();
}

void SM_arc_audio_Soloud_stopAll(jcontext ctx) {
    soloud.stopAll();
}

void SM_arc_audio_Soloud_pauseAll_boolean(jcontext ctx, jbool paused) {
    soloud.setPauseAll(paused);
}

void SM_arc_audio_Soloud_biquadSet_long_int_float_float(jcontext ctx, jlong handle, jint type, jfloat frequency, jfloat resonance) {
    ((BiquadResonantFilter *) handle)->setParams(type, frequency, resonance);
}

void SM_arc_audio_Soloud_echoSet_long_float_float_float(jcontext ctx, jlong handle, jfloat delay, jfloat decay, jfloat filter) {
    ((EchoFilter *) handle)->setParams(delay, decay, filter);
}

void SM_arc_audio_Soloud_lofiSet_long_float_float(jcontext ctx, jlong handle, jfloat sampleRate, jfloat bitDepth) {
    ((LofiFilter *) handle)->setParams(sampleRate, bitDepth);
}

void SM_arc_audio_Soloud_flangerSet_long_float_float(jcontext ctx, jlong handle, jfloat delay, jfloat frequency) {
    ((FlangerFilter *) handle)->setParams(delay, frequency);
}

void SM_arc_audio_Soloud_waveShaperSet_long_float(jcontext ctx, jlong handle, jfloat amount) {
    ((WaveShaperFilter *) handle)->setParams(amount);
}

void SM_arc_audio_Soloud_bassBoostSet_long_float(jcontext ctx, jlong handle, jfloat amount) {
    ((BassboostFilter *) handle)->setParams(amount);
}

void SM_arc_audio_Soloud_robotizeSet_long_float_int(jcontext ctx, jlong handle, jfloat freq, jint waveform) {
    ((RobotizeFilter *) handle)->setParams(freq, waveform);
}

void SM_arc_audio_Soloud_freeverbSet_long_float_float_float_float(jcontext ctx, jlong handle, jfloat mode, jfloat roomSize, jfloat damp, jfloat width) {
    ((FreeverbFilter *) handle)->setParams(mode, roomSize, damp, width);
}

jlong SM_arc_audio_Soloud_filterBiquad_R_long(jcontext ctx) {
    return (jlong) (new BiquadResonantFilter());
}

jlong SM_arc_audio_Soloud_filterEcho_R_long(jcontext ctx) {
    return (jlong) (new EchoFilter());
}

jlong SM_arc_audio_Soloud_filterLofi_R_long(jcontext ctx) {
    return (jlong) (new LofiFilter());
}

jlong SM_arc_audio_Soloud_filterFlanger_R_long(jcontext ctx) {
    return (jlong) (new FlangerFilter());
}

jlong SM_arc_audio_Soloud_filterBassBoost_R_long(jcontext ctx) {
    return (jlong) (new BassboostFilter());
}

jlong SM_arc_audio_Soloud_filterWaveShaper_R_long(jcontext ctx) {
    return (jlong) (new WaveShaperFilter());
}

jlong SM_arc_audio_Soloud_filterRobotize_R_long(jcontext ctx) {
    return (jlong) (new RobotizeFilter());
}

jlong SM_arc_audio_Soloud_filterFreeverb_R_long(jcontext ctx) {
    return (jlong) (new FreeverbFilter());
}

void SM_arc_audio_Soloud_setGlobalFilter_int_long(jcontext ctx, jint index, jlong handle) {
    soloud.setGlobalFilter(index, ((Filter *) handle));
}

void SM_arc_audio_Soloud_filterFade_int_int_int_float_float(jcontext ctx, jint voice, jint filter, jint attribute, jfloat value, jfloat timeSec) {
    soloud.fadeFilterParameter(voice, filter, attribute, value, timeSec);
}

void SM_arc_audio_Soloud_filterSet_int_int_int_float(jcontext ctx, jint voice, jint filter, jint attribute, jfloat value) {
    soloud.setFilterParameter(voice, filter, attribute, value);
}

jlong SM_arc_audio_Soloud_busNew_R_long(jcontext ctx) {
    return (jlong) (new Bus());
}

jlong SM_arc_audio_Soloud_wavLoad_Array1_byte_int_R_long(jcontext ctx, jobject bytes_object, jint length) {
    auto bytes = (jbyte *) ((jarray) bytes_object)->data;

    Wav *wav = new Wav();

    int result = wav->loadMem((unsigned char *) bytes, length, true, true);

    if (result != 0) throwError(ctx, result);

    return (jlong) wav;
}

void SM_arc_audio_Soloud_idSeek_int_float(jcontext ctx, jint id, jfloat seconds) {
    soloud.seek(id, seconds);
}

void SM_arc_audio_Soloud_idVolume_int_float(jcontext ctx, jint id, jfloat volume) {
    soloud.setVolume(id, volume);
}

jfloat SM_arc_audio_Soloud_idGetVolume_int_R_float(jcontext ctx, jint id) {
    return soloud.getVolume(id);
}

void SM_arc_audio_Soloud_idPan_int_float(jcontext ctx, jint id, jfloat pan) {
    soloud.setPan(id, pan);
}

void SM_arc_audio_Soloud_idPitch_int_float(jcontext ctx, jint id, jfloat pitch) {
    soloud.setRelativePlaySpeed(id, pitch);
}

void SM_arc_audio_Soloud_idPause_int_boolean(jcontext ctx, jint id, jbool pause) {
    soloud.setPause(id, pause);
}

jbool SM_arc_audio_Soloud_idGetPause_int_R_boolean(jcontext ctx, jint voice) {
    return soloud.getPause(voice);
}

void SM_arc_audio_Soloud_idProtected_int_boolean(jcontext ctx, jint id, jbool protect) {
    soloud.setProtectVoice(id, protect);
}

void SM_arc_audio_Soloud_idStop_int(jcontext ctx, jint voice) {
    soloud.stop(voice);
}

void SM_arc_audio_Soloud_idLooping_int_boolean(jcontext ctx, jint voice, jbool looping) {
    soloud.setLooping(voice, looping);
}

jbool SM_arc_audio_Soloud_idGetLooping_int_R_boolean(jcontext ctx, jint voice) {
    return soloud.getLooping(voice);
}

jfloat SM_arc_audio_Soloud_idPosition_int_R_float(jcontext ctx, jint voice) {
    return (jfloat) soloud.getStreamPosition(voice);
}

jbool SM_arc_audio_Soloud_idValid_int_R_boolean(jcontext ctx, jint voice) {
    return soloud.isValidVoiceHandle(voice);
}

jlong SM_arc_audio_Soloud_streamLoad_java_lang_String_R_long(jcontext ctx, jobject path_object) {
    auto path = stringToNative(ctx, (jstring) path_object);

    WavStream *stream = new WavStream();

    int result = stream->load(path);

    if (result != 0) throwError(ctx, result);

    return (jlong) stream;
}

jdouble SM_arc_audio_Soloud_streamLength_long_R_double(jcontext ctx, jlong handle) {
    WavStream *source = (WavStream *) handle;
    return (jdouble) source->getLength();
}

void SM_arc_audio_Soloud_sourceDestroy_long(jcontext ctx, jlong handle) {
    AudioSource *source = (AudioSource *) handle;
    delete source;
}

void SM_arc_audio_Soloud_sourceInaudible_long_boolean_boolean(jcontext ctx, jlong handle, jbool tick, jbool play) {
    AudioSource *wav = (AudioSource *) handle;
    wav->setInaudibleBehavior(tick, play);
}

jint SM_arc_audio_Soloud_sourcePlay_long_R_int(jcontext ctx, jlong handle) {
    AudioSource *wav = (AudioSource *) handle;
    return soloud.play(*wav);
}

jint SM_arc_audio_Soloud_sourceCount_long_R_int(jcontext ctx, jlong handle) {
    AudioSource *wav = (AudioSource *) handle;
    return soloud.countAudioSource(*wav);
}

jint SM_arc_audio_Soloud_sourcePlay_long_float_float_float_boolean_R_int(jcontext ctx, jlong handle, jfloat volume, jfloat pitch, jfloat pan, jbool loop) {
    AudioSource *wav = (AudioSource *) handle;

    int voice = soloud.play(*wav, volume, pan, false);
    soloud.setLooping(voice, loop);
    soloud.setRelativePlaySpeed(voice, pitch);

    return voice;
}

jint SM_arc_audio_Soloud_sourcePlayBus_long_long_float_float_float_boolean_R_int(jcontext ctx, jlong handle, jlong busHandle, jfloat volume, jfloat pitch, jfloat pan, jbool loop) {
    AudioSource *wav = (AudioSource *) handle;
    Bus *bus = (Bus *) busHandle;

    int voice = bus->play(*wav, volume, pan, false);
    soloud.setLooping(voice, loop);
    soloud.setRelativePlaySpeed(voice, pitch);

    return voice;
}

void SM_arc_audio_Soloud_sourceLoop_long_boolean(jcontext ctx, jlong handle, jbool loop) {
    AudioSource *source = (AudioSource *) handle;
    source->setLooping(loop);
}

void SM_arc_audio_Soloud_sourceStop_long(jcontext ctx, jlong handle) {
    AudioSource *source = (AudioSource *) handle;
    source->stop();
}

void SM_arc_audio_Soloud_sourceFilter_long_int_long(jcontext ctx, jlong handle, jint index, jlong filter) {
    ((AudioSource *) handle)->setFilter(index, ((Filter *) filter));
}
