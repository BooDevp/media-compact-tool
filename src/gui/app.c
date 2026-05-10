#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <vips/vips.h>

#include <scanner.h>
#include <imagen.h>
#include <video.h>
#include <config.h>
#include <progreso.h>
#include <str_util.h>
#include <log.h>

// ── Colores ──
static const COLORREF COL_BG = RGB(30, 30, 30);
static const COLORREF COL_ACCENT = RGB(0, 175, 239);
static const COLORREF COL_WHITE = RGB(255, 255, 255);
static const COLORREF COL_MUTED = RGB(160, 160, 160);
static const COLORREF COL_DROP_BG = RGB(40, 40, 40);
static const COLORREF COL_SUCCESS = RGB(0, 223, 135);
static const COLORREF COL_BAR_BG = RGB(50, 50, 50);
static const COLORREF COL_BAR_FILL = RGB(0, 175, 239);

// ── Forward declarations ──
static void mostrar_ventana_botones(HWND hwnd);
static void iniciar_procesamiento(void);

// ── Botones ──
#define ID_BTN_COMPRIMIR 200
#define ID_BTN_SALIR 201
#define ID_BTN_ABRIR 202
#define ID_BTN_VOLVER 203

// ── Mensajes personalizados ──
#define WM_ACTUALIZAR_VISTA (WM_APP + 1)
#define WM_PROCESO_TERMINADO (WM_APP + 2)
#define WM_INICIAR_PROCESO (WM_APP + 3)

// ── Vistas ──
#define VISTA_DROP 0
#define VISTA_PROCESANDO 1
#define VISTA_RESULTADOS 2

// ── Estado de la aplicacion ──
typedef struct
{
    volatile int vista;
    volatile int contando;
    volatile double pct_total;
    volatile int procesados;
    volatile int total;
    char archivo_actual[MAX_PATH_LEN];
    char tipo_actual[32];
    volatile double pct_archivo;
    Estadisticas stats;
    char destino[MAX_PATH_LEN];
    volatile int abrir_explorador;
    char ruta_drop[MAX_PATH_LEN];
    HWND hwnd;
    HANDLE hThread;
} AppState;

static AppState g_estado = {0};
static int g_clase_registrada = 0;

static int es_ruta_valida(const char *ruta)
{
    struct _stat st;
    return _stat(ruta, &st) == 0;
}

static void centrar_ventana(HWND hwnd)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int ancho = rc.right - rc.left;
    int alto = rc.bottom - rc.top;
    int scr_w = GetSystemMetrics(SM_CXSCREEN);
    int scr_h = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, NULL,
                 (scr_w - ancho) / 2,
                 (scr_h - alto) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// ── Callbacks de progreso (llamados desde hilo worker) ──
static void cb_total(double pct, int procesados, int total, void *user)
{
    (void)user;
    g_estado.pct_total = pct;
    g_estado.procesados = procesados;
    if (total > 0)
        g_estado.total = total;
    PostMessage(g_estado.hwnd, WM_ACTUALIZAR_VISTA, 0, 0);
}

static void cb_archivo(const char *tipo, const char *nombre, double pct, void *user)
{
    (void)user;
    strncpy(g_estado.tipo_actual, tipo ? tipo : "", sizeof(g_estado.tipo_actual) - 1);
    strncpy(g_estado.archivo_actual, nombre ? nombre : "", sizeof(g_estado.archivo_actual) - 1);
    g_estado.pct_archivo = pct;
    PostMessage(g_estado.hwnd, WM_ACTUALIZAR_VISTA, 0, 0);
}

static void cb_fin(int total, Estadisticas *stats, const char *destino, int abrir, void *user)
{
    (void)user;
    g_estado.stats = *stats;
    strncpy(g_estado.destino, destino, sizeof(g_estado.destino) - 1);
    g_estado.abrir_explorador = abrir;
    PostMessage(g_estado.hwnd, WM_PROCESO_TERMINADO, (WPARAM)total, 0);
}

// ── Parametros para el hilo worker ──
typedef struct
{
    char ruta[MAX_PATH_LEN];
    char salida[MAX_PATH_LEN + 20];
    int es_directorio;
} ThreadParams;

