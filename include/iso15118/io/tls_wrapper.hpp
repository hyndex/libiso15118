#pragma once

#ifdef ESP_PLATFORM
#include <mbedtls/ssl.h>
namespace iso15118::io {
struct TlsSocket {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    int sock;
};
int tls_read(TlsSocket*, void*, size_t);
int tls_write(TlsSocket*, const void*, size_t);
}
#endif
