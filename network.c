#include "network.h"
#include "err.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

struct sockaddr_in new_address(const uint16_t port, const char *address) {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);  // Convert to network byte order for socket API
    addr.sin_addr.s_addr = address ? inet_addr(address) : INADDR_ANY;
    return addr;
}

// Function to check if an address is one of our local addresses
bool is_own_address(const network_info *net, const struct sockaddr_in *addr) {
    // First check if it matches our bound address - compare in host byte order
    if (ntohs(net->my_addr.sin_port) == addr->sin_port) {
        // If we bound to INADDR_ANY (0.0.0.0), check all interfaces
        // This is important because when binding to INADDR_ANY, we need to
        // compare against all local IPs to avoid talking to ourselves
        if (net->my_addr.sin_addr.s_addr == INADDR_ANY ||
            net->my_addr.sin_addr.s_addr == addr->sin_addr.s_addr) {

            struct ifaddrs *ifaddr;
            bool is_own = false;

            if (getifaddrs(&ifaddr) == -1) {
                error("getifaddrs failed");
                // If we can't check, assume it's not own to avoid blocking legitimate traffic
                return false;
            }

            // Walk through linked list, looking for AF_INET interfaces
            for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL)
                    continue;

                if (ifa->ifa_addr->sa_family == AF_INET) {
                    struct sockaddr_in *local_addr = (struct sockaddr_in *)ifa->ifa_addr;

                    // Check if the address matches any of our interfaces
                    if (local_addr->sin_addr.s_addr == addr->sin_addr.s_addr) {
                        is_own = true;
                        break;
                    }
                }
            }

            freeifaddrs(ifaddr);
            return is_own;
        }
    }

    return false;
}

// Get all local addresses of this host
void get_local_addresses(network_info *net) {
    struct ifaddrs *ifaddr, *ifa;

    net->local_addr_count = 0;

    if (getifaddrs(&ifaddr) == -1) {
        syserr("getifaddrs failed");
        return;
    }

    // First pass: count how many IPv4 addresses we have
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET) {
            net->local_addr_count++;
        }
    }

    // Allocate memory for addresses
    net->local_addresses = malloc(net->local_addr_count * sizeof(struct in_addr));
    if (net->local_addresses == NULL)
        syserr("failed to allocate memory for local addresses");

    // Second pass: store the addresses
    int i = 0;
    for (ifa = ifaddr; ifa != NULL && i < net->local_addr_count; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            net->local_addresses[i++] = sa->sin_addr;
        }
    }

    freeifaddrs(ifaddr);
}

void network_init(network_info *net, const char *bind_address, uint16_t port) {
    net->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (net->socket_fd < 0)
        syserr("creating socket");

    const int flags = fcntl(net->socket_fd, F_GETFL, 0);
    if (flags == -1)
        syserr("getting socket flags");

    if (fcntl(net->socket_fd, F_SETFL, flags | O_NONBLOCK) == -1)
        syserr("setting socket to non-blocking");

    net->my_addr = new_address(port, bind_address);

    if (bind(net->socket_fd, (struct sockaddr *)&net->my_addr, sizeof(net->my_addr)) < 0)
        syserr("binding socket");

    // Initialize local addresses
    net->local_addresses = NULL;
    net->local_addr_count = 0;
    get_local_addresses(net);
}

void network_close(network_info *net) {
    if (net->socket_fd >= 0)
        close(net->socket_fd);
    net->socket_fd = -1;

    // Free allocated memory for local addresses
    if (net->local_addresses != NULL) {
        free(net->local_addresses);
        net->local_addresses = NULL;
    }
    net->local_addr_count = 0;
}

// Check if an address is one of our local addresses
bool is_local_address(const network_info *net, const struct in_addr *addr) {
    for (int i = 0; i < net->local_addr_count; i++) {
        if (net->local_addresses[i].s_addr == addr->s_addr) {
            return true;
        }
    }
    return false;
}

void send_message(const network_info *net, const size_t len, const struct sockaddr_in *dst_addr) {
    // Don't send messages to our own addresses
    if (is_own_address(net, dst_addr)) {
        error("attempted to send message to own address: %s:%d",
              inet_ntoa(dst_addr->sin_addr), dst_addr->sin_port);
        return;
    }

    ssize_t sent = sendto(net->socket_fd, net->buffer, len, 0, (struct sockaddr *)dst_addr, sizeof(*dst_addr));
    if (sent < 0)
        error("sending message");
}

ssize_t receive_message(network_info *net, struct sockaddr_in *src_addr) {
    socklen_t addr_len = sizeof(*src_addr);
    const ssize_t received = recvfrom(net->socket_fd, net->buffer, BUFFER_SIZE, 0,
                                      (struct sockaddr *)src_addr, &addr_len);
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            error("receiving message");
        return 0;
    }

    // Ignore messages from our own addresses
    if (is_own_address(net, src_addr)) {
        error("ignored message from own address: %s:%d\n",
               inet_ntoa(src_addr->sin_addr), src_addr->sin_port);
        return 0;
    }

    return received;
}

