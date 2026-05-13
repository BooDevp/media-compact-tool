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

static size_t obtener_tamano(const char *ruta)
{
    struct stat st;
    if (stat(ruta, &st) == 0)
        return (size_t)st.st_size;
    return 0;
}

static const char *extraer_nombre(const char *ruta)
{
    const char *sep = strrchr(ruta, '/');
    if (sep)
        return sep + 1;
    sep = strrchr(ruta, '\\');
    return sep ? sep + 1 : ruta;
}

static void copiar_archivo(const char *src, const char *dst)
{
    FILE *f_src = fopen(src, "rb");
    if (!f_src)
        return;
    FILE *f_dst = fopen(dst, "wb");
    if (!f_dst)
    {
        fclose(f_src);
        return;
    }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f_src)) > 0)
        fwrite(buf, 1, n, f_dst);
    fclose(f_dst);
    fclose(f_src);
}

static int abrir_y_validar(const char *ruta_in, AVFormatContext **ifmt,
                           int *es_hevc, int *idx_cover)
{
    *es_hevc = 0;
    *idx_cover = -1;

    if (avformat_open_input(ifmt, ruta_in, NULL, NULL) < 0)
    {
        log_printf("  VIDEO: error al abrir %s", ruta_in);
        return 0;
    }
    avformat_find_stream_info(*ifmt, NULL);

    AVDictionaryEntry *tag = av_dict_get((*ifmt)->metadata, "comment", NULL, 0);
    if (tag && strcmp(tag->value, FIRMA_OPTIMIZADO) == 0)
    {
        log_printf("  VIDEO: saltado (ya contiene FIRMA)");
        return 1;
    }

    int tiene_video = 0;
    for (int i = 0; i < (*ifmt)->nb_streams; i++)
    {
        AVStream *s = (*ifmt)->streams[i];
        if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (s->disposition & AV_DISPOSITION_ATTACHED_PIC)
            {
                *idx_cover = i;
                continue;
            }
            tiene_video = 1;
            *es_hevc = (s->codecpar->codec_id == AV_CODEC_ID_HEVC);
        }
    }

    if (!tiene_video)
    {
        log_printf("  VIDEO: no se encontraron streams de video");
        return 0;
    }

    return 2;
}

static void log_streams(AVFormatContext *ifmt)
{
    log_printf("  VIDEO: %d streams duracion=%.1fs", ifmt->nb_streams,
               ifmt->duration > 0 ? ifmt->duration / (double)AV_TIME_BASE : 0.0);
    for (int i = 0; i < ifmt->nb_streams; i++)
    {
        AVStream *s = ifmt->streams[i];
        if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (s->disposition & AV_DISPOSITION_ATTACHED_PIC)
            {
                log_printf("  VIDEO: [%d] portada %s", i,
                           avcodec_get_name(s->codecpar->codec_id));
            }
            else
            {
                int hevc = (s->codecpar->codec_id == AV_CODEC_ID_HEVC);
                log_printf("  VIDEO: [%d] %s %dx%d%s", i,
                           avcodec_get_name(s->codecpar->codec_id),
                           s->codecpar->width, s->codecpar->height,
                           hevc ? " HEVC" : "");
            }
        }
        else
        {
            log_printf("  VIDEO: [%d] %s", i,
                       avcodec_get_name(s->codecpar->codec_id));
        }
    }
}

static int configurar_codificacion(AVFormatContext *ifmt, AVFormatContext *ofmt,
                                   AVStream *in_s, AVStream *out_s,
                                   AVCodecContext **dec, AVCodecContext **enc)
{
    const AVCodec *d = avcodec_find_decoder(in_s->codecpar->codec_id);
    *dec = avcodec_alloc_context3(d);
    avcodec_parameters_to_context(*dec, in_s->codecpar);
    avcodec_open2(*dec, d, NULL);

    AVRational fps = in_s->avg_frame_rate;
    if (fps.num <= 0 || fps.den <= 0)
    {
        fps = av_guess_frame_rate(ifmt, in_s, NULL);
        if (fps.num <= 0 || fps.den <= 0)
            fps = (AVRational){30, 1};
    }

    const AVCodec *e = avcodec_find_encoder(AV_CODEC_ID_HEVC);
    *enc = avcodec_alloc_context3(e);
    (*enc)->height = (*dec)->height;
    (*enc)->width = (*dec)->width;
    (*enc)->pix_fmt = AV_PIX_FMT_YUV420P;
    (*enc)->time_base = av_inv_q(fps);

    if (ofmt->oformat->flags & AVFMT_GLOBALHEADER)
        (*enc)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_opt_set((*enc)->priv_data, "crf", VID_CRF, 0);
    av_opt_set((*enc)->priv_data, "preset", VID_PRESET, 0);
    avcodec_open2(*enc, e, NULL);

    avcodec_parameters_from_context(out_s->codecpar, *enc);
    out_s->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');
    out_s->time_base = (*enc)->time_base;

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

    log_printf("  VIDEO: [%d] HEVC %dfps crf=%s preset=%s rot=%d",
               in_s->index, (int)(av_q2d(fps) + 0.5), VID_CRF, VID_PRESET, n_sd);
    return 1;
}

