#include "posix_stub.hpp"
#include <iso15118/io/connection_ssl.hpp>
#include <iso15118/detail/io/socket_helper.hpp>
#include <iso15118/detail/helper.hpp>
#include <iso15118/io/tls_wrapper.hpp>

#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ticket.h>
#include <mbedtls/error.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>

namespace iso15118::io {

struct MbedTLSContext {
    TlsSocket socket;
    mbedtls_x509_crt cert;
    mbedtls_x509_crt ca;
    mbedtls_pk_context key;
    int fd{-1};
    int accept_fd{-1};
};

static int net_send(void *ctx, const unsigned char* buf, size_t len) {
    int fd = *static_cast<int*>(ctx);
    int ret = ::send(fd, buf, len, 0);
    if (ret < 0) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    return ret;
}

static int net_recv(void *ctx, unsigned char* buf, size_t len) {
    int fd = *static_cast<int*>(ctx);
    int ret = ::recv(fd, buf, len, 0);
    if (ret < 0) return MBEDTLS_ERR_SSL_WANT_READ;
    if (ret == 0) return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
    return ret;
}

ConnectionSSL::ConnectionSSL(PollManager& poll_manager_, const std::string& interface_name,
                             const config::SSLConfig& ssl_config)
    : poll_manager(poll_manager_), ssl(std::make_unique<MbedTLSContext>()) {

    mbedtls_ssl_init(&ssl->socket.ssl);
    mbedtls_ssl_config_init(&ssl->socket.conf);
    mbedtls_x509_crt_init(&ssl->cert);
    mbedtls_x509_crt_init(&ssl->ca);
    mbedtls_pk_init(&ssl->key);

    // load certificates
    mbedtls_x509_crt_parse_file(&ssl->cert, ssl_config.path_certificate_chain.c_str());
    mbedtls_x509_crt_parse_file(&ssl->ca, ssl_config.path_certificate_v2g_root.c_str());
    mbedtls_x509_crt_parse_file(&ssl->ca, ssl_config.path_certificate_mo_root.c_str());
    const char* pw = nullptr;
    if (ssl_config.private_key_password)
        pw = ssl_config.private_key_password->c_str();
    mbedtls_pk_parse_keyfile(&ssl->key, ssl_config.path_certificate_key.c_str(), pw);

    mbedtls_ssl_config_defaults(&ssl->socket.conf, MBEDTLS_SSL_IS_SERVER,
                                MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_ca_chain(&ssl->socket.conf, &ssl->ca, nullptr);
    mbedtls_ssl_conf_own_cert(&ssl->socket.conf, &ssl->cert, &ssl->key);
    mbedtls_ssl_conf_authmode(&ssl->socket.conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_setup(&ssl->socket.ssl, &ssl->socket.conf);

    sockaddr_in6 address;
    if (!get_first_sockaddr_in6_for_interface(interface_name, address)) {
        log_and_throw("Failed to get ipv6 socket address for interface " + interface_name);
    }
    end_point.port = 50000;
    memcpy(&end_point.address, &address.sin6_addr, sizeof(address.sin6_addr));

    ssl->fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (ssl->fd == -1) {
        log_and_throw("Failed to create an ipv6 socket");
    }

    address.sin6_port = htobe16(end_point.port);
    int opt = 1;
    setsockopt(ssl->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(ssl->fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    if (bind(ssl->fd, reinterpret_cast<const struct sockaddr*>(&address), sizeof(address)) == -1) {
        log_and_throw("Failed to bind ipv6 socket to interface " + interface_name);
    }

    if (listen(ssl->fd, 4) == -1) {
        log_and_throw("Listen on socket failed");
    }

    poll_manager.register_fd(ssl->fd, [this]() { this->handle_connect(); });
}

ConnectionSSL::~ConnectionSSL() = default;

void ConnectionSSL::set_event_callback(const ConnectionEventCallback& cb) {
    event_callback = cb;
}

Ipv6EndPoint ConnectionSSL::get_public_endpoint() const { return end_point; }

std::optional<sha512_hash_t> ConnectionSSL::get_vehicle_cert_hash() const { return std::nullopt; }

void ConnectionSSL::write(const uint8_t* buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        int ret = mbedtls_ssl_write(&ssl->socket.ssl, buf + off, len - off);
        if (ret <= 0) {
            log_and_raise_tls_error("mbedtls_ssl_write failed");
        }
        off += ret;
    }
}

ReadResult ConnectionSSL::read(uint8_t* buf, size_t len) {
    int ret = mbedtls_ssl_read(&ssl->socket.ssl, buf, len);
    if (ret > 0) {
        bool would_block = ret < (int)len;
        return {would_block, (size_t)ret};
    }
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return {true, 0};
    }
    log_and_raise_tls_error("mbedtls_ssl_read failed");
    return {false, 0};
}

void ConnectionSSL::handle_connect() {
    sockaddr_in6 address;
    socklen_t len = sizeof(address);
    ssl->accept_fd = accept(ssl->fd, reinterpret_cast<sockaddr*>(&address), &len);
    if (ssl->accept_fd < 0) {
        log_and_throw("Failed to accept");
    }

    poll_manager.unregister_fd(ssl->fd);
    ::close(ssl->fd);

    mbedtls_ssl_set_bio(&ssl->socket.ssl, &ssl->accept_fd, net_send, net_recv, nullptr);

    call_if_available(event_callback, ConnectionEvent::ACCEPTED);
    poll_manager.register_fd(ssl->accept_fd, [this]() { this->handle_data(); });
}

void ConnectionSSL::handle_data() {
    if (!handshake_complete) {
        int ret = mbedtls_ssl_handshake(&ssl->socket.ssl);
        if (ret == 0) {
            handshake_complete = true;
            call_if_available(event_callback, ConnectionEvent::OPEN);
            return;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return;
        }
        char buf[64];
        mbedtls_strerror(ret, buf, sizeof(buf));
        log_and_raise_tls_error(std::string("mbedtls handshake failed: ") + buf);
    }

    call_if_available(event_callback, ConnectionEvent::NEW_DATA);
}

void ConnectionSSL::close() {
    mbedtls_ssl_close_notify(&ssl->socket.ssl);
    ::close(ssl->accept_fd);
    poll_manager.unregister_fd(ssl->accept_fd);
    call_if_available(event_callback, ConnectionEvent::CLOSED);
}

} // namespace iso15118::io