static DWORD WINAPI ProcesarThread(LPVOID lpParam)
{
    ThreadParams *p = (ThreadParams *)lpParam;
    Estadisticas stats = {0, 0};
    int procesados = 0;

    if (p->es_directorio)
    {
        g_estado.contando = 1;
        int total = 0;
        contar_media_recursiva(p->ruta, &total);
        g_estado.contando = 0;
        g_estado.total = total;
        PostMessage(g_estado.hwnd, WM_ACTUALIZAR_VISTA, 0, 0);

        if (total > 0)
        {
            procesar_recursivo_con_progreso(p->ruta, p->salida, &stats, total, &procesados);
        }
    }
    else
    {
        g_estado.total = 1;
        g_estado.contando = 0;
        procesados = procesar_archivo_unico(p->ruta, p->salida, &stats);
    }

    progreso_imprimir_fin(procesados, stats, p->salida, p->es_directorio);

    free(p);
    return 0;
}

static void iniciar_procesamiento(void)
{
    if (g_estado.hThread)
    {
        CloseHandle(g_estado.hThread);
        g_estado.hThread = NULL;
    }

    ThreadParams *p = malloc(sizeof(ThreadParams));
    if (!p)
        return;

    strncpy(p->ruta, g_estado.ruta_drop, sizeof(p->ruta) - 1);
    p->ruta[sizeof(p->ruta) - 1] = '\0';

    p->es_directorio = es_directorio(g_estado.ruta_drop);

    if (p->es_directorio)
        snprintf(p->salida, sizeof(p->salida), "%s%s", g_estado.ruta_drop, SUFIJO_CARPETA);
    else
        generar_ruta_salida_archivo(g_estado.ruta_drop, p->salida, sizeof(p->salida));

    g_estado.vista = VISTA_PROCESANDO;
    g_estado.pct_total = 0.0;
    g_estado.procesados = 0;
    g_estado.total = 0;
    g_estado.pct_archivo = 0.0;
    g_estado.archivo_actual[0] = '\0';
    g_estado.tipo_actual[0] = '\0';

    mostrar_ventana_botones(g_estado.hwnd);

    InvalidateRect(g_estado.hwnd, NULL, TRUE);
    UpdateWindow(g_estado.hwnd);

    g_estado.hThread = CreateThread(NULL, 0, ProcesarThread, p, 0, NULL);
}

// ── Owner-draw botones ──
static void pintar_boton(HDC hdc, RECT *rc, int activo, int hover, const char *texto,
                         COLORREF col_borde, COLORREF col_texto)
{
    COLORREF bg = activo ? (hover ? RGB(55, 55, 55) : RGB(42, 42, 42))
                         : RGB(25, 25, 25);
    COLORREF border = activo ? col_borde : RGB(60, 60, 60);
    COLORREF fg = activo ? col_texto : RGB(80, 80, 80);

    HBRUSH hBrush = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    SelectObject(hdc, hBrush);
    SelectObject(hdc, hPen);
    Rectangle(hdc, rc->left, rc->top, rc->right, rc->bottom);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    DrawTextA(hdc, texto, -1, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

// ── Crear/mostrar/ocultar botones segun vista ──
static void mostrar_ventana_botones(HWND hwnd)
{
    int show_drop = (g_estado.vista == VISTA_DROP);
    int show_results = (g_estado.vista == VISTA_RESULTADOS);

    ShowWindow(GetDlgItem(hwnd, ID_BTN_COMPRIMIR), show_drop ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, ID_BTN_SALIR), show_drop ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, ID_BTN_ABRIR), show_results ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, ID_BTN_VOLVER), show_results ? SW_SHOW : SW_HIDE);
}

// ── Dibujado de vistas ──
static void dibujar_texto_centrado(HDC hdc, int y, int ancho, const char *texto,
                                   COLORREF color, int tam, int bold)
{
    HFONT hFont = CreateFontA(tam, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hOld = SelectObject(hdc, hFont);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    RECT rc = {0, y, ancho, y + tam + 10};
    DrawTextA(hdc, texto, -1, &rc, DT_CENTER | DT_SINGLELINE);
    SelectObject(hdc, hOld);
    DeleteObject(hFont);
}

static void dibujar_barra(HDC hdc, int x, int y, int w, int h, double pct)
{
    HBRUSH hBg = CreateSolidBrush(COL_BAR_BG);
    RECT rc = {x, y, x + w, y + h};
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    if (pct > 0.0)
    {
        int relleno = (int)(w * pct);
        if (relleno < 1)
            relleno = 1;
        HBRUSH hFill = CreateSolidBrush(COL_BAR_FILL);
        RECT rcFill = {x, y, x + relleno, y + h};
        FillRect(hdc, &rcFill, hFill);
        DeleteObject(hFill);
    }

    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(70, 70, 70));
    HGDIOBJ hOld = SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, hOld);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
}

