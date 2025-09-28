#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"

#define BUFFER_SIZE 65536
#define MAX_LOCAL_ADDRESSES 32

struct sockaddr_in new_address(uint16_t port, const char *address);

void network_init(network_info *net, const char *bind_address, uint16_t port);

bool is_local_address(const network_info *net, const struct in_addr *addr);

void network_close(network_info *net);

void send_message(const network_info *net, size_t len, const struct sockaddr_in *dst_addr);

ssize_t receive_message(network_info *net, struct sockaddr_in *src_addr);

#endif // NETWORK_H
