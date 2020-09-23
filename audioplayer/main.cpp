#include <stdio.h>
#include <iostream>
 
#define __STDC_CONSTANT_MACROS

//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <SDL2/SDL.h>
#import <OpenAL/al.h>
#import <OpenAL/alc.h>
#include <OpenAL/alut.h>
#ifdef __cplusplus
};
#endif

#define NUMBUFFERS              (8)
using namespace std;
 
int64_t channelLayout = 0;
int sampleRate = 0;
int channels = 0;
AVSampleFormat sampleFormat;
ALuint alBufferArray[NUMBUFFERS];

//从packet内读取信息到frame，并重采样后放置到buffer
int feedDataBack(AVFormatContext *pfrmtcxt,AVCodecContext *pcdctx,SwrContext* swrctx,int ai,ALuint src, ALuint bid){
    AVFrame* aFrame = av_frame_alloc();
    AVPacket *pkt = (AVPacket*)av_malloc(sizeof(AVPacket));
    while (av_read_frame(pfrmtcxt, pkt) == 0) {
        if (pkt->stream_index == ai) {
            int ret = avcodec_send_packet(pcdctx, pkt);
            if (ret == 0) {
                av_packet_unref(pkt);
                ret = avcodec_receive_frame(pcdctx, aFrame);
                if ( ret == 0 ) {
                    int bufferSize = av_rescale_rnd(aFrame->nb_samples, sampleRate, sampleRate, AV_ROUND_UP) * 2 * 2 * 1.2;
                    uint8_t* outBuffer= (uint8_t*)av_malloc(sizeof(uint8_t) * bufferSize);
                    int outSamples = swr_convert(swrctx, &outBuffer, bufferSize,
                        (const uint8_t**)&aFrame->data[0], aFrame->nb_samples);
                    int outDataSize =
                        av_samples_get_buffer_size(nullptr, 2, outSamples, AV_SAMPLE_FMT_S16, 1);
                    alBufferData(bid, AL_FORMAT_STEREO16, outBuffer, outDataSize, sampleRate);
                    alSourceQueueBuffers(src, 1, &bid);
                    return 0;
                }
                else {
                    return -1;
                }
            }
            else if(ret == AVERROR(EAGAIN)){
                cout << "Could not decode, buffer is full!" << endl;
                return -1;
            }
            else {
                cout << "Fail to send packet to decoder!" << endl;
                return -1;
            }
        }
    }
    return 0;
}
 
