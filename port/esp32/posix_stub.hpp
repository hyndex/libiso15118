#pragma once
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <esp_netif.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

// Provide minimal POSIX compatibility wrappers

struct ifaddrs {
    struct ifaddrs* ifa_next;
    char* ifa_name;
    struct sockaddr* ifa_addr;
};

inline int getifaddrs(struct ifaddrs**){ return -1; }
inline void freeifaddrs(struct ifaddrs*){}

