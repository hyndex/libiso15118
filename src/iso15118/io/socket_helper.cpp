// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Pionix GmbH and Contributors to EVerest
#include <iso15118/detail/io/socket_helper.hpp>

#include <cstring>
#ifdef ESP_PLATFORM
#include <port/esp32/posix_stub.hpp>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

#include <iso15118/detail/helper.hpp>

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
        if (current_if->ifa_addr == nullptr or current_if->ifa_addr->sa_family != AF_INET6) {
            continue;
        }

        // NOTE (aw): because we did the check for AF_INET6, we can assume that ifa_addr is indeed an sockaddr_in6
        const auto current_addr = reinterpret_cast<const sockaddr_in6*>(current_if->ifa_addr);
        if (not IN6_IS_ADDR_LINKLOCAL(&(current_addr->sin6_addr))) {
            continue;
        }
        interface_name = current_if->ifa_name;
        break; // Stop the loop if a interface is found
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
    return not interface_name.empty();
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

        // NOTE (aw): because we did the check for AF_INET6, we can assume that ifa_addr is indeed an sockaddr_in6
        const auto current_addr = reinterpret_cast<const sockaddr_in6*>(current_if->ifa_addr);

        // NOTE (sl): If using loopback device, accept any address. Loopback usually does not have a link local address
        if (interface_name.compare("lo") != 0 and not IN6_IS_ADDR_LINKLOCAL(&(current_addr->sin6_addr))) {
            continue;
        }

        if (interface_name == "auto") {
            logf_info("Found an ipv6 link local address for interface: %s", current_if->ifa_name);
        }

        memcpy(&address, current_addr, sizeof(address));
        found_interface = true;
        break; // Stop the loop if a interface is found
    }

    freeifaddrs(if_list_head);

    // Todo(sl): What to do if interface was not found?
    return found_interface;
}

std::unique_ptr<char[]> sockaddr_in6_to_name(const sockaddr_in6& address) {
    // account for ipv6 address string length plus possible scope/zone
    // identifier which seems to be an interface name, as both constants
    // (INET6_ADDRSTRLEN and IFNAMSIZ) include the terminating NULL, we
    // have one extra character that can account for the separating '%'
    // between the ipv6 address and the scope/zone identifier
    static constexpr auto MAX_NUMERIC_NAME_LENGTH = INET6_ADDRSTRLEN + IFNAMSIZ;
    auto name = std::make_unique<char[]>(MAX_NUMERIC_NAME_LENGTH);

    // FIXME (aw): what about alignment issues here between casting from sockaddr_in6 to sockaddr?
    const auto result = getnameinfo(reinterpret_cast<const sockaddr*>(&address), sizeof(address), name.get(),
                                    MAX_NUMERIC_NAME_LENGTH, nullptr, 0, NI_NUMERICHOST);

    if (result == 0) {
        return name;
    } else {
        return nullptr;
    }
}
} // namespace iso15118::io
