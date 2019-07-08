/*
 * Copyright (c) 2017 Jun Zhao
 * Copyright (c) 2017 Kaixuan Liu
 *
 * HW Acceleration API (video decoding) decode sample
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * HW-Accelerated decoding example.
 *
 * @example hw_decode.c
 * This example shows how to do HW-accelerated decoding with output
 * frames from the HW video surfaces.
 */

#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>

#include <Windows.h>

static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = NULL;
static int nFrameDecode = 0;
static BOOL bHWDecode = FALSE;
static BOOL bOnlyDecode = TRUE;

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                      NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    return err;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

static int decode_write(AVCodecContext *avctx, AVPacket *packet)
{
    AVFrame *frame = NULL, *sw_frame = NULL;
    AVFrame *tmp_frame = NULL;
    uint8_t *buffer = NULL;
    int size;
    int ret = 0;

    ret = avcodec_send_packet(avctx, packet);
    if (ret < 0) {
        fprintf(stderr, "Error during decoding\n");
        return ret;
    }

    while (1) {
        if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
            fprintf(stderr, "Can not alloc frame\n");
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            av_frame_free(&sw_frame);
            fprintf(stderr, "receive_frame return %d, frame decode %d\n", ret, nFrameDecode);
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error while decoding\n");
            goto fail;
        }
        nFrameDecode++;
        
        if (bHWDecode && frame->format == hw_pix_fmt) {
            /* retrieve data from GPU to CPU */
            if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                fprintf(stderr, "Error transferring the data to system memory\n");
                goto fail;
            }
            tmp_frame = sw_frame;
        } else
            tmp_frame = frame;
        if (bOnlyDecode)
        {
            goto fail;
        }
        size = av_image_get_buffer_size(tmp_frame->format, tmp_frame->width,
                                        tmp_frame->height, 1);
        buffer = av_malloc(size);
        if (!buffer) {
            fprintf(stderr, "Can not alloc buffer\n");
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        ret = av_image_copy_to_buffer(buffer, size,
                                      (const uint8_t * const *)tmp_frame->data,
                                      (const int *)tmp_frame->linesize, tmp_frame->format,
                                      tmp_frame->width, tmp_frame->height, 1);
        if (ret < 0) {
            fprintf(stderr, "Can not copy image to buffer\n");
            goto fail;
        }
        if (output_file != NULL && (ret = fwrite(buffer, 1, size, output_file)) < 0) {
            fprintf(stderr, "Failed to dump raw data.\n");
            goto fail;
        }

    fail:
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
        av_freep(&buffer);
        if (ret < 0)
            return ret;
    }
}

static int InputHWDeviceType()
{
    int nHWDeviceType = 0;
    do
    {
        fprintf(stderr, "1.vdpau\n");
        fprintf(stderr, "2.cuda\n");
        fprintf(stderr, "3.vaapi\n");
        fprintf(stderr, "4.dxva2\n");
        fprintf(stderr, "5.qsv\n");
        fprintf(stderr, "6.videotoolbox\n");
        fprintf(stderr, "7.d3d11va\n");
        fprintf(stderr, "8.drm\n");
        fprintf(stderr, "9.opencl\n");
        fprintf(stderr, "10.mediacodec\n");
        fprintf(stderr, "Input devcie type(num), -1 to exit:\n");
        scanf_s("%d", &nHWDeviceType);
        if (nHWDeviceType == -1)
        {
            exit(-1);
        }
        if (nHWDeviceType < 1 || nHWDeviceType > 10)
        {
            fprintf(stderr, "input device type invalid, reinput again!\n");
            continue;
        }
    } while (0);
    return nHWDeviceType;
}

static const char *const hw_type_names[] = {
    [AV_HWDEVICE_TYPE_CUDA] = "cuda",
    [AV_HWDEVICE_TYPE_DRM] = "drm",
    [AV_HWDEVICE_TYPE_DXVA2] = "dxva2",
    [AV_HWDEVICE_TYPE_D3D11VA] = "d3d11va",
    [AV_HWDEVICE_TYPE_OPENCL] = "opencl",
    [AV_HWDEVICE_TYPE_QSV] = "qsv",
    [AV_HWDEVICE_TYPE_VAAPI] = "vaapi",
    [AV_HWDEVICE_TYPE_VDPAU] = "vdpau",
    [AV_HWDEVICE_TYPE_VIDEOTOOLBOX] = "videotoolbox",
    [AV_HWDEVICE_TYPE_MEDIACODEC] = "mediacodec",
};

