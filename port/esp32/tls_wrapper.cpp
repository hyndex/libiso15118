#include "posix_stub.hpp"
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ticket.h>
#include <mbedtls/error.h>
#include <cstring>

namespace iso15118::io {
struct TlsSocket {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    int sock;
};

static void tls_socket_init(TlsSocket& t) {
    mbedtls_ssl_init(&t.ssl);
    mbedtls_ssl_config_init(&t.conf);
    t.sock = -1;
}

int tls_read(TlsSocket* t, void* buf, size_t len) {
    if (!t) return -1;
    return mbedtls_ssl_read(&t->ssl, reinterpret_cast<unsigned char*>(buf), len);
}

int tls_write(TlsSocket* t, const void* buf, size_t len) {
    if (!t) return -1;
    return mbedtls_ssl_write(&t->ssl, reinterpret_cast<const unsigned char*>(buf), len);
}
} // namespace iso15118::io
