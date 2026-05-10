#include <sistema.h>
#include <config.h>

#ifdef _WIN32
#include <windows.h>

void configurar_consola(void)
{
    SetConsoleTitle(TITLE_APP);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(CP_UTF8);
}
#else
void configurar_consola(void) { }
#endif
