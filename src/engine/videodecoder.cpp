#include "videodecoder.h"
#include "cpueffects.h"
#include <algorithm>
#include <QDebug>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#ifndef AV_CH_LAYOUT_MONO
#define AV_CH_LAYOUT_MONO 0x0000000000000004ULL
#endif
#ifndef AV_CH_LAYOUT_STEREO
#define AV_CH_LAYOUT_STEREO 0x0000000000000003ULL
#endif

VideoDecoder::VideoDecoder() {
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    swFrame = av_frame_alloc();
}

void VideoDecoder::setDatamoshing(bool enabled, double iDropProb, double pDupProb, int pDupCount, double pDropProb) {
    datamoshEnabled = enabled;
    iFrameDropProb = iDropProb;
    pFrameDuplicateProb = pDupProb;
    pFrameDuplicateCount = std::max(1, pDupCount);
    pFrameDropProb = pDropProb;
}

void VideoDecoder::setOpticalSmear(bool smearEnabled, double frameMerge, double frameSmear, double colorBleed, double lumaBias) {
    opticalSmearEnabled = smearEnabled;
    mergeStrength = frameMerge;
    smearStrength = frameSmear;
    bleedStrength = colorBleed;
    lumaStrength = lumaBias;
}

void VideoDecoder::setCpuXor(bool xorEnabled, double xorValue, double intensity) {
    cpuXorEnabled = xorEnabled;
    this->cpuXorValue = xorValue;
    this->cpuXorIntensity = intensity;
}

void VideoDecoder::setCpuOr(bool orEnabled, double orValue, double intensity) {
    cpuOrEnabled = orEnabled;
    this->cpuOrValue = orValue;
    this->cpuOrIntensity = intensity;
}

void VideoDecoder::setCpuAnd(bool andEnabled, double andValue, double intensity) {
    cpuAndEnabled = andEnabled;
    this->cpuAndValue = andValue;
    this->cpuAndIntensity = intensity;
}

void VideoDecoder::setCpuXnor(bool xnorEnabled, double xnorValue, double intensity) {
    cpuXnorEnabled = xnorEnabled;
    this->cpuXnorValue = xnorValue;
    this->cpuXnorIntensity = intensity;
}

void VideoDecoder::setCpuNand(bool nandEnabled, double nandValue, double intensity) {
    cpuNandEnabled = nandEnabled;
    this->cpuNandValue = nandValue;
    this->cpuNandIntensity = intensity;
}

void VideoDecoder::setPlaybackQuality(int downscaleFactor) {
    if (downscaleFactor < 1) downscaleFactor = 1;
    if (playbackQualityScale != downscaleFactor) {
        playbackQualityScale = downscaleFactor;
        if (swsCtx) {
            std::lock_guard<std::mutex> lock(decodeMutex);
            sws_freeContext(swsCtx);
            swsCtx = nullptr;
        }
        referenceFrameRgb.clear();
        hasReferenceFrame = false;
    }
}

VideoDecoder::~VideoDecoder() {
    close();
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&swFrame);
}

