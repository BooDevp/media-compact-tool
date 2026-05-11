#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <config.h>
#include <progreso.h>
#include <log.h>

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

static AVPacket *extraer_thumbnail(const char *ruta_in, int *ancho, int *alto)
{
    log_printf("  [THUMB] abriendo %s", ruta_in);
    AVFormatContext *ifmt = NULL;
    if (avformat_open_input(&ifmt, ruta_in, NULL, NULL) < 0)
    {
        log_printf("  [THUMB] ERROR avformat_open_input");
        return NULL;
    }
    avformat_find_stream_info(ifmt, NULL);

    int v_i = -1;
    for (unsigned i = 0; i < ifmt->nb_streams; i++)
    {
        if (ifmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
            && !(ifmt->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC))
        {
            v_i = (int)i;
            break;
        }
    }
    if (v_i < 0)
    {
        log_printf("  [THUMB] ERROR no se encontro stream de video");
        avformat_close_input(&ifmt);
        return NULL;
    }
    log_printf("  [THUMB] stream[%d] %s", v_i,
               avcodec_get_name(ifmt->streams[v_i]->codecpar->codec_id));

    const AVCodec *d = avcodec_find_decoder(ifmt->streams[v_i]->codecpar->codec_id);
    if (!d)
    {
        log_printf("  [THUMB] ERROR decoder no encontrado");
        avformat_close_input(&ifmt);
        return NULL;
    }

    AVCodecContext *dec = avcodec_alloc_context3(d);
    avcodec_parameters_to_context(dec, ifmt->streams[v_i]->codecpar);
    if (avcodec_open2(dec, d, NULL) < 0)
    {
        log_printf("  [THUMB] ERROR avcodec_open2 decoder");
        avcodec_free_context(&dec);
        avformat_close_input(&ifmt);
        return NULL;
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frm = av_frame_alloc();
    AVFrame *thumb = NULL;
    AVPacket *jpkt = NULL;
    int frame_obtenido = 0;

    while (av_read_frame(ifmt, pkt) >= 0)
    {
        if (pkt->stream_index == v_i)
        {
            if (avcodec_send_packet(dec, pkt) >= 0)
            {
                if (avcodec_receive_frame(dec, frm) >= 0)
                {
                    frame_obtenido = 1;
                    log_printf("  [THUMB] frame %dx%d", frm->width, frm->height);

                    int tw = 320;
                    int th = (int)(320.0 * frm->height / frm->width);

                    thumb = av_frame_alloc();
                    thumb->format = AV_PIX_FMT_YUV420P;
                    thumb->width = tw;
                    thumb->height = th;
                    if (av_image_alloc(thumb->data, thumb->linesize, tw, th,
                                       AV_PIX_FMT_YUV420P, 1) < 0)
                    {
                        log_printf("  [THUMB] ERROR av_image_alloc");
                        break;
                    }

                    struct SwsContext *sws = sws_getContext(
                        frm->width, frm->height, frm->format,
                        tw, th, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
                    if (sws)
                    {
                        sws_scale(sws, (const uint8_t **)frm->data, frm->linesize,
                                  0, frm->height, thumb->data, thumb->linesize);
                        sws_freeContext(sws);
                    }
                    else
                    {
                        log_printf("  [THUMB] ERROR sws_getContext NULL");
                    }

                    const AVCodec *mjpeg_enc = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
                    if (!mjpeg_enc)
                    {
                        log_printf("  [THUMB] ERROR encoder MJPEG no encontrado");
                        break;
                    }

                    AVCodecContext *enc_j = avcodec_alloc_context3(mjpeg_enc);
                    enc_j->width = tw;
                    enc_j->height = th;
                    enc_j->pix_fmt = AV_PIX_FMT_YUV420P;
                    enc_j->color_range = AVCOL_RANGE_JPEG;
                    enc_j->time_base = (AVRational){1, 1};
                    enc_j->compression_level = 5;

                    if (avcodec_open2(enc_j, NULL, NULL) >= 0)
                    {
                        jpkt = av_packet_alloc();
                        if (avcodec_send_frame(enc_j, thumb) >= 0)
                        {
                            if (avcodec_receive_packet(enc_j, jpkt) < 0)
                            {
                                av_packet_free(&jpkt);
                                jpkt = NULL;
                            }
                        }
                        avcodec_free_context(&enc_j);
                    }
                    else
                    {
                        log_printf("  [THUMB] ERROR avcodec_open2 MJPEG");
                    }

                    *ancho = tw;
                    *alto = th;
                    break;
                }
            }
        }
        av_packet_unref(pkt);
    }

    log_printf("  [THUMB] %s -> %s",
               frame_obtenido ? "OK" : "sin_frame",
               jpkt ? "JPEG_OK" : "JPEG_NULL");

    if (thumb)
    {
        av_freep(&thumb->data[0]);
        av_frame_free(&thumb);
    }
    av_frame_free(&frm);
    av_packet_free(&pkt);
    avcodec_free_context(&dec);
    avformat_close_input(&ifmt);
    return jpkt;
}

int compactar_video(const char *ruta_in, const char *ruta_out)
{
    log_printf(">>> VIDEO: entrada=%s", ruta_in);

    struct stat st_orig;
    size_t tam_original = 0;
    if (stat(ruta_in, &st_orig) == 0)
        tam_original = (size_t)st_orig.st_size;
    const char *nombre_archivo = strrchr(ruta_in, '/')
        ? strrchr(ruta_in, '/') + 1
        : (strrchr(ruta_in, '\\') ? strrchr(ruta_in, '\\') + 1 : ruta_in);
    log_printf("  VIDEO: %zuKB %s", tam_original / 1024, nombre_archivo);

    av_log_set_level(AV_LOG_QUIET);
    AVFormatContext *ifmt = NULL, *ofmt = NULL;
    AVCodecContext *dec = NULL, *enc = NULL;
    int v_idx = -1, success = 0, es_hevc_input = 0;
    int idx_cover_in = -1;

    if (avformat_open_input(&ifmt, ruta_in, NULL, NULL) < 0)
    {
        log_printf("  VIDEO: error al abrir %s", ruta_in);
        return 0;
    }
    avformat_find_stream_info(ifmt, NULL);
    log_printf("  VIDEO: %d streams duracion=%.1fs", ifmt->nb_streams,
               ifmt->duration > 0 ? ifmt->duration / (double)AV_TIME_BASE : 0.0);

    AVDictionaryEntry *tag = av_dict_get(ifmt->metadata, "comment", NULL, 0);
    if (tag && strcmp(tag->value, FIRMA_OPTIMIZADO) == 0)
    {
        log_printf("  VIDEO: saltado (ya contiene FIRMA)");
        avformat_close_input(&ifmt);
        return 2;
    }

    int tiene_video = 0;
    for (int i = 0; i < ifmt->nb_streams; i++)
    {
        AVStream *s = ifmt->streams[i];
        if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (s->disposition & AV_DISPOSITION_ATTACHED_PIC)
            {
                log_printf("  VIDEO: [%d] portada %s", i,
                           avcodec_get_name(s->codecpar->codec_id));
                idx_cover_in = i;
                continue;
            }
            tiene_video = 1;
            es_hevc_input = (s->codecpar->codec_id == AV_CODEC_ID_HEVC);
            log_printf("  VIDEO: [%d] %s %dx%d%s", i,
                       avcodec_get_name(s->codecpar->codec_id),
                       s->codecpar->width, s->codecpar->height,
                       es_hevc_input ? " HEVC" : "");
        }
        else
        {
            log_printf("  VIDEO: [%d] %s", i,
                       avcodec_get_name(s->codecpar->codec_id));
        }
    }

    if (!tiene_video)
    {
        log_printf("  VIDEO: no se encontraron streams de video");
        avformat_close_input(&ifmt);
        return 0;
    }

    AVPacket *thumbnail_pkt = NULL;
    int thumb_w = 0, thumb_h = 0;
    if (idx_cover_in < 0 && !es_hevc_input)
    {
        thumbnail_pkt = extraer_thumbnail(ruta_in, &thumb_w, &thumb_h);
        if (thumbnail_pkt)
            log_printf("  VIDEO: thumbnail %dx%d %d bytes", thumb_w, thumb_h,
                       thumbnail_pkt->size);
        else
            log_printf("  VIDEO: thumbnail no generado");
    }

    avformat_alloc_output_context2(&ofmt, NULL, NULL, ruta_out);

    for (int i = 0; i < ifmt->nb_streams; i++)
    {
        AVStream *in_s = ifmt->streams[i];
        AVStream *out_s = avformat_new_stream(ofmt, NULL);
        out_s->disposition = in_s->disposition;

        if (!es_hevc_input && in_s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
            && !(in_s->disposition & AV_DISPOSITION_ATTACHED_PIC) && v_idx < 0)
        {
            v_idx = i;

            const AVCodec *d = avcodec_find_decoder(in_s->codecpar->codec_id);
            dec = avcodec_alloc_context3(d);
            avcodec_parameters_to_context(dec, in_s->codecpar);
            avcodec_open2(dec, d, NULL);

            AVRational fps = in_s->avg_frame_rate;
            if (fps.num <= 0 || fps.den <= 0)
            {
                fps = av_guess_frame_rate(ifmt, in_s, NULL);
                if (fps.num <= 0 || fps.den <= 0)
                    fps = (AVRational){30, 1};
            }

            const AVCodec *e = avcodec_find_encoder(AV_CODEC_ID_HEVC);
            enc = avcodec_alloc_context3(e);
            enc->height = dec->height;
            enc->width = dec->width;
            enc->pix_fmt = AV_PIX_FMT_YUV420P;
            enc->time_base = av_inv_q(fps);

            if (ofmt->oformat->flags & AVFMT_GLOBALHEADER)
                enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            av_opt_set(enc->priv_data, "crf", VID_CRF, 0);
            av_opt_set(enc->priv_data, "preset", VID_PRESET, 0);
            avcodec_open2(enc, e, NULL);

            avcodec_parameters_from_context(out_s->codecpar, enc);
            out_s->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');
            out_s->time_base = enc->time_base;

            int n_sd = in_s->codecpar->nb_coded_side_data;
            for (int j = 0; j < n_sd; j++)
            {
                const AVPacketSideData *sd_src = &in_s->codecpar->coded_side_data[j];
                AVPacketSideData *sd_dst = av_packet_side_data_new(
                    &out_s->codecpar->coded_side_data,
                    &out_s->codecpar->nb_coded_side_data,
                    sd_src->type, sd_src->size, 0);
                if (sd_dst)
                    memcpy(sd_dst->data, sd_src->data, sd_src->size);
            }

            log_printf("  VIDEO: [%d] HEVC %dfps crf=%s preset=%s rot=%d", i,
                       (int)(av_q2d(fps) + 0.5), VID_CRF, VID_PRESET, n_sd);
        }
        else
        {
            avcodec_parameters_copy(out_s->codecpar, in_s->codecpar);
            out_s->codecpar->codec_tag = 0;
            out_s->time_base = in_s->time_base;
        }
    }

    if (thumbnail_pkt)
    {
        AVStream *cover_s = avformat_new_stream(ofmt, NULL);
        if (cover_s)
        {
            cover_s->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            cover_s->codecpar->codec_id = AV_CODEC_ID_MJPEG;
            cover_s->codecpar->width = thumb_w;
            cover_s->codecpar->height = thumb_h;
            cover_s->codecpar->format = AV_PIX_FMT_YUV420P;
            cover_s->disposition |= AV_DISPOSITION_ATTACHED_PIC;
            av_packet_ref(&cover_s->attached_pic, thumbnail_pkt);
            cover_s->attached_pic.stream_index = cover_s->index;
        }
    }

    if (idx_cover_in >= 0)
    {
        AVStream *in_s = ifmt->streams[idx_cover_in];
        AVStream *out_s = ofmt->streams[idx_cover_in];
        if (in_s->attached_pic.data)
            av_packet_ref(&out_s->attached_pic, &in_s->attached_pic);
    }

    if (!(ofmt->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&ofmt->pb, ruta_out, AVIO_FLAG_WRITE) < 0)
        {
            log_printf("  VIDEO: ERROR avio_open fallo");
        }
    }

    av_dict_set(&ofmt->metadata, "comment", FIRMA_OPTIMIZADO, 0);

    AVDictionary *opts = NULL;
    av_dict_set(&opts, "movflags", "faststart", 0);

    int ret_hdr = avformat_write_header(ofmt, &opts);
    if (ret_hdr >= 0)
    {
        av_dict_free(&opts);

        if (thumbnail_pkt)
            log_printf("  VIDEO: write_header OK + thumbnail %d bytes", thumbnail_pkt->size);
        else if (idx_cover_in >= 0)
            log_printf("  VIDEO: write_header OK + portada original");
        else
            log_printf("  VIDEO: write_header OK sin portada");

        AVPacket *pkt = av_packet_alloc();
        AVFrame *frm = av_frame_alloc();
        int64_t pts_c = 0;
        int64_t video_packets = 0;
        int64_t duracion_total = ifmt->duration;

        int64_t total_frames_est = 0;
        if (duracion_total > 0 && v_idx >= 0)
        {
            AVRational fps = av_guess_frame_rate(ifmt, ifmt->streams[v_idx], NULL);
            if (fps.num > 0 && fps.den > 0)
                total_frames_est = (int64_t)((double)duracion_total / AV_TIME_BASE * av_q2d(fps));
        }

        while (av_read_frame(ifmt, pkt) >= 0)
        {
            if (!es_hevc_input && pkt->stream_index == v_idx)
            {
                video_packets++;

                double pct = 0.0;
                if (total_frames_est > 0)
                    pct = (double)video_packets / total_frames_est;
                else if (duracion_total > 0 && pkt->pts != AV_NOPTS_VALUE)
                {
                    int64_t ts = av_rescale_q(pkt->pts,
                                              ifmt->streams[v_idx]->time_base,
                                              (AVRational){1, AV_TIME_BASE});
                    pct = (double)ts / duracion_total;
                }
                if (pct > 1.0) pct = 1.0;
                progreso_barra("VIDEO", nombre_archivo, pct);

                if (avcodec_send_packet(dec, pkt) >= 0)
                {
                    while (avcodec_receive_frame(dec, frm) >= 0)
                    {
                        frm->pts = pts_c++;
                        escribir_frame(frm, enc, ofmt->streams[v_idx], ofmt);
                        av_frame_unref(frm);
                    }
                }
            }
            else
            {
                av_packet_rescale_ts(pkt,
                    ifmt->streams[pkt->stream_index]->time_base,
                    ofmt->streams[pkt->stream_index]->time_base);
                av_interleaved_write_frame(ofmt, pkt);
            }
            av_packet_unref(pkt);
        }
        log_printf("  VIDEO: bucle terminado %lld packets", (long long)video_packets);

        progreso_barra("VIDEO", nombre_archivo, 1.0);
        if (enc)
            escribir_frame(NULL, enc, ofmt->streams[v_idx], ofmt);

        av_write_trailer(ofmt);
        av_frame_free(&frm);
        av_packet_free(&pkt);
        success = 1;
    }
    else
    {
        log_printf("  VIDEO: ERROR avformat_write_header ret=%d", ret_hdr);
        av_dict_free(&opts);
    }

    av_packet_free(&thumbnail_pkt);
    avcodec_free_context(&dec);
    avcodec_free_context(&enc);
    if (!(ofmt->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt->pb);
    avformat_free_context(ofmt);
    avformat_close_input(&ifmt);

    if (success)
    {
        struct stat st_out;
        size_t tam_comprimido = (stat(ruta_out, &st_out) == 0) ? (size_t)st_out.st_size : 0;
        log_printf("  VIDEO: %zuKB -> %zuKB (%.1f%%)",
                   tam_original / 1024, tam_comprimido / 1024,
                   tam_original > 0
                       ? (1.0 - (double)tam_comprimido / tam_original) * 100.0
                       : 0.0);
    }

    log_printf("  VIDEO: ret=%d", success);
    return success;
}