int main(int argc, char *argv[])
{
    AVFormatContext *input_ctx = NULL;
    int video_stream, ret;
    AVStream *video = NULL;
    AVCodecContext *decoder_ctx = NULL;
    AVCodec *decoder = NULL;
    AVPacket packet;
    enum AVHWDeviceType type;
    int i;

    LARGE_INTEGER nFreq;
    LARGE_INTEGER nBeginTime;
    LARGE_INTEGER nEndTime;
    double time;
    QueryPerformanceFrequency(&nFreq);
    char outputFile[_MAX_PATH];
    char inputFile[_MAX_PATH];

    do
    {
        /* open the input file */
        fprintf(stderr, "Input file:\n");
        scanf_s("%s", &inputFile, _MAX_PATH);
        if (avformat_open_input(&input_ctx, inputFile, NULL, NULL) != 0) {
            fprintf(stderr, "Cannot open input file '%s'\n", inputFile);
            continue;
        }
        avformat_close_input(&input_ctx);
        fprintf(stderr, "only deocde?(y/n):\n");
        char onlydecode[16] = { 0 };
        scanf_s("%s", &onlydecode, 16);
        if (onlydecode[0] == 'n')
        {
            bOnlyDecode = FALSE;
        }
        if (bOnlyDecode)
            break;
        fprintf(stderr, "input output file:\n");
        scanf_s("%s", &outputFile, _MAX_PATH);

    } while (0);

    type = AV_HWDEVICE_TYPE_NONE;
    while (!bHWDecode || (type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
    {
        nFrameDecode = 0;
     /*   int nHWDeviceType = InputHWDeviceType();
        if (nHWDeviceType == 0)
        {
            ;
        }
        else
        {
            type = av_hwdevice_find_type_by_name(hw_type_names[nHWDeviceType]);
            if (type == AV_HWDEVICE_TYPE_NONE) {
                fprintf(stderr, "Device type %s is not supported.\n", hw_type_names[nHWDeviceType]);
                fprintf(stderr, "Available device types:");
                while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
                    fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
                fprintf(stderr, "\n");
                continue;
            }
        }
        */
        avformat_open_input(&input_ctx, inputFile, NULL, NULL);
        if (avformat_find_stream_info(input_ctx, NULL) < 0) {
            fprintf(stderr, "Cannot find input stream information.\n");
            continue;
        }

        /* find the video stream information */
        ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
        if (ret < 0) {
            fprintf(stderr, "Cannot find a video stream in the input file\n");
            continue;
        }

        video_stream = ret;

        if (bHWDecode)
        {
            for (i = 0;; i++) {
                const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
                if (!config) {
                    fprintf(stderr, "Decoder %s does not support device type %s.\n",
                        decoder->name, av_hwdevice_get_type_name(type));
                    goto end;
                }
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    config->device_type == type) {
                    hw_pix_fmt = config->pix_fmt;
                    break;
                }
            }
        }
        else
        {
        }

        if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
        {
            return AVERROR(ENOMEM);
        }
        video = input_ctx->streams[video_stream];
        if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
            goto end;

        
        if (bHWDecode) 
        {
            decoder_ctx->get_format = get_hw_format;
            if (hw_decoder_init(decoder_ctx, type) < 0)
                goto end;
        }
        else
        {
            // cpu解码，设置解码线程数;
            // 一般情况下，视频帧为单slice;
            decoder_ctx->thread_count = 8;
            decoder_ctx->thread_type = FF_THREAD_FRAME;
        }

        if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) {
            fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
            goto end;
        }

        /* open the file to dump raw data */
        fopen_s(&output_file, outputFile, "w+");
        //    output_file = fopen(argv[3], "w+");

        QueryPerformanceCounter(&nBeginTime);
        
        // 分离器
        // H.264码流的sps/pps信息存储在AVCodecContext结构体的extradata中
        // "h264_mp4toannexb",bitstream filter处理
        // AVPacket中的数据变化：
        // 1.每个AVPacket的data添加NALU起始码{0,0,0,1}
        // 2.每个IDR帧数据前面添加了sps/pps
        AVBitStreamFilter *absFilter = NULL;
        AVBSFContext *absCtx = NULL;
        absFilter = av_bsf_get_by_name("h264_mp4toannexb");
        av_bsf_alloc(absFilter, &absCtx);
        avcodec_parameters_copy(absCtx->par_in, video->codecpar);
        av_bsf_init(absCtx);

        /* actual decoding and dump the raw data */
        while (ret >= 0) {
            if ((ret = av_read_frame(input_ctx, &packet)) < 0)
                break;

            if (video_stream == packet.stream_index)
            {
                int len = packet.data[3] + (packet.data[2] << 8) + (packet.data[1] << 16) + (packet.data[0] << 24);
                int naltype = packet.data[4];
                int readLen = packet.size; // len + 4
                //nFrameDecode++;
                if (av_bsf_send_packet(absCtx, &packet) != 0)
                {
                    av_packet_unref(&packet);
                    continue;
                }
                while (av_bsf_receive_packet(absCtx, &packet) == 0)
                {
                    ret = decode_write(decoder_ctx, &packet);
                    fprintf(stderr, "read frame len %d / %d, naltype %02x, bsf packet size %d, naltype %02x\n", readLen, len, naltype, packet.size, packet.data[4]);
                }
                if (nFrameDecode > 10000)
                    break;
            }

            av_packet_unref(&packet);
        }
        av_bsf_free(&absCtx);

        /* flush the decoder */
        packet.data = NULL;
        packet.size = 0;
        fprintf(stderr, "no frame input\n");
        ret = decode_write(decoder_ctx, &packet);
        av_packet_unref(&packet);
        QueryPerformanceCounter(&nEndTime);
        time = (double)(nEndTime.QuadPart - nBeginTime.QuadPart) / (double)nFreq.QuadPart;

        fprintf(stderr, "decode device %s >>> decode %d frame cost %f s\n", bHWDecode ? av_hwdevice_get_type_name(type) :"CPU", nFrameDecode, time);
        if (output_file)
            fclose(output_file);
    end:
        bHWDecode = TRUE;
        avcodec_free_context(&decoder_ctx);
        avformat_close_input(&input_ctx);
        av_buffer_unref(&hw_device_ctx);
    }
    int c;
    fprintf(stderr, "enter q to exit\n");
    while ((c = getchar()) != 'q')
    {
    }
    return 0;
}
