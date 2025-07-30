#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_FRAMES 1000
#define MAX_MVS_PER_FRAME 100000   // allow up to 100k per frame for dense videos

typedef struct {
    int frame;
    int mb_x, mb_y;
    int mvd_x, mvd_y;
} MVEntry;

MVEntry *frames[MAX_FRAMES];
int mv_counts[MAX_FRAMES];

static const uint8_t color[3] = {255, 0, 0}; // Default red; update as needed

int parse_csv(const char *filename, int method_id_only) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Failed to open CSV file: %s\n", filename);
        return -1;
    }
    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }

    int max_frame = 0;
    while (fgets(line, sizeof(line), f)) {
        int fr, method, mb_x, mb_y, mvd_x, mvd_y;
        if (sscanf(line, "%d,%d,%d,%d,%d,%d",
                   &fr, &method, &mb_x, &mb_y, &mvd_x, &mvd_y) == 6) {
            if (method != method_id_only) continue;
            if (fr < 0 || fr >= MAX_FRAMES) continue;
            int count = mv_counts[fr];
            if (count >= MAX_MVS_PER_FRAME) continue;
            if (!frames[fr]) {
                frames[fr] = malloc(sizeof(MVEntry) * MAX_MVS_PER_FRAME);
                if (!frames[fr]) { fclose(f); return -1; }
            }
            MVEntry e = {fr, mb_x, mb_y, mvd_x, mvd_y};
            frames[fr][count] = e;
            mv_counts[fr]++;
            if (fr > max_frame) max_frame = fr;
        }
    }
    fclose(f);
    return max_frame + 1;
}

void draw_line_rgb(uint8_t *buf, int width, int height, int x0, int y0, int x1, int y1,
                   uint8_t r, uint8_t g, uint8_t b)
{
    int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1, err = dx + dy;
    while (1) {
        if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
            int idx = (y0 * width + x0) * 3;
            buf[idx] = r; buf[idx+1] = g; buf[idx+2] = b;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_motions_fullframe(uint8_t *buf, int width, int height, MVEntry *mvs, int count) {
    // Automatically detect macroblock grid
    int max_mb_x = 0, max_mb_y = 0;
    for (int i = 0; i < count; i++) {
        if (mvs[i].mb_x > max_mb_x) max_mb_x = mvs[i].mb_x;
        if (mvs[i].mb_y > max_mb_y) max_mb_y = mvs[i].mb_y;
    }
    max_mb_x++; max_mb_y++;

    for (int i = 0; i < count; i++) {
        int x0 = (mvs[i].mb_x * width) / max_mb_x;
        int y0 = (mvs[i].mb_y * height) / max_mb_y;
        int x1 = x0 + mvs[i].mvd_x;
        int y1 = y0 + mvs[i].mvd_y;
        draw_line_rgb(buf, width, height, x0, y0, x1, y1, color[0], color[1], color[2]);
    }
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <ref_video> <csv> <output.mp4> <method_id>\n", argv[0]);
        return 1;
    }
    const char *ref_video = argv[1], *csv = argv[2], *out_file = argv[3];
    int method_id = atoi(argv[4]);

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, ref_video, NULL, NULL) != 0) {
        fprintf(stderr, "Failed to open video %s\n", ref_video); return 1;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        avformat_close_input(&fmt_ctx); return 1;
    }
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index < 0) { avformat_close_input(&fmt_ctx); return 1; }
    AVStream *vs = fmt_ctx->streams[video_index];
    int width = vs->codecpar->width, height = vs->codecpar->height;

    int num_frames = parse_csv(csv, method_id);
    if (num_frames <= 0) { avformat_close_input(&fmt_ctx); return 1; }

    AVFormatContext *out_fmt = NULL;
    avformat_alloc_output_context2(&out_fmt, NULL, NULL, out_file);
    if (!out_fmt) { avformat_close_input(&fmt_ctx); return 1; }

    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVStream *out_stream = avformat_new_stream(out_fmt, codec);
    AVCodecContext *enc_ctx = avcodec_alloc_context3(codec);
    enc_ctx->width = width; enc_ctx->height = height;
    enc_ctx->time_base = vs->time_base;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    out_stream->time_base = enc_ctx->time_base;
    if (out_fmt->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(enc_ctx, codec, NULL) < 0) goto fail;
    if (avcodec_parameters_from_context(out_stream->codecpar, enc_ctx) < 0) goto fail;
    if (!(out_fmt->oformat->flags & AVFMT_NOFILE))
        if (avio_open(&out_fmt->pb, out_file, AVIO_FLAG_WRITE) < 0) goto fail;
    avformat_write_header(out_fmt, NULL);

    struct SwsContext *sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                                                width, height, AV_PIX_FMT_YUV420P,
                                                SWS_BICUBIC, NULL, NULL, NULL);

    AVFrame *frame_yuv = av_frame_alloc(), *frame_rgb = av_frame_alloc();
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
    uint8_t *rgb_buffer = (uint8_t *)av_malloc(num_bytes);
    av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, rgb_buffer, AV_PIX_FMT_RGB24, width, height, 1);
    frame_yuv->format = AV_PIX_FMT_YUV420P;
    frame_yuv->width = width; frame_yuv->height = height;
    if (av_frame_get_buffer(frame_yuv, 32) < 0) goto fail;

    AVPacket pkt;
    av_init_packet(&pkt);

    for (int f = 0; f < num_frames; f++) {
        memset(rgb_buffer, 255, num_bytes); // white
        if (frames[f]) draw_motions_fullframe(rgb_buffer, width, height, frames[f], mv_counts[f]);
        // RGB -> YUV420P
        sws_scale(sws_ctx, (const uint8_t * const *)frame_rgb->data, frame_rgb->linesize,
                  0, height, frame_yuv->data, frame_yuv->linesize);
        frame_yuv->pts = f;
        if (avcodec_send_frame(enc_ctx, frame_yuv) < 0) break;
        while (avcodec_receive_packet(enc_ctx, &pkt) == 0) {
            av_interleaved_write_frame(out_fmt, &pkt);
            av_packet_unref(&pkt);
        }
    }
    avcodec_send_frame(enc_ctx, NULL);
    while (avcodec_receive_packet(enc_ctx, &pkt) == 0) {
        av_interleaved_write_frame(out_fmt, &pkt);
        av_packet_unref(&pkt);
    }
    av_write_trailer(out_fmt);

fail:
    for (int f = 0; f < MAX_FRAMES; f++) if (frames[f]) free(frames[f]);
    av_free(rgb_buffer); av_frame_free(&frame_rgb); av_frame_free(&frame_yuv);
    sws_freeContext(sws_ctx);
    if (!(out_fmt->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_fmt->pb);
    avcodec_free_context(&enc_ctx);
    avformat_free_context(out_fmt); avformat_close_input(&fmt_ctx);
    return 0;
}

