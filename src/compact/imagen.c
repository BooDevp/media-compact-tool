#include <stdio.h>
#include <string.h>
#include <vips/vips.h>
#include "../../include/config.h"

// Comprime una imagen JPEG con libvips y marca el archivo como optimizado
int compactar_imagen_jpg(const char *ruta_in, const char *ruta_out)
{
    VipsImage *in = vips_image_new_from_file(ruta_in, NULL);
    if (!in)
        return 0;

    const char *artist;
    if (vips_image_get_typeof(in, "exif-ifd0-Artist") != 0 &&
        vips_image_get_string(in, "exif-ifd0-Artist", &artist) == 0 &&
        strstr(artist, FIRMA_OPTIMIZADO))
    {
        g_object_unref(in);
        return 0;
    }

    VipsImage *rotada;
    if (vips_autorot(in, &rotada, NULL))
    {
        g_object_unref(in);
        return 0;
    }

    vips_image_set_string(rotada, "exif-ifd0-Artist", FIRMA_OPTIMIZADO);
    int ret = vips_jpegsave(rotada, ruta_out, "Q", IMG_QUALITY, "optimize_coding", TRUE, NULL);

    g_object_unref(in);
    g_object_unref(rotada);
    return (ret == 0);
}