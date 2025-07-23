// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Pionix GmbH and Contributors to EVerest
#include <iso15118/detail/io/helper_ssl.hpp>

#include <cassert>
#include <stdexcept>

#ifndef ESP_PLATFORM
#include <openssl/err.h>
#else
#include <mbedtls/error.h>
#endif

namespace iso15118::io {

static int add_error_str(const char* str, std::size_t len, void* u) {
    assert(u);
    auto& text = *reinterpret_cast<std::string*>(u);
    text += ": " + std::string(str, len);
    return 0;
}

static void log_and_raise(const std::string& error_msg) {
    throw std::runtime_error(error_msg);
}

void log_and_raise_tls_error(const std::string& error_msg) {
#ifndef ESP_PLATFORM
    std::string error_message = {error_msg};
    ERR_print_errors_cb(&add_error_str, &error_message);
    log_and_raise(error_message);
#else
    char buf[128];
    mbedtls_strerror(mbedtls_ssl_get_verify_result(nullptr), buf, sizeof(buf));
    std::string msg = error_msg + ": " + buf;
    log_and_raise(msg);
#endif
}

} // namespace iso15118::io