int main(int argc, char* argv[])
{
    char* filepath;
    if (argc==2) {
        filepath = argv[1];
    }
    else {
        cout << "Usage: audioplayer <filepath>" << endl;
        return -1;
    }
    
    AVFormatContext *pformatcxt;
    AVCodec *pcode;
    AVCodecContext *pcodectx;
    
    //初始化
    av_register_all();
    avformat_network_init();
    pformatcxt=avformat_alloc_context();
    
    //打开视频文件
    if(avformat_open_input(&pformatcxt, filepath, NULL, NULL)!=0){
        cout<<"打开文件失败！"<<endl;
        return -1;
    }
    
    //查找视频文件的视频流
    if(avformat_find_stream_info(pformatcxt, NULL)<0){
        cout<<"未找到音频流信息！"<<endl;
        return -1;
    }
    
    int ai=-1;
    
    for(int i=0;i<pformatcxt->nb_streams;i++){
        if(pformatcxt->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            ai=i;
            break;
        }
    }
 
    //未找到音频流
    if(ai==-1){
        cout<<"未找到音频流！"<<endl;
        return -1;
    }
    
    //构建解码器上下文
    pcodectx=pformatcxt->streams[ai]->codec;
    
    //寻找解码器
    pcode=avcodec_find_decoder(pcodectx->codec_id);
    if(pcode==NULL){
        cout<<"未找到解码器！"<<endl;
        return -1;
    }
    
    //打开解码器
    if(avcodec_open2(pcodectx, pcode, NULL)<0){
        cout<<"打开解码器失败"<<endl;
        return -1;
    }
    av_dump_format(pformatcxt, 0,  filepath, 0);//输出文件信息
    
    int64_t in_channel_layout = av_get_default_channel_layout(pcodectx->channels);
    channelLayout = pcodectx->channel_layout;
    sampleRate = pcodectx->sample_rate;
    channels = pcodectx->channels;
    sampleFormat = AVSampleFormat((int)pcodectx->sample_fmt);
    
    //初始化swr
    struct SwrContext* au_convert_ctx = swr_alloc();
    
    au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,
                                        AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                                        sampleRate, in_channel_layout,
                                        sampleFormat, sampleRate, 0,
                                        NULL);

    swr_init(au_convert_ctx);
    
    
    //设置OpenAL
    ALCdevice* pDevice;
    ALCcontext* pContext;
    ALuint m_source;
    
    pDevice = alcOpenDevice(NULL);
    pContext = alcCreateContext(pDevice, NULL);
    alcMakeContextCurrent(pContext);
    
    
    alGenSources(1, &m_source);

    ALfloat SourcePos[] = { 0.0, 0.0, 0.0 };
    ALfloat SourceVel[] = { 0.0, 0.0, 0.0 };
    ALfloat ListenerPos[] = { 0.0, 0, 0 };
    ALfloat ListenerVel[] = { 0.0, 0.0, 0.0 };
    // first 3 elements are "at", second 3 are "up"
    ALfloat ListenerOri[] = { 0.0, 0.0, -1.0,  0.0, 1.0, 0.0 };
    alSourcef(m_source, AL_PITCH, 1.0);
    alSourcef(m_source, AL_GAIN, 1.0);
    alSourcefv(m_source, AL_POSITION, SourcePos);
    alSourcefv(m_source, AL_VELOCITY, SourceVel);
    alSourcef(m_source, AL_REFERENCE_DISTANCE, 50.0f);
    alSourcei(m_source, AL_LOOPING, AL_FALSE);
    alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    alListener3f(AL_POSITION, 0, 0, 0);

    alGenBuffers(NUMBUFFERS, alBufferArray);
    
    for(int i=0;i<NUMBUFFERS;i++){
        feedDataBack(pformatcxt,pcodectx,au_convert_ctx,ai,m_source,alBufferArray[i]);
    }
    
    // Start playing source
    alSourcePlay(m_source);
    
    ALint iBuffersProcessed;
    ALint iState;
    ALuint bufferId;
    ALint iQueuedBuffers;
    while (true) {
        iBuffersProcessed = 0;
        //获取处理完的buffer数
        alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
        //更新buffer中内容
        while (iBuffersProcessed > 0) {
            bufferId = 0;
            alSourceUnqueueBuffers(m_source, 1, &bufferId);
            feedDataBack(pformatcxt,pcodectx,au_convert_ctx,ai,m_source,bufferId);
            iBuffersProcessed -= 1;
        }
        alGetSourcei(m_source, AL_SOURCE_STATE, &iState);
        if (iState != AL_PLAYING) {
            alGetSourcei(m_source, AL_BUFFERS_QUEUED, &iQueuedBuffers);
            if (iQueuedBuffers) {
                alSourcePlay(m_source);
            }
            else {
                // Finished playing
                break;
            }
        }
    }

    
    //收尾工作
    swr_free(&au_convert_ctx);
    
    alDeleteBuffers(NUMBUFFERS, alBufferArray);
    alDeleteSources(1, &m_source);
    
    ALCcontext* pCurContext;
    ALCdevice* pCurDevice;
    
    pCurContext = alcGetCurrentContext();
    pCurDevice = alcGetContextsDevice(pCurContext);
    
    alcMakeContextCurrent(NULL);
    alcDestroyContext(pCurContext);
    alcCloseDevice(pCurDevice);
    
    avcodec_close(pcodectx);
    avformat_close_input(&pformatcxt);
    
    return 0;
}

