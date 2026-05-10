#include <windows.h>
#include <vips/vips.h>
#include <signal.h>
#include <stdlib.h>

#include <config.h>
#include <log.h>
#include <app.h>

static void manejar_sigint(int sig)
{
    (void)sig;
    log_close();
    vips_shutdown();
    exit(0);
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    FreeConsole();
    CreateMutexA(NULL, TRUE, "Global\\MediaCompactor_Unique_Mutex_ID");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;
#endif

    signal(SIGINT, manejar_sigint);

    if (VIPS_INIT(argv[0]))
        return 1;

    log_init();
    iniciar_app(argc, argv);
    log_close();
    vips_shutdown();
    return 0;
}