bool VideoDecoder::openFile(const std::string& filePath) {
    close();

    if (!packet) {
        packet = av_packet_alloc();
    }
    if (!frame) {
        frame = av_frame_alloc();
    }
    if (!swFrame) {
        swFrame = av_frame_alloc();
    }

    if (!packet || !frame || !swFrame) {
        qWarning() << "FFmpeg: Failed to allocate decoder working frames/buffers";
        return false;
    }

    hwPixFmt = -1;
    duration = 0.0;
    width = 0;
    height = 0;
    fps = 0.0;
    timeBase = 0.0;
    lastDecodedTime = -999.0;
    hasReferenceFrame = false;
    referenceFrameRgb.clear();
    audioSamples.clear();

    if (avformat_open_input(&formatCtx, filePath.c_str(), nullptr, nullptr) != 0) {
        qWarning() << "FFmpeg: Failed to open video file:" << QString::fromStdString(filePath);
        return false;
    }

    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        qWarning() << "FFmpeg: Failed to find stream info";
        return false;
    }

    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        qWarning() << "FFmpeg: No video stream found. Treating as audio-only track.";
    } else {
        AVStream* stream = formatCtx->streams[videoStreamIndex];
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) {
            qWarning() << "FFmpeg: Unsupported video codec";
            return false;
        }

        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            return false;
        }

        if (avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0) {
            qWarning() << "FFmpeg: Failed to copy codec params to context";
            return false;
        }

        codecCtx->thread_count = 4;
        codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        codecCtx->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
        codecCtx->err_recognition = 0; 

        auto codecSupportsHwAccel = [](AVCodecID codecId) -> bool {
            switch (codecId) {
            case AV_CODEC_ID_H264:
            case AV_CODEC_ID_HEVC:
            case AV_CODEC_ID_VP8:
            case AV_CODEC_ID_VP9:
            case AV_CODEC_ID_AV1:
            case AV_CODEC_ID_MPEG2VIDEO:
            case AV_CODEC_ID_MPEG4:
            case AV_CODEC_ID_VC1:
                return true;
            default:
                return false;
            }
        };
        auto tryEnableHwAccel = [&](AVHWDeviceType type) -> bool {
            if (!codecSupportsHwAccel(codec->id)) {
                return false;
            }

            if (type == AV_HWDEVICE_TYPE_NONE) {
                return false;
            }

            AVBufferRef* testDeviceCtx = nullptr;
            if (av_hwdevice_ctx_create(&testDeviceCtx, type, nullptr, nullptr, 0) < 0) {
                return false;
            }

            int selectedPixFmt = -1;
            for (int i = 0;; ++i) {
                const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
                if (!config) break;
                if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == type) {
                    selectedPixFmt = config->pix_fmt;
                    break;
                }
            }

            if (selectedPixFmt == -1) {
                av_buffer_unref(&testDeviceCtx);
                return false;
            }

            hwDeviceCtx = testDeviceCtx;
            hwPixFmt = selectedPixFmt;
            codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
            codecCtx->opaque = this;
            codecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
                VideoDecoder* dec = static_cast<VideoDecoder*>(ctx->opaque);
                for (const enum AVPixelFormat* p = pix_fmts; *p != -1; ++p) {
                    if (*p == dec->hwPixFmt) return *p;
                }
                return AV_PIX_FMT_NONE;
            };

            return true;
        };

        bool hwEnabled = false;
        hwEnabled = tryEnableHwAccel(av_hwdevice_find_type_by_name("d3d11va"));
        if (!hwEnabled && codecSupportsHwAccel(codec->id)) {
            hwEnabled = tryEnableHwAccel(av_hwdevice_find_type_by_name("cuda"));
        }

        if (!hwEnabled) {
            hwPixFmt = -1;
        }

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            qWarning() << "FFmpeg: Failed to open video codec";
            return false;
        }

        width = codecCtx->width;
        height = codecCtx->height;
        lastDecodedTime = -999.0;
        fps = av_q2d(stream->avg_frame_rate);
        timeBase = av_q2d(stream->time_base);
    }

    duration = formatCtx->duration / (double)AV_TIME_BASE;

    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
            break;
        }
    }

    if (audioStreamIndex != -1) {
        AVStream* astream = formatCtx->streams[audioStreamIndex];
        const AVCodec* acodec = avcodec_find_decoder(astream->codecpar->codec_id);
        if (acodec) {
            audioCodecCtx = avcodec_alloc_context3(acodec);
            if (avcodec_parameters_to_context(audioCodecCtx, astream->codecpar) >= 0) {
                if (avcodec_open2(audioCodecCtx, acodec, nullptr) >= 0) {
                    AVChannelLayout out_layout;
                    av_channel_layout_default(&out_layout, 2); 

                    AVChannelLayout in_layout = astream->codecpar->ch_layout;
                    if (in_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
                        av_channel_layout_default(&in_layout, in_layout.nb_channels > 0 ? in_layout.nb_channels : 2);
                    }

                    swrCtx = nullptr;
                    int err = swr_alloc_set_opts2(
                        &swrCtx,
                        &out_layout, AV_SAMPLE_FMT_FLT, 44100,
                        &in_layout, static_cast<AVSampleFormat>(astream->codecpar->format), astream->codecpar->sample_rate,
                        0, nullptr
                    );
                    if (err < 0 || !swrCtx) {
                        qWarning() << "FFmpeg: Failed to allocate SwrContext:" << err;
                        swrCtx = nullptr;
                    } else {
                        if (swr_init(swrCtx) < 0) {
                            qWarning() << "FFmpeg: Failed to initialize SwrContext";
                            swr_free(&swrCtx);
                            swrCtx = nullptr;
                        }
                    }
                }
            }
        }
    }

    if (audioStreamIndex != -1) {
        preDecodeAudio(filePath);
    }

    return true;
}

