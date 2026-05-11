#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <vips/vips.h>

#include <config.h>
#include <log.h>

static double ssim_vips(VipsImage *ref, VipsImage *cmp)
{
    if (vips_image_get_width(ref) != vips_image_get_width(cmp) ||
        vips_image_get_height(ref) != vips_image_get_height(cmp)) 
        return 0.0;

    VipsImage *ref_band = ref, *cmp_band = cmp;
    int need_unref_bands = 0;

    if (vips_image_get_bands(ref) > 1)
    {
        need_unref_bands = 1;
        vips_extract_band(ref, &ref_band, 0, NULL);
        vips_extract_band(cmp, &cmp_band, 0, NULL);
    }

    VipsImage *ref_f = NULL, *cmp_f = NULL;
    vips_cast_float(ref_band, &ref_f, NULL);
    vips_cast_float(cmp_band, &cmp_f, NULL);

    double sigma = 1.5;
    double C1 = (0.01 * 255.0) * (0.01 * 255.0);
    double C2 = (0.03 * 255.0) * (0.03 * 255.0);

    VipsImage *mu1 = NULL, *mu2 = NULL;
    vips_gaussblur(ref_f, &mu1, sigma, NULL);
    vips_gaussblur(cmp_f, &mu2, sigma, NULL);

    VipsImage *mu1_sq = NULL, *mu2_sq = NULL, *mu12 = NULL;
    vips_multiply(mu1, mu1, &mu1_sq, NULL);
    vips_multiply(mu2, mu2, &mu2_sq, NULL);
    vips_multiply(mu1, mu2, &mu12, NULL);

    VipsImage *ref_sq = NULL, *blur_ref_sq = NULL, *sigma1_sq = NULL;
    vips_multiply(ref_f, ref_f, &ref_sq, NULL);
    vips_gaussblur(ref_sq, &blur_ref_sq, sigma, NULL);
    vips_subtract(blur_ref_sq, mu1_sq, &sigma1_sq, NULL);

    VipsImage *cmp_sq = NULL, *blur_cmp_sq = NULL, *sigma2_sq = NULL;
    vips_multiply(cmp_f, cmp_f, &cmp_sq, NULL);
    vips_gaussblur(cmp_sq, &blur_cmp_sq, sigma, NULL);
    vips_subtract(blur_cmp_sq, mu2_sq, &sigma2_sq, NULL);

    VipsImage *ref_cmp = NULL, *blur_ref_cmp = NULL, *sigma12 = NULL;
    vips_multiply(ref_f, cmp_f, &ref_cmp, NULL);
    vips_gaussblur(ref_cmp, &blur_ref_cmp, sigma, NULL);
    vips_subtract(blur_ref_cmp, mu12, &sigma12, NULL);

    VipsImage *num1 = NULL, *num2 = NULL, *num = NULL;
    vips_linear1(mu12, &num1, 2.0, C1, NULL);
    vips_linear1(sigma12, &num2, 2.0, C2, NULL);
    vips_multiply(num1, num2, &num, NULL);

    VipsImage *den1 = NULL, *den2 = NULL;
    vips_add(mu1_sq, mu2_sq, &den1, NULL);
    vips_add(sigma1_sq, sigma2_sq, &den2, NULL);
    VipsImage *den1_c1 = NULL, *den2_c2 = NULL, *den = NULL;
    vips_linear1(den1, &den1_c1, 1.0, C1, NULL);
    vips_linear1(den2, &den2_c2, 1.0, C2, NULL);
    vips_multiply(den1_c1, den2_c2, &den, NULL);

    VipsImage *ssim_map = NULL;
    vips_divide(num, den, &ssim_map, NULL);

    double ssim_val;
    vips_avg(ssim_map, &ssim_val, NULL);

    g_object_unref(mu1);
    g_object_unref(mu2);
    g_object_unref(mu1_sq);
    g_object_unref(mu2_sq);
    g_object_unref(mu12);
    g_object_unref(ref_sq);
    g_object_unref(blur_ref_sq);
    g_object_unref(sigma1_sq);
    g_object_unref(cmp_sq);
    g_object_unref(blur_cmp_sq);
    g_object_unref(sigma2_sq);
    g_object_unref(ref_cmp);
    g_object_unref(blur_ref_cmp);
    g_object_unref(sigma12);
    g_object_unref(num1);
    g_object_unref(num2);
    g_object_unref(num);
    g_object_unref(den1);
    g_object_unref(den2);
    g_object_unref(den1_c1);
    g_object_unref(den2_c2);
    g_object_unref(den);
    g_object_unref(ssim_map);
    g_object_unref(ref_f);
    g_object_unref(cmp_f);
    if (need_unref_bands)
    {
        g_object_unref(ref_band);
        g_object_unref(cmp_band);
    }

    return ssim_val;
}

