#include <stdlib.h>
#include <endian.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <netdb.h>

#include "messages.h"
#include "network.h"
#include "common.h"

static size_t read_addr(const uint8_t *buffer, size_t pos, struct sockaddr_in *p_addr) {
    if (pos + 7 >= BUFFER_SIZE)
        return 0;

    const uint8_t p_addr_len = buffer[pos++];
    if (p_addr_len != 4)
        return 0;

    memcpy(&p_addr->sin_addr, buffer + pos, 4);
    pos += 4;

    uint16_t port_n;
    memcpy(&port_n, buffer + pos, 2);
    p_addr->sin_port = port_n;
    p_addr->sin_family = AF_INET;

    return pos + sizeof(port_n);
}

bool read_hello_reply_message(const uint8_t *buffer, const network_info *net, const size_t len, hello_reply_message *msg) {
    if (len < 3)
        return false;

    size_t pos = 1;
    // Get the number of peers included in this message
    const uint16_t peer_count = ntohs(*(uint16_t*)(buffer + pos));
    pos += 2;

    if (peer_count > MAX_CONTACTS)
        return false;

    const size_t remaining = len - pos;
    const size_t max_contacts_by_size = remaining / 7; // each contact uses 7 bytes
    if (peer_count > max_contacts_by_size)
        return false;

    msg->count = peer_count;

    for (uint16_t i = 0; i < msg->count; i++) {
        if (pos >= len) return false;
        pos = read_addr(buffer, pos, &msg->contacts[i]);

        // Skip our own addresses to avoid circular connections
        if (is_local_address(net, &msg->contacts[i].sin_addr) &&
            msg->contacts[i].sin_port == net->my_addr.sin_port) {
            return false;
        }

        if (pos == 0) return false;
    }

    // Ensure we've read the entire message
    if (pos != len)
        return false;

    return true;
}

bool read_leader_message(const uint8_t *buffer, const size_t len, leader_message *msg) {
    if (len != 2)
        return false;

    msg->synchronized = buffer[1];
    if (msg->synchronized != 0 && msg->synchronized != 255)
        return false;

    return true;
}

bool read_sync_and_time_message(const uint8_t *buffer, size_t len, sync_and_time_message *msg) {
    if (len != 10)
        return false;

    msg->synchronized = buffer[1];
    memcpy(&msg->timestamp, buffer + 2, sizeof(msg->timestamp));
    msg->timestamp = be64toh(msg->timestamp);
    return true;
}

size_t write_hello_message(uint8_t *buffer) {
    buffer[0] = (uint8_t)HELLO;
    return 1;
}

static size_t write_addr(uint8_t *buffer, const struct sockaddr_in *addr, const size_t pos) {
    if (pos + 7 >= BUFFER_SIZE)
        return 0;
    buffer[pos] = 4;
    memcpy(buffer + pos + 1, &addr->sin_addr, 4);
    
    // Convert port to network byte order for the message
    uint16_t net_port = addr->sin_port;
    memcpy(buffer + pos + 5, &net_port, 2);
    
    return (ssize_t)pos + 7;
}

size_t write_hello_reply_message(const peers_info *peers, const struct sockaddr_in *src_addr, uint8_t *buffer) {
    size_t pos = 0;
    buffer[pos++] = HELLO_REPLY;

    // Count peers that aren't the source
    uint16_t valid_peer_count = 0;
    for (size_t i = 0; i < peers->connected_peers_count; i++) {
        const struct sockaddr_in *p_addr = &peers->connected_peers[i].addr;
        if (p_addr->sin_addr.s_addr == src_addr->sin_addr.s_addr &&
            p_addr->sin_port == src_addr->sin_port)
            continue;
        valid_peer_count++;
    }

    const uint16_t net_count = htons(valid_peer_count);
    memcpy(buffer + pos, &net_count, 2);
    pos += 2;

    for (size_t i = 0; i < peers->connected_peers_count; i++) {
        const struct sockaddr_in *p_addr = &peers->connected_peers[i].addr;

        // Skip the source address
        if (p_addr->sin_addr.s_addr == src_addr->sin_addr.s_addr &&
            p_addr->sin_port == src_addr->sin_port)
            continue;

        pos = write_addr(buffer, p_addr, pos);
        if (pos == 0)
            return 0;
    }

    return pos;
}

size_t write_connect_message(uint8_t *buffer) {
    buffer[0] = (uint8_t)CONNECT;
    return 1;
}

size_t write_ack_connect_message(uint8_t *buffer) {
    buffer[0] = (uint8_t)ACK_CONNECT;
    return 1;
}

static size_t write_timestamp_message(const sync_info *sync, uint8_t *buffer, const uint64_t time, const uint8_t message_type) {
    buffer[0] = message_type;
    buffer[1] = sync->my_sync_level;
    const uint64_t net_time = htobe64(time);
    memcpy(buffer + 2, &net_time, sizeof(net_time));

    return 10;  // 1 byte message + 1 byte sync_level + 8 bytes timestamp
}

size_t write_time_message(const sync_info *sync, uint8_t *buffer, const uint64_t time) {
    return write_timestamp_message(sync, buffer, time, TIME);
}

size_t write_sync_start_message(const sync_info *sync, uint8_t *buffer, const uint64_t timestamp) {
    return write_timestamp_message(sync, buffer, timestamp, SYNC_START);
}

size_t write_delay_request_message(uint8_t *buffer) {
    buffer[0] = (uint8_t)DELAY_REQUEST;
    return 1;
}

size_t write_sync_response_message(const sync_info *sync, uint8_t *buffer, const uint64_t timestamp) {
    return write_timestamp_message(sync, buffer, timestamp, DELAY_RESPONSE);
}
