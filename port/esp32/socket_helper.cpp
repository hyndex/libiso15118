#include "posix_stub.hpp"
#include <iso15118/detail/io/socket_helper.hpp>
#include <esp_netif.h>

namespace iso15118::io {

bool check_and_update_interface(std::string& name) {
    if (name == "auto") {
        name = "STA"; // default Wi-Fi interface
    }
    return true;
}

bool get_first_sockaddr_in6_for_interface(const std::string& if_name, sockaddr_in6& dst) {
    esp_netif_ip_info_t info;
    // For demo: query link local address of default interface
    auto netif = esp_netif_get_handle_from_ifkey(if_name.c_str());
    if (!netif) {
        return false;
    }
    if (esp_netif_get_ip6_linklocal(netif, &info.ip) != ESP_OK) {
        return false;
    }
    memset(&dst, 0, sizeof(dst));
    dst.sin6_family = AF_INET6;
    memcpy(&dst.sin6_addr, &info.ip6.addr, sizeof(dst.sin6_addr));
    return true;
}

} // namespace iso15118::io