static int guardar_buffer(const char *ruta, const void *buf, size_t len)
{
    FILE *f = fopen(ruta, "wb");
    if (!f) return 0;
    size_t escrito = fwrite(buf, 1, len, f);
    fclose(f);
    return escrito == len;
}

static void copiar_archivo(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) return;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in); fclose(out);
}

int compactar_imagen_adaptativo(const char *ruta_in, const char *ruta_out)
{
    log_printf(">>> PROCESANDO: entrada=%s salida=%s", ruta_in, ruta_out);

    VipsImage *original = vips_image_new_from_file(ruta_in, NULL);
    if (!original)
    {
        log_printf("ERROR: no se pudo cargar %s", ruta_in);
        return 0;
    }

    const char *artist;
    if (vips_image_get_typeof(original, "exif-ifd0-Artist") != 0 &&
        vips_image_get_string(original, "exif-ifd0-Artist", &artist) == 0 &&
        strstr(artist, FIRMA_OPTIMIZADO))
    {
        log_printf("SALTADO: ya contiene FIRMA_OPTIMIZADO (Artist=%s)", artist);
        g_object_unref(original);
        return 2;
    }

    VipsImage *rotada;
    if (vips_autorot(original, &rotada, NULL))
    {
        log_printf("FALLO autorot -> se omite");
        g_object_unref(original);
        return 0;
    }
    g_object_unref(original);

    vips_image_remove(rotada, "exif-data");
    vips_image_remove(rotada, "iptc-data");
    vips_image_remove(rotada, "xmp-data");

    struct stat st;
    size_t tam_original = 0;
    if (stat(ruta_in, &st) == 0)
        tam_original = (size_t)st.st_size;

    int has_alpha = vips_image_hasalpha(rotada);
    log_printf("  tam_original=%zu has_alpha=%d", tam_original, has_alpha);

    if (has_alpha)
    {
        void *buf = NULL;
        size_t len = 0;

        if (vips_pngsave_buffer(rotada, &buf, &len,
                                "palette", TRUE, "colours", PNG_MAX_COLORES, NULL))
        {
            log_printf("  PNG: fallo palette save -> se omite");
            g_object_unref(rotada);
            return 0;
        }

        VipsImage *comprimida = vips_image_new_from_buffer(buf, len, "", NULL);
        if (!comprimida)
        {
            log_printf("  PNG: fallo reload desde buffer -> se omite");
            g_free(buf);
            g_object_unref(rotada);
            return 0;
        }

        double ssim = ssim_vips(rotada, comprimida);
        log_printf("  PNG: palette_len=%zu (%zuKB) SSIM=%.6f umbral=%.2f",
                   len, len / 1024, ssim, SSIM_UMBRAL_PNG);

        int ssim_ok = ssim >= SSIM_UMBRAL_PNG;
        int tam_ok = len < tam_original;

        if (ssim_ok && tam_ok)
        {
            log_printf("  PNG: COMPRIMIDO (SSIM OK %.4f >= %.2f, ahorro %zuKB)",
                       ssim, SSIM_UMBRAL_PNG, (tam_original - len) / 1024);
            vips_image_set_string(rotada, "exif-ifd0-Artist", FIRMA_OPTIMIZADO);
            void *buf_out = NULL;
            size_t len_out = 0;
            if (vips_pngsave_buffer(rotada, &buf_out, &len_out,
                                    "palette", TRUE, "colours", PNG_MAX_COLORES, NULL) == 0)
            {
                guardar_buffer(ruta_out, buf_out, len_out);
                g_free(buf_out);
            }
            g_free(buf);
            g_object_unref(comprimida);
            g_object_unref(rotada);
            return 1;
        }

        g_free(buf);
        g_object_unref(comprimida);

        if (!ssim_ok)
            log_printf("  PNG: Falla por SSIM insuficiente (%.6f < %.2f) | Tamanio %s (%zuKB < %zuKB) -> se omite",
                       ssim, SSIM_UMBRAL_PNG, tam_ok ? "OK" : "insuficiente",
                       len / 1024, tam_original / 1024);
        else
            log_printf("  PNG: SSIM OK (%.4f >= %.2f) | Falla por tamanio (%zuKB >= %zuKB) -> se omite",
                       ssim, SSIM_UMBRAL_PNG, len / 1024, tam_original / 1024);

        g_object_unref(rotada);
        return 0;
    }

    int calidades[] = {IMG_QUALITY_1, IMG_QUALITY_2, IMG_QUALITY_3, IMG_QUALITY_4};
    double umbrales[] = {SSIM_UMBRAL_1, SSIM_UMBRAL_2, SSIM_UMBRAL_3, SSIM_UMBRAL_4};
    double ahorros_min[] = {AHORRO_MINIMO_1, AHORRO_MINIMO_2, AHORRO_MINIMO_3, AHORRO_MINIMO_4};
    int num_intentos = 4;

    for (int intento = 0; intento < num_intentos; intento++)
    {
        int calidad = calidades[intento];
        double umbral = umbrales[intento];
        double min_ahorro = ahorros_min[intento];
        void *buf = NULL;
        size_t len = 0;

        log_printf("  JPEG intento=%d calidad=%d umbral_ssim=%.4f min_ahorro=%.0f%%", intento + 1, calidad, umbral, min_ahorro);

        if (vips_jpegsave_buffer(rotada, &buf, &len,
                                 "Q", calidad, "optimize_coding", TRUE, NULL))
        {
            log_printf("  JPEG: fallo jpegsave_buffer -> sig. intento");
            g_free(buf);
            continue;
        }

        VipsImage *comprimida = vips_image_new_from_buffer(buf, len, "", NULL);
        if (!comprimida)
        {
            log_printf("  JPEG: fallo reload buffer -> sig. intento");
            g_free(buf);
            continue;
        }

        double ssim = ssim_vips(rotada, comprimida);
        g_object_unref(comprimida);

        double ahorro = tam_original > 0
            ? (1.0 - (double)len / (double)tam_original) * 100.0
            : 0.0;

        log_printf("  JPEG: SSIM=%.6f ahorro=%.1f%% len=%zu orig=%zu",
                   ssim, ahorro, len, tam_original);

        int ssim_ok = ssim >= umbral;
        int ultimo = (intento == num_intentos - 1);
        int ahorro_ok = ultimo ? (ahorro > 0.0) : (ahorro >= min_ahorro);

        if (ssim_ok && ahorro_ok)
        {
            log_printf("  JPEG intento%d: COMPRIMIDO Q%d (SSIM OK %.4f >= %.4f, ahorro %.1f%% %s %.0f%%)",
                       intento + 1, calidad, ssim, umbral, ahorro,
                       ultimo ? ">" : ">=", min_ahorro);
            vips_image_set_string(rotada, "exif-ifd0-Artist", FIRMA_OPTIMIZADO);
            void *buf_out = NULL;
            size_t len_out = 0;
            if (vips_jpegsave_buffer(rotada, &buf_out, &len_out,
                                     "Q", calidad, "optimize_coding", TRUE, NULL) == 0)
            {
                guardar_buffer(ruta_out, buf_out, len_out);
                g_free(buf_out);
            }
            g_free(buf);
            g_object_unref(rotada);
            return 1;
        }

        g_free(buf);

        if (!ssim_ok && !ahorro_ok)
            log_printf("  JPEG intento%d: Falla por SSIM insuficiente (%.6f < %.4f) y ahorro insuficiente (%.1f%% %s %.0f%%) -> sig. intento",
                       intento + 1, ssim, umbral, ahorro,
                       ultimo ? "<=" : "<", min_ahorro);
        else if (!ssim_ok)
            log_printf("  JPEG intento%d: Falla por SSIM insuficiente (%.6f < %.4f) | Ahorro OK (%.1f%% %s %.0f%%) -> sig. intento",
                       intento + 1, ssim, umbral, ahorro,
                       ultimo ? ">" : ">=", min_ahorro);
        else
            log_printf("  JPEG intento%d: SSIM OK (%.4f >= %.4f) | Falla por ahorro insuficiente (%.1f%% %s %.0f%%) -> sig. intento",
                       intento + 1, ssim, umbral, ahorro,
                       ultimo ? "<=" : "<", min_ahorro);
    }

    log_printf("  JPEG: ningun intento logro comprimir -> copia del original");
    g_object_unref(rotada);
    copiar_archivo(ruta_in, ruta_out);
    return 3;
}
