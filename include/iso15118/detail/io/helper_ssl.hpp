// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Pionix GmbH and Contributors to EVerest
#pragma once

#include <string>

namespace iso15118::io {

#ifdef ESP_PLATFORM
void log_and_raise_tls_error(const std::string& error_msg);
#else
void log_and_raise_openssl_error(const std::string& error_msg);
#endif

} // namespace iso15118::io
