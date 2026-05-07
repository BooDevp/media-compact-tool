#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "../../include/config.h"
#include "../../include/ui.h"

static int escribir_frame(AVFrame *frame, AVCodecContext *enc, AVStream *st, AVFormatContext *ofmt)
{
    AVPacket *pkt = av_packet_alloc();
    int ret = avcodec_send_frame(enc, frame);
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(enc, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        pkt->stream_index = st->index;
        av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
        av_interleaved_write_frame(ofmt, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    return 0;
}

int compactar_video(const char *ruta_in, const char *ruta_out)
{
    const char *nombre_archivo = strrchr(ruta_in, '/') ? strrchr(ruta_in, '/') + 1 : (strrchr(ruta_in, '\\') ? strrchr(ruta_in, '\\') + 1 : ruta_in);

    av_log_set_level(AV_LOG_QUIET);
    AVFormatContext *ifmt = NULL, *ofmt = NULL;
    AVCodecContext *dec = NULL, *enc = NULL;
    int v_idx = -1, success = 0;

    if (avformat_open_input(&ifmt, ruta_in, NULL, NULL) < 0)
        return 0;
    avformat_find_stream_info(ifmt, NULL);

    AVDictionaryEntry *tag = av_dict_get(ifmt->metadata, "comment", NULL, 0);
    if (tag && strcmp(tag->value, FIRMA_OPTIMIZADO) == 0)
    {
        avformat_close_input(&ifmt);
        return 0;
    }

    avformat_alloc_output_context2(&ofmt, NULL, NULL, ruta_out);
    for (int i = 0; i < ifmt->nb_streams; i++)
    {
        AVStream *in_s = ifmt->streams[i];
        AVStream *out_s = avformat_new_stream(ofmt, NULL);
        if (in_s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && v_idx < 0)
        {
            v_idx = i;
            const AVCodec *d = avcodec_find_decoder(in_s->codecpar->codec_id);
            dec = avcodec_alloc_context3(d);
            avcodec_parameters_to_context(dec, in_s->codecpar);
            avcodec_open2(dec, d, NULL);

            const AVCodec *e = avcodec_find_encoder(AV_CODEC_ID_HEVC);
            enc = avcodec_alloc_context3(e);
            enc->height = dec->height;
            enc->width = dec->width;
            enc->pix_fmt = AV_PIX_FMT_YUV420P;
            enc->time_base = av_inv_q(av_guess_frame_rate(ifmt, in_s, NULL));
            av_opt_set(enc->priv_data, "crf", VID_CRF, 0);
            av_opt_set(enc->priv_data, "preset", VID_PRESET, 0);
            avcodec_open2(enc, e, NULL);
            avcodec_parameters_from_context(out_s->codecpar, enc);
            out_s->time_base = enc->time_base;
        }
        else
        {
            avcodec_parameters_copy(out_s->codecpar, in_s->codecpar);
            out_s->codecpar->codec_tag = 0;
        }
    }

    if (!(ofmt->oformat->flags & AVFMT_NOFILE))
        avio_open(&ofmt->pb, ruta_out, AVIO_FLAG_WRITE);
    av_dict_set(&ofmt->metadata, "comment", FIRMA_OPTIMIZADO, 0);

    if (avformat_write_header(ofmt, NULL) >= 0)
    {
        AVPacket *pkt = av_packet_alloc();
        AVFrame *frm = av_frame_alloc();
        int64_t pts = 0;
        int64_t duracion_total = ifmt->duration;
        
        while (av_read_frame(ifmt, pkt) >= 0)
        {
            if (pkt->stream_index == v_idx)
            {
                // ACTUALIZACIÓN DE BARRA DE PROGRESO
                if (duracion_total > 0 && pkt->pts != AV_NOPTS_VALUE)
                {
                    double seg_actual = pkt->pts * av_q2d(ifmt->streams[v_idx]->time_base);
                    double seg_total = duracion_total / (double)AV_TIME_BASE;
                    double porcentaje = seg_actual / seg_total;
                    if (porcentaje > 1.0)
                        porcentaje = 1.0;
                    
                    ui_barra_progreso("VIDEO", nombre_archivo, porcentaje);
                }

                if (avcodec_send_packet(dec, pkt) >= 0)
                {
                    while (avcodec_receive_frame(dec, frm) >= 0)
                    {
                        frm->pts = pts++;
                        escribir_frame(frm, enc, ofmt->streams[v_idx], ofmt);
                    }
                }
            }
            else
            {
                av_packet_rescale_ts(pkt, ifmt->streams[pkt->stream_index]->time_base, ofmt->streams[pkt->stream_index]->time_base);
                av_interleaved_write_frame(ofmt, pkt);
            }
            av_packet_unref(pkt);
        }

        // Finalizar
        ui_barra_progreso("VIDEO", nombre_archivo, 1.0);

        escribir_frame(NULL, enc, ofmt->streams[v_idx], ofmt);
        av_write_trailer(ofmt);
        av_frame_free(&frm);
        av_packet_free(&pkt);
        success = 1;
    }

    avcodec_free_context(&dec);
    avcodec_free_context(&enc);

    if (!(ofmt->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&ofmt->pb);
    }

    avformat_free_context(ofmt);
    avformat_close_input(&ifmt);
    return success;
}