void VideoDecoder::preDecodeAudio(const std::string& filePath) {
    AVFormatContext* aFormatCtx = nullptr;
    if (avformat_open_input(&aFormatCtx, filePath.c_str(), nullptr, nullptr) != 0) return;
    if (avformat_find_stream_info(aFormatCtx, nullptr) < 0) {
        avformat_close_input(&aFormatCtx);
        return;
    }

    int aStreamIndex = -1;
    const AVCodec* aCodec = nullptr;
    for (unsigned int i = 0; i < aFormatCtx->nb_streams; i++) {
        if (aFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            aStreamIndex = i;
            aCodec = avcodec_find_decoder(aFormatCtx->streams[i]->codecpar->codec_id);
            break;
        }
    }

    if (aStreamIndex == -1 || !aCodec) {
        avformat_close_input(&aFormatCtx);
        return;
    }

    AVCodecContext* aCodecCtx = avcodec_alloc_context3(aCodec);
    avcodec_parameters_to_context(aCodecCtx, aFormatCtx->streams[aStreamIndex]->codecpar);
    if (avcodec_open2(aCodecCtx, aCodec, nullptr) < 0) {
        avcodec_free_context(&aCodecCtx);
        avformat_close_input(&aFormatCtx);
        return;
    }

    AVPacket* audioPacket = av_packet_alloc();
    AVFrame* audioFrame = av_frame_alloc();

    SwrContext* swrCtx = nullptr;
    AVChannelLayout stereoLayout = AV_CHANNEL_LAYOUT_STEREO;
    if (swr_alloc_set_opts2(
            &swrCtx,
            &stereoLayout, AV_SAMPLE_FMT_FLT, 44100,
            &aCodecCtx->ch_layout, aCodecCtx->sample_fmt, aCodecCtx->sample_rate,
            0, nullptr) < 0 || !swrCtx || swr_init(swrCtx) < 0) {
        qWarning() << "FFmpeg: Failed to initialize audio resampler for predecode.";
        if (swrCtx) {
            swr_free(&swrCtx);
        }
        av_frame_free(&audioFrame);
        av_packet_free(&audioPacket);
        avcodec_free_context(&aCodecCtx);
        avformat_close_input(&aFormatCtx);
        return;
    }

    while (av_read_frame(aFormatCtx, audioPacket) >= 0) {
        if (audioPacket->stream_index == aStreamIndex) {
            if (avcodec_send_packet(aCodecCtx, audioPacket) == 0) {
                while (avcodec_receive_frame(aCodecCtx, audioFrame) == 0) {
                    int maxOutSamples = swr_get_out_samples(swrCtx, audioFrame->nb_samples);
                    std::vector<float> resampled(maxOutSamples * 2);
                    uint8_t* outData[1] = { reinterpret_cast<uint8_t*>(resampled.data()) };

                    int converted = swr_convert(
                        swrCtx, 
                        outData, maxOutSamples, 
                        (const uint8_t**)audioFrame->data, audioFrame->nb_samples
                    );

                    if (converted > 0) {
                        audioSamples.insert(audioSamples.end(), resampled.begin(), resampled.begin() + converted * 2);
                    }
                }
            }
        }
        av_packet_unref(audioPacket);
    }

    av_packet_free(&audioPacket);
    av_frame_free(&audioFrame);

    av_seek_frame(formatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
    if (codecCtx) avcodec_flush_buffers(codecCtx);
    if (audioCodecCtx) avcodec_flush_buffers(audioCodecCtx);
}

void VideoDecoder::close() {
    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if (swFrame) {
        av_frame_free(&swFrame);
        swFrame = nullptr;
    }
    if (packet) {
        av_packet_free(&packet);
        packet = nullptr;
    }
    if (audioCodecCtx) {
        avcodec_free_context(&audioCodecCtx);
        audioCodecCtx = nullptr;
    }
    audioStreamIndex = -1;
    audioSamples.clear();

    if (swsCtx) {
        sws_freeContext(swsCtx);
        swsCtx = nullptr;
    }
    if (hwDeviceCtx) {
        av_buffer_unref(&hwDeviceCtx);
        hwDeviceCtx = nullptr;
    }
    if (swrCtx) {
        swr_free(&swrCtx);
        swrCtx = nullptr;
    }
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }
    if (formatCtx) {
        avformat_close_input(&formatCtx);
        formatCtx = nullptr;
    }
    videoStreamIndex = -1;
    hwPixFmt = -1;
    duration = 0.0;
    width = 0;
    height = 0;
    fps = 0.0;
    timeBase = 0.0;
    lastDecodedTime = -999.0;
    hasReferenceFrame = false;
    referenceFrameRgb.clear();
}