static void copiar_portada(AVFormatContext *ifmt, AVFormatContext *ofmt, int idx_cover)
{
    if (idx_cover < 0)
        return;
    AVStream *in_s = ifmt->streams[idx_cover];
    AVStream *out_s = ofmt->streams[idx_cover];
    if (in_s->attached_pic.data)
        av_packet_ref(&out_s->attached_pic, &in_s->attached_pic);
}

static int bucle_principal(AVFormatContext *ifmt, AVFormatContext *ofmt,
                           AVCodecContext *dec, AVCodecContext *enc,
                           int v_idx, int es_hevc, const char *nombre_archivo)
{
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
        if (!es_hevc && pkt->stream_index == v_idx)
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
            if (pct > 1.0)
                pct = 1.0;
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

    av_frame_free(&frm);
    av_packet_free(&pkt);
    return 1;
}

int compactar_video(const char *ruta_in, const char *ruta_out)
{
    log_printf(">>> VIDEO: entrada=%s", ruta_in);

    size_t tam_original = obtener_tamano(ruta_in);
    const char *nombre_archivo = extraer_nombre(ruta_in);
    log_printf("  VIDEO: %zuKB %s", tam_original / 1024, nombre_archivo);

    av_log_set_level(AV_LOG_QUIET);
    AVFormatContext *ifmt = NULL, *ofmt = NULL;
    AVCodecContext *dec = NULL, *enc = NULL;
    int v_idx = -1, es_hevc_input = 0, idx_cover_in = -1, success = 0;

    int estado = abrir_y_validar(ruta_in, &ifmt, &es_hevc_input, &idx_cover_in);
    if (estado == 1)
    {
        avformat_close_input(&ifmt);
        return 2;
    }
    if (estado == 0)
        return 0;

    log_streams(ifmt);

    avformat_alloc_output_context2(&ofmt, NULL, NULL, ruta_out);

    for (int i = 0; i < ifmt->nb_streams; i++)
    {
        AVStream *in_s = ifmt->streams[i];
        AVStream *out_s = avformat_new_stream(ofmt, NULL);
        out_s->disposition = in_s->disposition;

        if (!es_hevc_input && in_s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !(in_s->disposition & AV_DISPOSITION_ATTACHED_PIC) && v_idx < 0)
        {
            v_idx = i;
            configurar_codificacion(ifmt, ofmt, in_s, out_s, &dec, &enc);
        }
        else
        {
            avcodec_parameters_copy(out_s->codecpar, in_s->codecpar);
            out_s->codecpar->codec_tag = 0;
            out_s->time_base = in_s->time_base;
        }
    }

    copiar_portada(ifmt, ofmt, idx_cover_in);

    if (!(ofmt->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&ofmt->pb, ruta_out, AVIO_FLAG_WRITE) < 0)
            log_printf("  VIDEO: ERROR avio_open fallo");
    }

    av_dict_set(&ofmt->metadata, "comment", FIRMA_OPTIMIZADO, 0);

    AVDictionary *opts = NULL;
    av_dict_set(&opts, "movflags", "faststart", 0);

    int ret_hdr = avformat_write_header(ofmt, &opts);
    if (ret_hdr >= 0)
    {
        av_dict_free(&opts);
        log_printf("  VIDEO: write_header OK");

        bucle_principal(ifmt, ofmt, dec, enc, v_idx, es_hevc_input, nombre_archivo);

        av_write_trailer(ofmt);
        success = 1;
    }
    else
    {
        log_printf("  VIDEO: ERROR avformat_write_header ret=%d", ret_hdr);
        av_dict_free(&opts);
    }

    avcodec_free_context(&dec);
    avcodec_free_context(&enc);
    if (!(ofmt->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt->pb);
    avformat_free_context(ofmt);
    avformat_close_input(&ifmt);

    if (success)
    {
        size_t tam_comprimido = obtener_tamano(ruta_out);
        if (tam_original > 0 && tam_comprimido >= tam_original)
        {
            log_printf("  VIDEO: resultado %zuKB >= original %zuKB, se conserva el original",
                       tam_comprimido / 1024, tam_original / 1024);
            copiar_archivo(ruta_in, ruta_out);
        }
        else
        {
            log_printf("  VIDEO: %zuKB -> %zuKB (%.1f%%)",
                       tam_original / 1024, tam_comprimido / 1024,
                       tam_original > 0
                           ? (1.0 - (double)tam_comprimido / tam_original) * 100.0
                           : 0.0);
        }
    }

    log_printf("  VIDEO: ret=%d", success);
    return success;
}
