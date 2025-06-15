# CMake generated Testfile for 
# Source directory: C:/Users/Admin/Desktop/key
# Build directory: C:/Users/Admin/Desktop/key/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(KeyloggerTests "C:/Users/Admin/Desktop/key/build/test_keylogger.exe")
set_tests_properties(KeyloggerTests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Admin/Desktop/key/CMakeLists.txt;65;add_test;C:/Users/Admin/Desktop/key/CMakeLists.txt;0;")
subdirs("doctest")
