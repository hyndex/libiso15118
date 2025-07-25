#include <iso15118/config.hpp>
#include <iso15118/io/connection_ssl.hpp>
#include <iso15118/io/logging.hpp>
#include <iso15118/io/poll_manager.hpp>
#include <iso15118/io/time.hpp>

#include <iostream>

using namespace iso15118;

int main() {
    io::set_logging_callback([](LogLevel, const std::string& msg) { std::cout << msg << std::endl; });
    auto poll_manager = io::PollManager();

    config::SSLConfig ssl{config::CertificateBackend::EVEREST_LAYOUT,
                          {},
                          "pki/certs/client/cso/CPO_CERT_CHAIN.pem",
                          "pki/certs/client/cso/SECC_LEAF.key",
                          "123456",
                          "pki/certs/ca/v2g/V2G_ROOT_CA.pem",
                          "pki/certs/ca/oem/OEM_ROOT_CA.pem",
                          false,
                          false,
                          false,
                          "/tmp"};

    auto connection = io::ConnectionSSL(poll_manager, "STA", ssl);
    connection.set_event_callback([](io::ConnectionEvent){});
    poll_manager.poll(10);
    connection.close();
    return 0;
}
