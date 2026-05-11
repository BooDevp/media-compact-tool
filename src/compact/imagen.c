#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <vips/vips.h>

#include <config.h>
#include <log.h>

/**
 * SSIM Basado en Luminancia (Espacio de color LAB)
 * Extrae el canal L para comparar cómo el ojo humano percibe realmente la luz y el contraste.
 */
static double ssim_vips(VipsImage *ref, VipsImage *cmp)
{
    VipsImage *ref_lab = NULL, *cmp_lab = NULL;
    VipsImage *ref_L = NULL, *cmp_L = NULL;
    VipsImage *ref_f = NULL, *cmp_f = NULL;

    // Convertimos al espacio LAB para aislar la luminancia (Banda 0)
    if (vips_colourspace(ref, &ref_lab, VIPS_INTERPRETATION_LAB, NULL) ||
        vips_colourspace(cmp, &cmp_lab, VIPS_INTERPRETATION_LAB, NULL)) {
        if (ref_lab) g_object_unref(ref_lab);
        if (cmp_lab) g_object_unref(cmp_lab);
        return 0.0;
    }

    vips_extract_band(ref_lab, &ref_L, 0, NULL);
    vips_extract_band(cmp_lab, &cmp_L, 0, NULL);

    vips_cast_float(ref_L, &ref_f, NULL);
    vips_cast_float(cmp_L, &cmp_f, NULL);

    // Parámetros estándar SSIM
    double sigma = 1.5;
    double C1 = (0.01 * 100.0) * (0.01 * 100.0); // En LAB, L va de 0 a 100
    double C2 = (0.03 * 100.0) * (0.03 * 100.0);

    VipsImage *mu1 = NULL, *mu2 = NULL, *mu1_sq = NULL, *mu2_sq = NULL, *mu12 = NULL;
    vips_gaussblur(ref_f, &mu1, sigma, NULL);
    vips_gaussblur(cmp_f, &mu2, sigma, NULL);
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

    VipsImage *den1 = NULL, *den2 = NULL, *den1_c1 = NULL, *den2_c2 = NULL, *den = NULL;
    vips_add(mu1_sq, mu2_sq, &den1, NULL);
    vips_add(sigma1_sq, sigma2_sq, &den2, NULL);
    vips_linear1(den1, &den1_c1, 1.0, C1, NULL);
    vips_linear1(den2, &den2_c2, 1.0, C2, NULL);
    vips_multiply(den1_c1, den2_c2, &den, NULL);

    VipsImage *ssim_map = NULL;
    vips_divide(num, den, &ssim_map, NULL);

    double ssim_val;
    vips_avg(ssim_map, &ssim_val, NULL);

    // Limpieza masiva de objetos
    g_object_unref(ref_lab); g_object_unref(cmp_lab);
    g_object_unref(ref_L); g_object_unref(cmp_L);
    g_object_unref(ref_f); g_object_unref(cmp_f);
    g_object_unref(mu1); g_object_unref(mu2);
    g_object_unref(mu1_sq); g_object_unref(mu2_sq); g_object_unref(mu12);
    g_object_unref(ref_sq); g_object_unref(blur_ref_sq); g_object_unref(sigma1_sq);
    g_object_unref(cmp_sq); g_object_unref(blur_cmp_sq); g_object_unref(sigma2_sq);
    g_object_unref(ref_cmp); g_object_unref(blur_ref_cmp); g_object_unref(sigma12);
    g_object_unref(num1); g_object_unref(num2); g_object_unref(num);
    g_object_unref(den1); g_object_unref(den2); g_object_unref(den1_c1); g_object_unref(den2_c2);
    g_object_unref(den); g_object_unref(ssim_map);

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
    VipsImage *original = vips_image_new_from_file(ruta_in, NULL);
    if (!original) return 0;

    // 1. Verificación de firma optimizada
    const char *artist;
    if (vips_image_get_typeof(original, "exif-ifd0-Artist") != 0 &&
        vips_image_get_string(original, "exif-ifd0-Artist", &artist) == 0 &&
        strstr(artist, FIRMA_OPTIMIZADO)) {
        g_object_unref(original);
        return 2;
    }

    // 2. Autorotación y limpieza de metadatos pesados
    VipsImage *rotada = NULL;
    if (vips_autorot(original, &rotada, NULL)) {
        g_object_unref(original);
        return 0;
    }
    g_object_unref(original);

    vips_image_remove(rotada, "exif-data");
    vips_image_remove(rotada, "iptc-data");
    vips_image_remove(rotada, "xmp-data");

    // Insertamos la firma ANTES de probar las calidades para evitar doble compresión
    vips_image_set_string(rotada, "exif-ifd0-Artist", FIRMA_OPTIMIZADO);

    struct stat st;
    size_t tam_original = (stat(ruta_in, &st) == 0) ? (size_t)st.st_size : 0;

    // 3. Lógica para PNG (Palette)
    if (vips_image_hasalpha(rotada)) {
        void *buf = NULL; size_t len = 0;
        if (vips_pngsave_buffer(rotada, &buf, &len, "palette", TRUE, "colours", PNG_MAX_COLORES, NULL) == 0) {
            VipsImage *comprimida = vips_image_new_from_buffer(buf, len, "", NULL);
            if (comprimida) {
                double ssim = ssim_vips(rotada, comprimida);
                if (ssim >= SSIM_UMBRAL_PNG && len < tam_original) {
                    guardar_buffer(ruta_out, buf, len);
                    g_free(buf); g_object_unref(comprimida); g_object_unref(rotada);
                    return 1;
                }
                g_object_unref(comprimida);
            }
            g_free(buf);
        }
        g_object_unref(rotada);
        return 0;
    }

    // 4. Lógica para JPEG (Adaptativo de paso único)
    int calidades[] = {IMG_QUALITY_1, IMG_QUALITY_2, IMG_QUALITY_3, IMG_QUALITY_4};
    double umbrales[] = {SSIM_UMBRAL_1, SSIM_UMBRAL_2, SSIM_UMBRAL_3, SSIM_UMBRAL_4};
    double ahorros_min[] = {AHORRO_MINIMO_1, AHORRO_MINIMO_2, AHORRO_MINIMO_3, AHORRO_MINIMO_4};

    for (int i = 0; i < 4; i++) {
        void *buf = NULL; size_t len = 0;
        if (vips_jpegsave_buffer(rotada, &buf, &len, "Q", calidades[i], "optimize_coding", TRUE, NULL)) continue;

        VipsImage *comprimida = vips_image_new_from_buffer(buf, len, "", NULL);
        if (!comprimida) { g_free(buf); continue; }

        double ssim = ssim_vips(rotada, comprimida);
        g_object_unref(comprimida);

        double ahorro = (1.0 - (double)len / (double)tam_original) * 100.0;
        int ssim_ok = (ssim >= umbrales[i]);
        int ahorro_ok = (i == 3) ? (ahorro > 0.0) : (ahorro >= ahorros_min[i]);

        if (ssim_ok && ahorro_ok) {
            log_printf("  EXITO Q%d: SSIM %.4f, Ahorro %.1f%%", calidades[i], ssim, ahorro);
            guardar_buffer(ruta_out, buf, len);
            g_free(buf); g_object_unref(rotada);
            return 1;
        }
        g_free(buf);
    }

    // 5. Fallback: Copia original si nada convence
    g_object_unref(rotada);
    copiar_archivo(ruta_in, ruta_out);
    return 3;
}