bool VideoDecoder::seekTo(double timestamp) {
    if (!formatCtx || videoStreamIndex == -1) return false;
    if (timeBase <= 0.0) return false;

    AVStream* stream = formatCtx->streams[videoStreamIndex];
    int64_t seekTarget = static_cast<int64_t>(timestamp / timeBase);

    if (avformat_seek_file(formatCtx, videoStreamIndex, INT64_MIN, seekTarget, seekTarget, AVSEEK_FLAG_BACKWARD) < 0) {
        return false;
    }

    avcodec_flush_buffers(codecCtx);
    return true;
}

bool VideoDecoder::decodeFrameAt(double timestamp, DecodedVideoFrame& outFrame) {
    std::lock_guard<std::mutex> decodeLock(decodeMutex);

    if (videoStreamIndex == -1) {
        outFrame.timestamp = timestamp;
        outFrame.width = 1280 / playbackQualityScale;
        outFrame.height = 720 / playbackQualityScale;
        outFrame.rgbData.assign(outFrame.width * outFrame.height * 3, 0);
        return true;
    }

    if (!formatCtx || !codecCtx) return false;

    timestamp = std::clamp(timestamp, 0.0, duration);

    double tolerance = (fps > 0.0) ? (0.5 / fps) : 0.015;

    if (lastDecodedTime >= 0.0 && std::abs(timestamp - lastDecodedTime) <= tolerance && !referenceFrameRgb.empty()) {
        outFrame.timestamp = lastDecodedTime;
        outFrame.width = width / playbackQualityScale;
        outFrame.height = height / playbackQualityScale;
        outFrame.rgbData = referenceFrameRgb;
        return true;
    }

    bool needSeek = (lastDecodedTime < 0.0) || (timestamp < lastDecodedTime - tolerance) || (timestamp - lastDecodedTime > 1.0);

    if (needSeek) {
        if (!seekTo(timestamp)) {
            return false;
        }
        lastDecodedTime = timestamp;
        hasReferenceFrame = false; 
    }

    bool isCatchingUp = !needSeek && (timestamp - lastDecodedTime > (2.0 / std::max(fps, 1.0)));
    if (isCatchingUp) {
        codecCtx->skip_frame = AVDISCARD_NONREF;
    }
    int maxTries = 200;
    while (maxTries-- > 0) {
        int readRet = av_read_frame(formatCtx, packet);
        if (readRet < 0) {
            break; 
        }

        if (packet->stream_index == videoStreamIndex) {
            bool isIFrame = (packet->flags & AV_PKT_FLAG_KEY);
            int sendCount = 1;
            if (isIFrame) {
                if (datamoshEnabled && hasReferenceFrame) {
                    double r = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
                    if (r < iFrameDropProb) {
                        sendCount = 0;
                    }
                }
                hasReferenceFrame = true;
            } else if (hasReferenceFrame && datamoshEnabled) {
                double r = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
                if (r < pFrameDuplicateProb) {
                    sendCount = pFrameDuplicateCount;
                } else if (r < pFrameDuplicateProb + pFrameDropProb) {
                    sendCount = 0;
                }
            }

            for (int i = 0; i < sendCount; ++i) {
                int sendRes = avcodec_send_packet(codecCtx, packet);
                if (sendRes == AVERROR(EAGAIN) || sendRes < 0) break;
            }
            av_packet_unref(packet);

            while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                AVFrame* activeFrame = frame;
                if (frame->format == hwPixFmt) {
                    av_frame_unref(swFrame);
                    if (av_hwframe_transfer_data(swFrame, frame, 0) < 0) {
                        continue;
                    }
                    activeFrame = swFrame;
                }
                int64_t pts = activeFrame->best_effort_timestamp;
                if (pts == AV_NOPTS_VALUE) {
                    pts = activeFrame->pts;
                }

                bool hasValidPts = (pts != AV_NOPTS_VALUE);
                double framePts = hasValidPts ? (pts * timeBase) : timestamp;

                double frameTolerance = (fps > 0.0) ? (0.5 / fps) : 0.0;
                if (!hasValidPts || framePts >= timestamp - frameTolerance) {
                    int outWidth = width / playbackQualityScale;
                    int outHeight = height / playbackQualityScale;
                    swsCtx = sws_getCachedContext(
                        swsCtx,
                        width, height, (AVPixelFormat)activeFrame->format,
                        outWidth, outHeight, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr
                    );
                    if (!swsCtx) {
                        qDebug() << "VideoEngine Trace: swsCtx is NULL! Crash imminent.";
                        return false;
                    }
                    outFrame.timestamp = framePts;
                    outFrame.width = outWidth;
                    outFrame.height = outHeight;
                    size_t rgbSize = outWidth * outHeight * 3;
                    if (outFrame.rgbData.size() != rgbSize) {
                        outFrame.rgbData.resize(rgbSize);
                    }
                    uint8_t* dest[4] = { outFrame.rgbData.data(), nullptr, nullptr, nullptr };
                    int linesize[4] = { outWidth * 3, 0, 0, 0 };
                    dest[0] += linesize[0] * (outFrame.height - 1);
                    linesize[0] = -linesize[0];
                    sws_scale(
                        swsCtx,
                        activeFrame->data,
                        activeFrame->linesize,
                        0,
                        height, 
                        dest,
                        linesize
                    );

                    if (opticalSmearEnabled && !referenceFrameRgb.empty()) {
                        CpuEffects::blendWithPreviousFrame(outFrame.rgbData, referenceFrameRgb, mergeStrength, smearStrength, bleedStrength, lumaStrength);
                    }
                    if (cpuXorEnabled && !referenceFrameRgb.empty()) {
                        CpuEffects::cpuXorWithPreviousFrame(outFrame.rgbData, referenceFrameRgb, cpuXorValue, cpuXorIntensity);
                    }
                    if (cpuOrEnabled && !referenceFrameRgb.empty()) {
                        CpuEffects::cpuOrWithPreviousFrame(outFrame.rgbData, referenceFrameRgb, cpuOrValue, cpuOrIntensity);
                    }
                    if (cpuAndEnabled && !referenceFrameRgb.empty()) {
                        CpuEffects::cpuAndWithPreviousFrame(outFrame.rgbData, referenceFrameRgb, cpuAndValue, cpuAndIntensity);
                    }
                    if (cpuXnorEnabled && !referenceFrameRgb.empty()) {
                        CpuEffects::cpuXnorWithPreviousFrame(outFrame.rgbData, referenceFrameRgb, cpuXnorValue, cpuXnorIntensity);
                    }
                    if (cpuNandEnabled && !referenceFrameRgb.empty()) {
                        CpuEffects::cpuNandWithPreviousFrame(outFrame.rgbData, referenceFrameRgb, cpuNandValue, cpuNandIntensity);
                    }

                    referenceFrameRgb = outFrame.rgbData;
                    hasReferenceFrame = true;
                    lastDecodedTime = framePts;
                    if (isCatchingUp) {
                        codecCtx->skip_frame = AVDISCARD_DEFAULT;
                    }
                    return true;
                }
            }
        } else {
            av_packet_unref(packet);
        }
    }

    return false;
}
