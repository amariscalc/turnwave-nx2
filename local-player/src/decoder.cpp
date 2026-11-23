#include "decoder.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <libavutil/replaygain.h>
#include <stdexcept>
extern "C" {
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
static void check(int result, const char* action) {
    if (result >= 0) return;
    char message[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(result, message, sizeof message);
    throw std::runtime_error(std::string(action) + ": " + message);
}
void Decoder::close() {
    setSpeed(1.);
    av_packet_free(&packet);
    av_frame_free(&frame);
    swr_free(&converter);
    avcodec_free_context(&codec);
    avformat_close_input(&file);
}
void Decoder::open(const std::string& path) {
    close();
    mediaPath=path;
    draining = false;
    discardBefore = -1;
    AVDictionary* options = nullptr;
    av_dict_set(&options, "protocol_whitelist", "file", 0);
    av_dict_set(&options, "format_whitelist", "aac,aiff,ape,asf,caf,flac,matroska,webm,mov,mp4,m4a,3gp,3g2,mj2,mp3,ogg,wav,wv", 0);
    int opened = avformat_open_input(&file, ("file:" + path).c_str(), nullptr, &options);
    av_dict_free(&options);
    check(opened,"Open file");
    check(avformat_find_stream_info(file,nullptr),"Read audio header");
    const AVCodec* decoder = nullptr;
    stream = av_find_best_stream(file, AVMEDIA_TYPE_AUDIO,-1,-1,&decoder,0);
    check(stream,"Find audio track");
    auto* track = file->streams[stream];
    codec = avcodec_alloc_context3(decoder);
    if (!codec) throw std::bad_alloc();
    check(avcodec_parameters_to_context(codec,track->codecpar),"Read codec");
    codec->pkt_timebase = track->time_base;
    check(avcodec_open2(codec,decoder,nullptr),"Open decoder");
    sourceRate = codec->sample_rate;
    channels = codec->ch_layout.nb_channels;
    bits = codec->bits_per_raw_sample ? codec->bits_per_raw_sample : codec->bits_per_coded_sample;
    kbps = (codec->bit_rate ? codec->bit_rate : file->bit_rate) / 1000;
    format = avcodec_get_name(codec->codec_id);
    duration = track->duration != AV_NOPTS_VALUE ? track->duration * av_q2d(track->time_base) :
        (file->duration != AV_NOPTS_VALUE ? file->duration / double(AV_TIME_BASE) : 0);
    auto tag = [&](const char* key) -> std::string {
        const auto* entry = av_dict_get(track->metadata,key,nullptr,0);
        if (!entry) entry = av_dict_get(file->metadata,key,nullptr,0);
        return entry ? entry->value : "";
    };
    title = tag("title"); artist = tag("artist"); album = tag("album");
    genre=tag("genre");
    auto number=[](const std::string& text) {return int(std::clamp(std::strtol(text.c_str(),nullptr,10),0L,9999L));};
    discNumber=number(tag("disc"));trackNumber=number(tag("track"));
    double trackDb=NAN,albumDb=NAN,trackPeak=0.,albumPeak=0.;
    const auto* side=av_packet_side_data_get(track->codecpar->coded_side_data,track->codecpar->nb_coded_side_data,AV_PKT_DATA_REPLAYGAIN);
    if(side && side->size>=sizeof(AVReplayGain)) {
        AVReplayGain gain{};
        std::memcpy(&gain,side->data,sizeof gain);
        if(gain.track_gain!=INT32_MIN) trackDb=gain.track_gain/100000.;
        if(gain.album_gain!=INT32_MIN) albumDb=gain.album_gain/100000.;
        trackPeak=gain.track_peak/100000.;albumPeak=gain.album_peak/100000.;
    }
    auto readValue=[](const std::string& value,double fallback) {
        char* end=nullptr;
        double result=std::strtod(value.c_str(),&end);
        return end!=value.c_str() && std::isfinite(result)?result:fallback;
    };
    trackDb=readValue(tag("replaygain_track_gain"),trackDb);
    albumDb=readValue(tag("replaygain_album_gain"),albumDb);
    trackPeak=readValue(tag("replaygain_track_peak"),trackPeak);
    albumPeak=readValue(tag("replaygain_album_peak"),albumPeak);
    auto scale=[](double db,double peak) {
        if(!std::isfinite(db)) return 1.;
        double gain=std::pow(10.,std::clamp(db,-40.,24.)/20.);
        return peak>0?std::min(gain,1./peak):gain;
    };
    trackGain=scale(trackDb,trackPeak);
    albumGain=std::isfinite(albumDb)?scale(albumDb,albumPeak):trackGain;
    cover.clear();
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    check(swr_alloc_set_opts2(&converter,&stereo,AV_SAMPLE_FMT_FLT,rate,&codec->ch_layout,
                              codec->sample_fmt,sourceRate,0,nullptr),"Configure PCM");
    check(swr_init(converter),"Initialize PCM");
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!frame || !packet) throw std::bad_alloc();
}
int Decoder::convert(const uint8_t** input,int count,std::vector<float>& samples) {
    int capacity = std::max(64,swr_get_out_samples(converter,count));
    samples.resize(size_t(capacity)*2);
    auto* output = reinterpret_cast<uint8_t*>(samples.data());
    int written = swr_convert(converter,&output,capacity,input,count);
    check(written,"Convert PCM");
    samples.resize(size_t(written)*2);
    return written;
}
bool Decoder::readSource(std::vector<float>& samples) {
    for (;;) {
        int status = avcodec_receive_frame(codec,frame);
        if (status == AVERROR_EOF) return convert(nullptr,0,samples)>0;
        if (status >= 0) {
            int count = convert(const_cast<const uint8_t**>(frame->extended_data),frame->nb_samples,samples);
            if (discardBefore >= 0 && frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                auto* track = file->streams[stream];
                const auto start = track->start_time == AV_NOPTS_VALUE ? 0 : track->start_time;
                double frameTime = (frame->best_effort_timestamp-start)*av_q2d(track->time_base);
                int skip = std::clamp(std::round((discardBefore-frameTime)*rate),0.0,double(count));
                samples.erase(samples.begin(),samples.begin()+skip*2);
                if (skip < count) discardBefore = -1;
            }
            av_frame_unref(frame);
            if (!samples.empty()) return true;
            continue;
        }
        if (status != AVERROR(EAGAIN)) check(status,"Decode audio");
        if (draining) throw std::runtime_error("Unexpected end of decoder stream");
        for (;;) {
            status = av_read_frame(file,packet);
            if (status == AVERROR_EOF) {
                check(avcodec_send_packet(codec,nullptr),"Drain decoder");
                draining = true;
                break;
            }
            check(status,"Read packet");
            if (packet->stream_index == stream) {
                status = avcodec_send_packet(codec,packet);
                av_packet_unref(packet);
                check(status,"Submit packet");
                break;
            }
            av_packet_unref(packet);
        }
    }
}
bool Decoder::seek(double seconds) {
    auto* track = file->streams[stream];
    const auto start = track->start_time == AV_NOPTS_VALUE ? 0 : track->start_time;
    const int64_t target = start + llround(seconds/av_q2d(track->time_base));
    if (av_seek_frame(file,stream,target,AVSEEK_FLAG_BACKWARD)<0) return false;
    avcodec_flush_buffers(codec);
    swr_close(converter);
    check(swr_init(converter),"Reset PCM");
    draining = false;
    discardBefore = seconds;
    return true;
}

void Decoder::loadCover() {
    if(!file || !cover.empty()) return;
    for(unsigned i=0;i<file->nb_streams;i++) {
        auto* attachment=file->streams[i];
        auto* params=attachment->codecpar;
        if(!(attachment->disposition&AV_DISPOSITION_ATTACHED_PIC) || params->codec_id!=AV_CODEC_ID_MJPEG ||
           attachment->attached_pic.size<=0 || attachment->attached_pic.size>8*1024*1024) continue;
        if(decodeCover(attachment->attached_pic)) return;
    }
    namespace fs=std::filesystem;
    for(const char* name:{"cover.jpg","folder.jpg","cover.jpeg","folder.jpeg"}) {
        std::ifstream file(fs::path(mediaPath).parent_path()/name,std::ios::binary|std::ios::ate);
        auto size=file.tellg();
        if(size<=0 || size>8*1024*1024) continue;
        AVPacket* packet=av_packet_alloc();
        if(!packet) return;
        bool loaded=false;
        if(av_new_packet(packet,int(size))>=0) {
            file.seekg(0);
            if(file.read(reinterpret_cast<char*>(packet->data),size)) loaded=decodeCover(*packet);
        }
        av_packet_free(&packet);
        if(loaded) return;
    }
}
bool Decoder::decodeCover(const AVPacket& input) {
    const auto* decoder=avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    if(!decoder) return false;
    AVCodecContext* image=avcodec_alloc_context3(decoder);
    AVFrame* picture=av_frame_alloc();
    if(image) image->max_pixels=4096*4096;
    if(image && picture && avcodec_open2(image,decoder,nullptr)>=0 && avcodec_send_packet(image,&input)>=0 &&
       avcodec_receive_frame(image,picture)>=0 && picture->width>0 && picture->height>0) {
        int w=std::max(1,coverSide*picture->width/std::max(picture->width,picture->height));
        int h=std::max(1,coverSide*picture->height/std::max(picture->width,picture->height));
        auto* scale=sws_getContext(picture->width,picture->height,AVPixelFormat(picture->format),
                                   w,h,AV_PIX_FMT_BGRA,SWS_BILINEAR,nullptr,nullptr,nullptr);
        if(scale) {
            cover.assign(coverSide*coverSide,0xff101218);
            constexpr int paddedSide=coverSide+16;
            std::vector<uint32_t> scaled(paddedSide*(coverSide+1));
            uint8_t* dest[]={reinterpret_cast<uint8_t*>(scaled.data()),nullptr,nullptr,nullptr};
            int stride[]={paddedSide*4,0,0,0};
            if(sws_scale(scale,picture->data,picture->linesize,0,picture->height,dest,stride)==h) {
                for(int y=0;y<h;y++)
                    std::copy_n(scaled.data()+y*paddedSide,w,cover.data()+((coverSide-h)/2+y)*coverSide+(coverSide-w)/2);
            } else cover.clear();
            sws_freeContext(scale);
        }
    }
    av_frame_free(&picture);
    avcodec_free_context(&image);
    return !cover.empty();
}
