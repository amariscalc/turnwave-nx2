#include "decoder.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}
static void checked(int result) {
    if(result<0) throw std::runtime_error("Audio tempo filter failed");
}
void Decoder::setSpeed(double speed) {
    avfilter_graph_free(&tempo);
    tempoEnded=false;tempoPts=0;
    if(speed==1.) return;
    auto* graph=avfilter_graph_alloc();
    if(!graph) throw std::bad_alloc();
    graph->nb_threads=1;
    AVFilterContext *input=nullptr,*filter=nullptr,*output=nullptr;
    char args[24];
    std::snprintf(args,sizeof args,"%.2f",speed);
    try {
        checked(avfilter_graph_create_filter(&input,avfilter_get_by_name("abuffer"),"input",
            "sample_rate=48000:sample_fmt=flt:channel_layout=stereo:time_base=1/48000",nullptr,graph));
        checked(avfilter_graph_create_filter(&filter,avfilter_get_by_name("atempo"),"tempo",args,nullptr,graph));
        checked(avfilter_graph_create_filter(&output,avfilter_get_by_name("abuffersink"),"output",nullptr,nullptr,graph));
        checked(avfilter_link(input,0,filter,0));
        checked(avfilter_link(filter,0,output,0));
        checked(avfilter_graph_config(graph,nullptr));
    } catch(...) {avfilter_graph_free(&graph);throw;}
    tempo=graph;tempoIn=input;tempoOut=output;
}
bool Decoder::read(std::vector<float>& samples) {
    if(!tempo) return readSource(samples);
    auto freeFrame=[](AVFrame* f){av_frame_free(&f);};
    std::unique_ptr<AVFrame,decltype(freeFrame)> frame(av_frame_alloc(),freeFrame);
    if(!frame) throw std::bad_alloc();
    for(;;) {
        int result=av_buffersink_get_frame(tempoOut,frame.get());
        if(result==AVERROR_EOF) return false;
        if(result>=0) {
            samples.resize(size_t(frame->nb_samples)*2);
            std::memcpy(samples.data(),frame->data[0],samples.size()*sizeof(float));
            if(!samples.empty()) return true;
            av_frame_unref(frame.get());continue;
        }
        if(result!=AVERROR(EAGAIN) || tempoEnded) {checked(result);return false;}
        if(!readSource(tempoBuffer)) {
            checked(av_buffersrc_add_frame_flags(tempoIn,nullptr,0));
            tempoEnded=true;continue;
        }
        frame->format=AV_SAMPLE_FMT_FLT;frame->sample_rate=rate;
        frame->nb_samples=int(tempoBuffer.size()/2);frame->pts=tempoPts;tempoPts+=frame->nb_samples;
        av_channel_layout_default(&frame->ch_layout,2);
        checked(av_frame_get_buffer(frame.get(),0));
        std::memcpy(frame->data[0],tempoBuffer.data(),tempoBuffer.size()*sizeof(float));
        checked(av_buffersrc_add_frame_flags(tempoIn,frame.get(),0));
        av_frame_unref(frame.get());
    }
}
