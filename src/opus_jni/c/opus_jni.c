#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <opus.h>
#include <opus_types.h>
#include <errno.h>
#include <string.h>

#define APPLICATION OPUS_APPLICATION_AUDIO

typedef struct _OpusOptions {
    int frameSize;
    int sampleRate;
    int channels;
    int bitrate;
    int maxFrameSize;
    int maxPacketSize;
} OpusOptions;

typedef struct _OpusEncodeInfo {
    OpusEncoder* encoder;
    OpusOptions opts;
} OpusEncodeInfo;

typedef struct _OpusDecodeInfo {
    OpusDecoder* decoder;
    OpusOptions opts;
} OpusDecodeInfo;

jbyteArray as_byte_array(JNIEnv *env, unsigned char* buf, int len)
{
    jbyteArray array = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion (env, array, 0, len, (jbyte*) (buf));
    return array;
}

unsigned char* as_unsigned_char_array(JNIEnv *env, jbyteArray array)
{
    int len = (*env)->GetArrayLength(env, array);
    unsigned char* buf = (unsigned char*) malloc(len * sizeof(unsigned char));
    (*env)->GetByteArrayRegion (env, array, 0, len, (jbyte*) (buf));
    return buf;
}

OpusOptions readOpusOptions(JNIEnv *env, jobject obj)
{
    OpusOptions opts;

    jclass clsOpusOptions;

    jfieldID fFrameSize;
    jfieldID fSampleRate;
    jfieldID fChannels;
    jfieldID fBitrate;
    jfieldID fMaxFrameSize;
    jfieldID fMaxPacketSize;


    clsOpusOptions = (*env)->GetObjectClass(env, obj);

    fFrameSize = (*env)->GetFieldID(env, clsOpusOptions, "frameSize", "I");
    fSampleRate = (*env)->GetFieldID(env, clsOpusOptions, "sampleRate", "I");
    fChannels = (*env)->GetFieldID(env, clsOpusOptions, "channels", "I");
    fBitrate = (*env)->GetFieldID(env, clsOpusOptions, "bitrate", "I");
    fMaxFrameSize = (*env)->GetFieldID(env, clsOpusOptions, "maxFrameSize", "I");
    fMaxPacketSize = (*env)->GetFieldID(env, clsOpusOptions, "maxPacketSize", "I");

    opts.frameSize = (*env)->GetIntField(env, obj, fFrameSize);
    opts.sampleRate = (*env)->GetIntField(env, obj, fSampleRate);
    opts.channels = (*env)->GetIntField(env, obj, fChannels);
    opts.bitrate = (*env)->GetIntField(env, obj, fBitrate);
    opts.maxFrameSize = (*env)->GetIntField(env, obj, fMaxFrameSize);
    opts.maxPacketSize = (*env)->GetIntField(env, obj, fMaxPacketSize);

    return opts;
}

JNIEXPORT jlong JNICALL Java_de_zortax_opus_OpusHandler_createEncoder(JNIEnv *env, jobject inst, jobject obj)
{
    OpusEncoder *encoder;
    OpusOptions opts;
    OpusEncodeInfo* info = (OpusEncodeInfo*) malloc(sizeof(OpusEncodeInfo));
    int err;

    opts = readOpusOptions(env, obj);
    encoder = opus_encoder_create((opus_int32) opts.sampleRate, opts.channels, APPLICATION, &err);
    if (err < 0) {
        fprintf(stderr, "failed to create encoder: %s\n", opus_strerror(err));
        return -1;
    }

    err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(opts.bitrate));
    if (err < 0) {
        fprintf(stderr, "failed to set bitrate: %s\n", opus_strerror(err));
        return -1;
    }

    info->encoder = encoder;
    info->opts = opts;

    return (long) info;
}

JNIEXPORT jlong JNICALL Java_de_zortax_opus_OpusHandler_createDecoder(JNIEnv *env, jobject inst, jobject obj)
{
    OpusDecoder *decoder;
    OpusOptions opts;
    OpusDecodeInfo* info = (OpusDecodeInfo*) malloc(sizeof(OpusDecodeInfo));
    int err;

    opts = readOpusOptions(env, obj);
    decoder = opus_decoder_create(opts.sampleRate, opts.channels, &err);
    if (err < 0) {
        fprintf(stderr, "failed to create decoder: %s\n", opus_strerror(err));
        return -1;
    }


    info->decoder = decoder;
    info->opts = opts;

    return (long) info;
}

JNIEXPORT jbyteArray JNICALL Java_de_zortax_opus_OpusHandler_encodeFrame(JNIEnv *env, jobject inst, jlong pointer, jbyteArray in_buff)
{
    OpusEncodeInfo* info = (OpusEncodeInfo*) pointer;
    int i, nbBytes;
    jbyteArray out;
    unsigned char* pcm_bytes = as_unsigned_char_array(env, in_buff);
    unsigned char* cbits = (unsigned char*) malloc(info->opts.maxPacketSize);
    opus_int16 in[info->opts.frameSize * info->opts.channels * 2];

    for (i = 0; i < info->opts.frameSize * info->opts.channels; i++) {
        in[i] = pcm_bytes[2*i+1] << 8 | pcm_bytes[2*i];
    }
    nbBytes = opus_encode(info->encoder, in, info->opts.frameSize, cbits, info->opts.maxPacketSize);
    if (nbBytes < 0) {
        fprintf(stderr, "encode failed: %s\n", opus_strerror(nbBytes));
    }
    out = as_byte_array(env, cbits, nbBytes);
    free(cbits);
    free(pcm_bytes);
    return out;
}

JNIEXPORT jbyteArray JNICALL Java_de_zortax_opus_OpusHandler_decodeFrame(JNIEnv *env, jobject inst, jlong pointer, jbyteArray in_buff)
{
    OpusDecodeInfo* info = (OpusDecodeInfo*) pointer;
    int i, frame_size, len, out_len;
    jbyteArray out_buff;
    unsigned char* pcm_bytes;
    unsigned char* cbits = as_unsigned_char_array(env, in_buff);
    opus_int16 out[info->opts.maxFrameSize * info->opts.channels];

    len = (*env)->GetArrayLength (env, in_buff);
    frame_size = opus_decode(info->decoder, cbits, len, out, info->opts.maxFrameSize, 0);
    if (frame_size < 0) {
        fprintf(stderr, "decoder failed\n");
    }

    out_len = info->opts.channels * frame_size * 2;
    pcm_bytes = (unsigned char*) malloc(out_len);

    for (i = 0; i < info->opts.channels * frame_size; i++) {
        pcm_bytes[2 * i] = out[i] & 0xFF;
        pcm_bytes[2 * i + 1] = (out[i] >> 8) & 0xFF;
    }

    out_buff = as_byte_array(env, pcm_bytes, out_len);
    free(pcm_bytes);
    return out_buff;
}


JNIEXPORT void JNICALL Java_de_zortax_opus_OpusHandler_destroyEncoder(JNIEnv *env, jobject inst, jlong pointer)
{
    OpusEncodeInfo* info = (OpusEncodeInfo*) pointer;
    opus_encoder_destroy(info->encoder);
    free(info);
}

JNIEXPORT void JNICALL Java_de_zortax_opus_OpusHandler_destroyDecoder(JNIEnv *env, jobject inst, jlong pointer)
{
    OpusDecodeInfo* info = (OpusDecodeInfo*) pointer;
    opus_decoder_destroy(info->decoder);
    free(info);
}
