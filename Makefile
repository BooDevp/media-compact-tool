CC = gcc

# Carpetas de dependencias (Rutas relativas a la raíz de tu proyecto)
VIPS_DEV = ./vips-dev
FFMPEG_DEV = ./ffmpeg-dev

# 1. Headers (.h): Incluimos las rutas de VIPS, GLIB y FFMPEG
CFLAGS = -I$(VIPS_DEV)/include \
         -I$(VIPS_DEV)/include/glib-2.0 \
         -I$(VIPS_DEV)/lib/glib-2.0/include \
         -I$(FFMPEG_DEV)/include \
         -I./include \
         -Wall -O3

# 2. Librerías de enlace (.lib / .a):
# -L indica dónde buscar, -l indica qué librería usar
LDFLAGS = -L$(VIPS_DEV)/lib -lvips -lglib-2.0 -lgobject-2.0 \
          -L$(FFMPEG_DEV)/lib -lavformat -lavcodec -lavutil -lswscale -lswresample \
          -lm

OBJ = compactador.exe

# Buscamos todos los archivos .c en la carpeta src y sus subcarpetas
SRC = $(shell find src -name "*.c")

# Regla principal
all:
	$(CC) $(SRC) -o $(OBJ) $(CFLAGS) $(LDFLAGS)

# Limpieza básica
clean:
	rm -f $(OBJ)