cmake_minimum_required(VERSION 3.15)
project(KeyloggerProject)

# Настройки кодировки
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8")

# Подключаем Doctest
add_subdirectory(doctest)

# Основные настройки
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройка OpenSSL
set(OPENSSL_ROOT_DIR "C:/msys64/mingw64")
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.dll.a")
set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.dll.a")
set(WIN32_LIBS ws2_32 crypt32 user32 advapi32)

# Основная библиотека (без main())
add_library(keylogger_lib STATIC
    src/keylogger.cpp
    include/keylogger.h
)

target_include_directories(keylogger_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    "${OPENSSL_INCLUDE_DIR}"
)

# Основное приложение
add_executable(keylogger
    src/main.cpp
)

target_link_libraries(keylogger PRIVATE
    keylogger_lib
    "${OPENSSL_CRYPTO_LIBRARY}"
    "${OPENSSL_SSL_LIBRARY}"
    ${WIN32_LIBS}
)

# Тестовое приложение
add_executable(test_keylogger
    tests/test_key.cpp
)

target_include_directories(test_keylogger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    "${OPENSSL_INCLUDE_DIR}"
)

target_link_libraries(test_keylogger PRIVATE
    keylogger_lib
    doctest::doctest
    "${OPENSSL_CRYPTO_LIBRARY}"
    "${OPENSSL_SSL_LIBRARY}"
    ${WIN32_LIBS}
)

enable_testing()
add_test(NAME KeyloggerTests COMMAND test_keylogger)

# Копирование DLL
add_custom_command(TARGET keylogger POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${OPENSSL_ROOT_DIR}/bin/libcrypto-3-x64.dll"
        "$<TARGET_FILE_DIR:keylogger>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${OPENSSL_ROOT_DIR}/bin/libssl-3-x64.dll"
        "$<TARGET_FILE_DIR:keylogger>"
)

add_custom_command(TARGET test_keylogger POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${OPENSSL_ROOT_DIR}/bin/libcrypto-3-x64.dll"
        "$<TARGET_FILE_DIR:test_keylogger>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${OPENSSL_ROOT_DIR}/bin/libssl-3-x64.dll"
        "$<TARGET_FILE_DIR:test_keylogger>"
)