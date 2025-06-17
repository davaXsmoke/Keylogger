# CMake generated Testfile for 
# Source directory: C:/Users/user/Desktop/key1
# Build directory: C:/Users/user/Desktop/key1/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(KeyloggerTests "C:/Users/user/Desktop/key1/build/test_keylogger.exe")
set_tests_properties(KeyloggerTests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/user/Desktop/key1/CMakeLists.txt;60;add_test;C:/Users/user/Desktop/key1/CMakeLists.txt;0;")
subdirs("doctest")
