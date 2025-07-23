// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Pionix GmbH and Contributors to EVerest
#pragma once

#include <functional>
#include <map>
#include <vector>

#ifndef ESP_PLATFORM
#include <poll.h>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <sys/select.h>
#endif
namespace iso15118::io {

using PollCallback = const std::function<void()>;

#ifndef ESP_PLATFORM
struct PollSet {
    std::vector<struct pollfd> fds;
    std::vector<PollCallback*> callbacks;
};
#endif

class PollManager {
public:
    PollManager();

    void register_fd(int fd, PollCallback& poll_callback);
    void unregister_fd(int fd);

    void poll(int timeout_ms);
    void abort();

private:
    std::map<int, PollCallback> registered_fds;

    
#ifndef ESP_PLATFORM
    PollSet poll_set;
    int event_fd{-1};
#else
    EventGroupHandle_t event_group{nullptr};
    static constexpr EventBits_t ABORT_BIT = BIT0;
#endif
};

} // namespace iso15118::io
