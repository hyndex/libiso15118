#include "posix_stub.hpp"
#include <iso15118/io/connection_plain.hpp>
#include <iso15118/detail/io/socket_helper.hpp>
#include <iso15118/detail/helper.hpp>

namespace iso15118::io {

ConnectionPlain::ConnectionPlain(PollManager& poll_manager_, const std::string& interface_name)
    : poll_manager(poll_manager_) {
    sockaddr_in6 address;
    if (!get_first_sockaddr_in6_for_interface(interface_name, address)) {
        log_and_throw("Failed to get ipv6 socket address for interface " + interface_name);
    }
    end_point.port = 50000;
    memcpy(&end_point.address, &address.sin6_addr, sizeof(address.sin6_addr));

    fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd == -1) {
        log_and_throw("Failed to create an ipv6 socket");
    }

    address.sin6_port = htobe16(end_point.port);
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    if (bind(fd, reinterpret_cast<const struct sockaddr*>(&address), sizeof(address)) == -1) {
        log_and_throw("Failed to bind ipv6 socket to interface " + interface_name);
    }

    if (listen(fd, 4) == -1) {
        log_and_throw("Listen on socket failed");
    }

    poll_manager.register_fd(fd, [this]() { this->handle_connect(); });
}

ConnectionPlain::~ConnectionPlain() = default;

void ConnectionPlain::set_event_callback(const ConnectionEventCallback& cb) { event_callback = cb; }

Ipv6EndPoint ConnectionPlain::get_public_endpoint() const { return end_point; }

void ConnectionPlain::write(const uint8_t* buf, size_t len) {
    if (::write(fd, buf, len) != (ssize_t)len) {
        log_and_throw("write failed");
    }
}

ReadResult ConnectionPlain::read(uint8_t* buf, size_t len) {
    int ret = ::read(fd, buf, len);
    if (ret >= 0) {
        bool would_block = ret < (int)len;
        return {would_block, (size_t)ret};
    }
    return {true, 0};
}

void ConnectionPlain::handle_connect() {
    sockaddr_in6 address;
    socklen_t len = sizeof(address);
    int accept_fd = accept(fd, reinterpret_cast<sockaddr*>(&address), &len);
    if (accept_fd < 0) {
        log_and_throw("Failed to accept");
    }

    poll_manager.unregister_fd(fd);
    ::close(fd);

    call_if_available(event_callback, ConnectionEvent::ACCEPTED);
    connection_open = true;
    call_if_available(event_callback, ConnectionEvent::OPEN);

    fd = accept_fd;
    poll_manager.register_fd(fd, [this]() { this->handle_data(); });
}

void ConnectionPlain::handle_data() { call_if_available(event_callback, ConnectionEvent::NEW_DATA); }

void ConnectionPlain::close() {
    shutdown(fd, SHUT_RDWR);
    ::close(fd);
    poll_manager.unregister_fd(fd);
    connection_open = false;
    call_if_available(event_callback, ConnectionEvent::CLOSED);
}

} // namespace iso15118::io