static void dibujar_escaparate(HDC hdc, int w, int h)
{
    SetTextColor(hdc, COL_MUTED);
    SetBkMode(hdc, TRANSPARENT);
    RECT rc = {w - 120, h - 22, w - 10, h - 4};
    DrawTextA(hdc, "ESC para salir", -1, &rc, DT_RIGHT | DT_SINGLELINE);
}

static void dibujar_vista_drop(HDC hdc, int w, int h)
{
    // Instruccion
    dibujar_texto_centrado(hdc, 52, w, "ARRASTA UN ARCHIVO O CARPETA AQUI", COL_MUTED, 12, 0);

    // Area de drop
    int dl = 50, dr = w - 50, dt = 80, db = 185;
    HBRUSH hDropBrush = CreateSolidBrush(COL_DROP_BG);
    HPEN hDropPen = CreatePen(PS_DASH, 1, COL_ACCENT);
    HGDIOBJ hOldBrush = SelectObject(hdc, hDropBrush);
    HGDIOBJ hOldPen = SelectObject(hdc, hDropPen);
    Rectangle(hdc, dl, dt, dr, db);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hDropBrush);
    DeleteObject(hDropPen);

    if (g_estado.ruta_drop[0])
    {
        RECT rcPath = {dl + 10, dt + 35, dr - 10, db - 10};
        SetTextColor(hdc, COL_WHITE);
        SetBkMode(hdc, TRANSPARENT);
        char buf[MAX_PATH_LEN];
        int len = strlen(g_estado.ruta_drop);
        if (len > 55)
            snprintf(buf, sizeof(buf), "...%s", g_estado.ruta_drop + len - 52);
        else
            snprintf(buf, sizeof(buf), "%s", g_estado.ruta_drop);
        DrawTextA(hdc, buf, -1, &rcPath, DT_CENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
    }
    else
    {
        RECT rcPh = {dl + 10, dt + 35, dr - 10, db - 10};
        SetTextColor(hdc, COL_MUTED);
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, "Suelta el archivo o carpeta aqui", -1, &rcPh, DT_CENTER | DT_SINGLELINE);
    }

    dibujar_escaparate(hdc, w, h);
}

static void dibujar_vista_procesando(HDC hdc, int w, int h)
{
    if (g_estado.contando)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "Analizando archivos... (%d encontrados)", g_estado.procesados);
        dibujar_texto_centrado(hdc, 80, w, buf, COL_MUTED, 14, 0);
        dibujar_escaparate(hdc, w, h);
        return;
    }

    // Barra total
    int bx = 50, bw = w - 100, by = 68, bh = 18;
    dibujar_barra(hdc, bx, by, bw, bh, g_estado.pct_total);

    // Porcentaje total
    char buf_pct[32];
    snprintf(buf_pct, sizeof(buf_pct), "%3.0f%%", g_estado.pct_total * 100);
    SetTextColor(hdc, COL_WHITE);
    SetBkMode(hdc, TRANSPARENT);
    RECT rcPct = {bx + bw + 8, by - 2, w - 10, by + 22};
    DrawTextA(hdc, buf_pct, -1, &rcPct, DT_LEFT | DT_SINGLELINE);

    char buf_cnt[64];
    snprintf(buf_cnt, sizeof(buf_cnt), "Archivos: %d / %d", g_estado.procesados, g_estado.total);
    SetTextColor(hdc, COL_MUTED);
    SetBkMode(hdc, TRANSPARENT);
    RECT rcCnt = {bx, by + 24, w - 10, by + 44};
    DrawTextA(hdc, buf_cnt, -1, &rcCnt, DT_LEFT | DT_SINGLELINE);

    if (g_estado.archivo_actual[0])
    {
        char tipo_label[64];
        if (g_estado.tipo_actual[0])
            snprintf(tipo_label, sizeof(tipo_label), "Procesando: %s", g_estado.tipo_actual);
        else
            snprintf(tipo_label, sizeof(tipo_label), "Procesando...");
        SetTextColor(hdc, COL_ACCENT);
        SetBkMode(hdc, TRANSPARENT);
        RECT rcTipo = {bx, 128, w - 10, 150};
        DrawTextA(hdc, tipo_label, -1, &rcTipo, DT_LEFT | DT_SINGLELINE);

        SetTextColor(hdc, COL_WHITE);
        SetBkMode(hdc, TRANSPARENT);
        RECT rcNom = {bx, 148, w - 10, 168};
        DrawTextA(hdc, g_estado.archivo_actual, -1, &rcNom, DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS);

        // Barra archivo
        dibujar_barra(hdc, bx, 170, bw, 14, g_estado.pct_archivo);
    }

    dibujar_escaparate(hdc, w, h);
}

