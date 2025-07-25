#include "posix_stub.hpp"
#include <iso15118/detail/io/socket_helper.hpp>
#include <cstring>
#include <netinet/in.h>

namespace iso15118::io {

namespace {

auto choose_first_ipv6_interface() {
    std::string interface_name{};
    struct ifaddrs* if_list_head;
    const auto get_if_addrs_result = getifaddrs(&if_list_head);

    if (get_if_addrs_result == -1) {
        logf_error("Failed to call getifaddrs");
        return std::string("");
    }

    for (auto current_if = if_list_head; current_if != nullptr; current_if = current_if->ifa_next) {
        if (current_if->ifa_addr == nullptr || current_if->ifa_addr->sa_family != AF_INET6) {
            continue;
        }

        const auto current_addr = reinterpret_cast<const sockaddr_in6*>(current_if->ifa_addr);
        if (!IN6_IS_ADDR_LINKLOCAL(&(current_addr->sin6_addr))) {
            continue;
        }
        interface_name = current_if->ifa_name;
        break;
    }
    freeifaddrs(if_list_head);
    return interface_name;
}

} // namespace

bool check_and_update_interface(std::string& interface_name) {
    if (interface_name == "auto") {
        logf_info("Search for the first available ipv6 interface");
        interface_name = choose_first_ipv6_interface();
    }

    struct ipv6_mreq mreq {};
    mreq.ipv6mr_interface = if_nametoindex(interface_name.c_str());
    if (!mreq.ipv6mr_interface) {
        logf_error("No such interface: %s", interface_name.c_str());
        return false;
    }
    return !interface_name.empty();
}

bool get_first_sockaddr_in6_for_interface(const std::string& interface_name, sockaddr_in6& address) {
    struct ifaddrs* if_list_head;
    const auto get_if_addrs_result = getifaddrs(&if_list_head);

    if (get_if_addrs_result == -1) {
        log_and_throw("Failed to call getifaddrs");
    }

    bool found_interface = false;

    for (auto current_if = if_list_head; current_if != nullptr; current_if = current_if->ifa_next) {
        if (current_if->ifa_addr == nullptr) {
            continue;
        }

        if (current_if->ifa_addr->sa_family != AF_INET6) {
            continue;
        }

        if (interface_name.compare("auto") != 0 && interface_name.compare(current_if->ifa_name) != 0) {
            continue;
        }

        const auto current_addr = reinterpret_cast<const sockaddr_in6*>(current_if->ifa_addr);

        if (interface_name.compare("lo") != 0 && !IN6_IS_ADDR_LINKLOCAL(&(current_addr->sin6_addr))) {
            continue;
        }

        if (interface_name == "auto") {
            logf_info("Found an ipv6 link local address for interface: %s", current_if->ifa_name);
        }

        std::memcpy(&address, current_addr, sizeof(address));
        found_interface = true;
        break;
    }

    freeifaddrs(if_list_head);

    return found_interface;
}

} // namespace iso15118::io
