#include "posix_stub.hpp"
#include <iso15118/io/poll_manager.hpp>
#include <algorithm>
#include <iso15118/detail/helper.hpp>

namespace iso15118::io {

PollManager::PollManager() {
    event_group = xEventGroupCreate();
}

void PollManager::register_fd(int fd, PollCallback& poll_callback) {
    registered_fds.emplace(fd, poll_callback);
}

void PollManager::unregister_fd(int fd) {
    registered_fds.erase(fd);
}

void PollManager::poll(int timeout_ms) {
    const int chunk_ms = 100;
    int elapsed = 0;
    while (true) {
        if (xEventGroupGetBits(event_group) & ABORT_BIT) {
            xEventGroupClearBits(event_group, ABORT_BIT);
            return;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        for (auto const& [fd, cb] : registered_fds) {
            FD_SET(fd, &readfds);
            if (fd > max_fd) {
                max_fd = fd;
            }
        }

        struct timeval tv;
        int wait_ms;
        if (timeout_ms < 0) {
            wait_ms = chunk_ms;
        } else {
            wait_ms = std::min(chunk_ms, timeout_ms - elapsed);
            if (wait_ms < 0) {
                return; // timeout reached
            }
        }
        tv.tv_sec = wait_ms / 1000;
        tv.tv_usec = (wait_ms % 1000) * 1000;

        int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            log_and_throw("Select failed");
        } else if (ret > 0) {
            for (auto const& [fd, cb] : registered_fds) {
                if (FD_ISSET(fd, &readfds)) {
                    cb();
                }
            }
        }

        if (timeout_ms >= 0) {
            elapsed += wait_ms;
            if (elapsed >= timeout_ms) {
                break;
            }
        }
    }
}

void PollManager::abort() {
    xEventGroupSetBits(event_group, ABORT_BIT);
}

} // namespace iso15118::io
