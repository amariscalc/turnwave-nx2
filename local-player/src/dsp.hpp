#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>
#include <cstdint>

struct Equalizer {
    static constexpr std::array<int,10> hz{60,170,310,600,1000,3000,6000,12000,14000,16000};
    std::array<int,10> gain{};
    int volume=100, balance=0, preamp=0;
    bool on=false;
    struct Filter {
        double b0=1,b1=0,b2=0,a1=0,a2=0;
        double z1[2]{},z2[2]{};
        double apply(double x,int ch) {
            double y = b0*x + z1[ch];
            z1[ch] = b1*x-a1*y+z2[ch]; z2[ch] = b2*x-a2*y;
            return y;
        }
    };
    std::array<Filter,10> filters{};
    void rebuild() {
        for (int i=0;i<10;i++) {
            double w=2*3.141592653589793*hz[i]/48000, a=std::pow(10.,gain[i]/40.);
            double alpha=std::sin(w)/2.8, den=1+alpha/a;
            filters[i]={(1+alpha*a)/den,-2*std::cos(w)/den,(1-alpha*a)/den,
                        -2*std::cos(w)/den,(1-alpha/a)/den};
        }
    }
    void render(const float* data,int frames,int16_t* out,double replayGain=1.) {
        double scale=replayGain*volume/100.0*(on ? std::pow(10.,preamp/20.) : 1.);
        double level[2]={scale*(1-std::max(0,balance)/100.),scale*(1+std::min(0,balance)/100.)};
        for (int f=0;f<frames;f++) for(int c=0;c<2;c++) {
            double x=data[f*2+c];
            if(on) for(int i=0;i<10;i++) if(gain[i]) x=filters[i].apply(x,c);
            out[f*2+c]=static_cast<int16_t>(std::clamp(std::lrint(std::clamp(x*level[c],-1.,1.)*32768.),-32768L,32767L));
        }
    }
};
inline std::array<float,32> spectrum(const int16_t* samples,int frames) {
    constexpr int n=512;
    std::array<std::complex<float>,n> bins{};
    for(int i=0;i<std::min(frames,n);++i) {
        float window=0.5f-0.5f*std::cos(6.2831853f*i/(n-1));
        bins[i]=window*(samples[2*i]+samples[2*i+1])/65536.f;
    }
    for(int i=1,j=0;i<n;++i) {
        int bit=n>>1; for(;j&bit;bit>>=1) j^=bit;
        j^=bit; if(i<j) std::swap(bins[i],bins[j]);
    }
    for(int len=2;len<=n;len*=2) {
        auto step=std::polar(1.f,-6.2831853f/len);
        for(int i=0;i<n;i+=len) {
            std::complex<float> w=1;
            for(int j=0;j<len/2;j++,w*=step) {
                auto a=bins[i+j], b=bins[i+j+len/2]*w;
                bins[i+j]=a+b; bins[i+j+len/2]=a-b;
            }
        }
    }
    std::array<float,32> bars{};
    for(int i=0;i<32;i++) {
        int first=1+int(std::pow(240.f,i/32.f)-1), last=std::min(256,std::max(first+1,int(std::pow(240.f,(i+1)/32.f))));
        float peak=0;
        for(int k=first;k<last;k++) peak=std::max(peak,std::abs(bins[k])/128.f);
        bars[i]=std::clamp((20*std::log10(peak+1e-8f)+66.f)/66.f,0.f,1.f);
    }
    return bars;
}
