add_executable(test_utf test_utf.c)
utf_target_defaults(test_utf)
target_link_libraries(test_utf PRIVATE utf99)
add_test(NAME test_utf COMMAND test_utf)