static void dibujar_vista_resultados(HDC hdc, int w, int h)
{
    // Checkmark
    dibujar_texto_centrado(hdc, 60, w, "PROCESO COMPLETADO", COL_SUCCESS, 20, 1);

    // Stats
    char buf[128];
    int y = 95;
    snprintf(buf, sizeof(buf), "Imagenes:  %d", g_estado.stats.imagenes);
    dibujar_texto_centrado(hdc, y, w, buf, COL_WHITE, 14, 0);
    y += 22;
    snprintf(buf, sizeof(buf), "Videos:    %d", g_estado.stats.videos);
    dibujar_texto_centrado(hdc, y, w, buf, COL_WHITE, 14, 0);
    y += 22;
    snprintf(buf, sizeof(buf), "Total:     %d", g_estado.stats.imagenes + g_estado.stats.videos);
    dibujar_texto_centrado(hdc, y, w, buf, COL_ACCENT, 16, 1);

    if (g_estado.destino[0] && (g_estado.stats.imagenes > 0 || g_estado.stats.videos > 0))
    {
        y += 30;
        char dest_buf[MAX_PATH_LEN + 20];
        snprintf(dest_buf, sizeof(dest_buf), "Destino: %s", g_estado.destino);
        SetTextColor(hdc, COL_MUTED);
        SetBkMode(hdc, TRANSPARENT);
        RECT rc = {30, y, w - 30, y + 20};
        DrawTextA(hdc, dest_buf, -1, &rc, DT_CENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
    }

    dibujar_escaparate(hdc, w, h);
}

// ── Ventana principal ──
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HFONT hDef = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        CreateWindow("BUTTON", "COMPRIMIR",
                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                     195, 200, 130, 32,
                     hwnd, (HMENU)ID_BTN_COMPRIMIR, hInst, NULL);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_COMPRIMIR), FALSE);

        CreateWindow("BUTTON", "SALIR",
                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                     340, 200, 80, 32,
                     hwnd, (HMENU)ID_BTN_SALIR, hInst, NULL);

        CreateWindow("BUTTON", "ABRIR CARPETA",
                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                     165, 210, 130, 32,
                     hwnd, (HMENU)ID_BTN_ABRIR, hInst, NULL);

        CreateWindow("BUTTON", "VOLVER",
                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                     320, 210, 90, 32,
                     hwnd, (HMENU)ID_BTN_VOLVER, hInst, NULL);

        SendDlgItemMessage(hwnd, ID_BTN_COMPRIMIR, WM_SETFONT, (WPARAM)hDef, TRUE);
        SendDlgItemMessage(hwnd, ID_BTN_SALIR, WM_SETFONT, (WPARAM)hDef, TRUE);
        SendDlgItemMessage(hwnd, ID_BTN_ABRIR, WM_SETFONT, (WPARAM)hDef, TRUE);
        SendDlgItemMessage(hwnd, ID_BTN_VOLVER, WM_SETFONT, (WPARAM)hDef, TRUE);

        ShowWindow(GetDlgItem(hwnd, ID_BTN_ABRIR), SW_HIDE);
        ShowWindow(GetDlgItem(hwnd, ID_BTN_VOLVER), SW_HIDE);

        DragAcceptFiles(hwnd, TRUE);
        break;
    }

    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        UINT count = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);
        if (count > 0)
        {
            DragQueryFileA(hDrop, 0, g_estado.ruta_drop, MAX_PATH_LEN);
            for (char *p = g_estado.ruta_drop; *p; p++)
                if (*p == '\\')
                    *p = '/';

            EnableWindow(GetDlgItem(hwnd, ID_BTN_COMPRIMIR),
                         es_ruta_valida(g_estado.ruta_drop) ? TRUE : FALSE);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        DragFinish(hDrop);
        break;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (HIWORD(wParam) == BN_CLICKED)
        {
            if (id == ID_BTN_COMPRIMIR)
                iniciar_procesamiento();
            else if (id == ID_BTN_SALIR)
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            else if (id == ID_BTN_VOLVER)
            {
                g_estado.vista = VISTA_DROP;
                g_estado.ruta_drop[0] = '\0';
                EnableWindow(GetDlgItem(hwnd, ID_BTN_COMPRIMIR), FALSE);
                mostrar_ventana_botones(hwnd);
                InvalidateRect(hwnd, NULL, TRUE);
                UpdateWindow(hwnd);
            }
            else if (id == ID_BTN_ABRIR)
            {
                char ruta[MAX_PATH_LEN];
                strncpy(ruta, g_estado.destino, sizeof(ruta) - 1);
                ruta[sizeof(ruta) - 1] = '\0';
                for (char *p = ruta; *p; p++)
                    if (*p == '/')
                        *p = '\\';
                ShellExecuteA(hwnd, "open", ruta, NULL, NULL, SW_SHOWNORMAL);
            }
        }
        break;
    }

    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            if (g_estado.vista == VISTA_DROP || g_estado.vista == VISTA_RESULTADOS)
                PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        break;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        const char *texto = NULL;
        COLORREF borde = COL_ACCENT, color_txt = COL_WHITE;

        if (dis->CtlID == ID_BTN_COMPRIMIR)
        {
            texto = "COMPRIMIR";
            borde = COL_ACCENT;
        }
        else if (dis->CtlID == ID_BTN_SALIR)
        {
            texto = "SALIR";
            borde = COL_MUTED;
        }
        else if (dis->CtlID == ID_BTN_ABRIR)
        {
            texto = "ABRIR CARPETA";
            borde = COL_SUCCESS;
            color_txt = COL_SUCCESS;
        }
        else if (dis->CtlID == ID_BTN_VOLVER)
        {
            texto = "VOLVER";
            borde = COL_MUTED;
        }

        if (texto)
        {
            int activo = !(dis->itemState & ODS_DISABLED);
            int hover = (dis->itemState & ODS_HOTLIGHT) || (dis->itemState & ODS_SELECTED);
            pintar_boton(dis->hDC, &dis->rcItem, activo, hover, texto, borde, color_txt);
        }
        return TRUE;
    }

    case WM_ACTUALIZAR_VISTA:
    {
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
        break;
    }

    case WM_PROCESO_TERMINADO:
    {
        g_estado.vista = VISTA_RESULTADOS;
        if (g_estado.hThread)
        {
            CloseHandle(g_estado.hThread);
            g_estado.hThread = NULL;
        }
        mostrar_ventana_botones(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);

        if (g_estado.abrir_explorador)
        {
            char ruta[MAX_PATH_LEN];
            strncpy(ruta, g_estado.destino, sizeof(ruta) - 1);
            ruta[sizeof(ruta) - 1] = '\0';
            for (char *p = ruta; *p; p++)
                if (*p == '/')
                    *p = '\\';
            ShellExecuteA(hwnd, "open", ruta, NULL, NULL, SW_SHOWNORMAL);
        }
        break;
    }

    case WM_INICIAR_PROCESO:
    {
        iniciar_procesamiento();
        break;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH hBg = CreateSolidBrush(COL_BG);
        FillRect(hdc, &ps.rcPaint, hBg);
        DeleteObject(hBg);

        switch (g_estado.vista)
        {
        case VISTA_DROP:
            dibujar_vista_drop(hdc, rc.right, rc.bottom);
            break;
        case VISTA_PROCESANDO:
            dibujar_vista_procesando(hdc, rc.right, rc.bottom);
            break;
        case VISTA_RESULTADOS:
            dibujar_vista_resultados(hdc, rc.right, rc.bottom);
            break;
        }

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_CLOSE:
    {
        if (g_estado.hThread)
        {
            WaitForSingleObject(g_estado.hThread, 5000);
            CloseHandle(g_estado.hThread);
            g_estado.hThread = NULL;
        }
        DestroyWindow(hwnd);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ── Punto de entrada de la GUI ──
void iniciar_app(int argc, char *argv[])
{
    HINSTANCE hInst = GetModuleHandle(NULL);

    progreso_registrar(cb_total, cb_archivo, cb_fin, NULL);

    if (!g_clase_registrada)
    {
        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = "CompactadorMainClass";
        wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(100));
        wc.hIconSm = (HICON)LoadImage(hInst, MAKEINTRESOURCE(100), IMAGE_ICON, 16, 16, 0);
        wc.hbrBackground = NULL;

        if (!RegisterClassExA(&wc))
            return;

        g_clase_registrada = 1;
    }

    HWND hwnd = CreateWindowExA(
        0, "CompactadorMainClass", "Compactador v1.0",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 360,
        NULL, NULL, hInst, NULL);

    if (!hwnd)
        return;

    g_estado.hwnd = hwnd;
    centrar_ventana(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    if (argc > 1 && argv[1])
    {
        strncpy(g_estado.ruta_drop, argv[1], MAX_PATH_LEN - 1);
        limpiar_ruta(g_estado.ruta_drop);
        if (es_ruta_valida(g_estado.ruta_drop))
            PostMessage(hwnd, WM_INICIAR_PROCESO, 0, 0);
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
