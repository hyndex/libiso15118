#pragma once
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <esp_netif.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

// Provide minimal POSIX compatibility wrappers

struct ifaddrs {
    struct ifaddrs* ifa_next;
    char* ifa_name;
    struct sockaddr* ifa_addr;
};

inline unsigned if_nametoindex(const char* ifname) {
    auto netif = esp_netif_get_handle_from_ifkey(ifname);
    if (!netif) {
        return 0;
    }
    return esp_netif_get_netif_impl_index(netif);
}

inline int getifaddrs(struct ifaddrs** ifap) {
    if (!ifap) {
        return -1;
    }

    struct ifaddrs* head = nullptr;
    struct ifaddrs** next = &head;
    esp_netif_t* netif = nullptr;

    while ((netif = esp_netif_next(netif)) != nullptr) {
        auto entry = static_cast<ifaddrs*>(std::calloc(1, sizeof(ifaddrs)));
        if (!entry) {
            freeifaddrs(head);
            return -1;
        }

        const char* key = esp_netif_get_ifkey(netif);
        if (key) {
            entry->ifa_name = strdup(key);
        }

        esp_ip6_addr_t ip6{};
        if (esp_netif_get_ip6_linklocal(netif, &ip6) == ESP_OK) {
            auto addr6 = static_cast<sockaddr_in6*>(std::calloc(1, sizeof(sockaddr_in6)));
            if (!addr6) {
                free(entry);
                freeifaddrs(head);
                return -1;
            }
            addr6->sin6_family = AF_INET6;
            std::memcpy(&addr6->sin6_addr, &ip6, sizeof(ip6));
            entry->ifa_addr = reinterpret_cast<sockaddr*>(addr6);
        } else {
            esp_netif_ip_info_t ip{};
            if (esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
                auto addr4 = static_cast<sockaddr_in*>(std::calloc(1, sizeof(sockaddr_in)));
                if (!addr4) {
                    free(entry);
                    freeifaddrs(head);
                    return -1;
                }
                addr4->sin_family = AF_INET;
                addr4->sin_addr.s_addr = ip.ip.addr;
                entry->ifa_addr = reinterpret_cast<sockaddr*>(addr4);
            }
        }

        *next = entry;
        next = &entry->ifa_next;
    }

    *ifap = head;
    return 0;
}

inline void freeifaddrs(struct ifaddrs* ifa) {
    while (ifa) {
        auto next = ifa->ifa_next;
        std::free(ifa->ifa_name);
        std::free(ifa->ifa_addr);
        std::free(ifa);
        ifa = next;
    }
}

