include(Catch)

add_executable(test_logging logging.cpp)

target_link_libraries(test_logging
    PRIVATE
        iso15118
        Catch2::Catch2WithMain
)

catch_discover_tests(test_logging)

add_executable(connection_openssl_test)
add_custom_command(
    TARGET connection_openssl_test POST_BUILD
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/pki
    COMMAND cd pki && cp pki.sh ${CMAKE_CURRENT_BINARY_DIR}/pki
    COMMAND cp -r configs ${CMAKE_CURRENT_BINARY_DIR}/pki
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
target_sources(connection_openssl_test
    PRIVATE
        connection_openssl.cpp
)
target_link_libraries(connection_openssl_test
    PRIVATE
        iso15118::iso15118
)

if(ESP_PLATFORM)
add_executable(connection_mbedtls_test connection_mbedtls.cpp)
add_custom_command(
    TARGET connection_mbedtls_test POST_BUILD
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/pki
    COMMAND cd pki && cp pki.sh ${CMAKE_CURRENT_BINARY_DIR}/pki
    COMMAND cp -r configs ${CMAKE_CURRENT_BINARY_DIR}/pki
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(connection_mbedtls_test PRIVATE iso15118::iso15118)
endif()
