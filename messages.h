#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdbool.h>
#include "common.h"

/* Message type codes */
#define HELLO           1
#define HELLO_REPLY     2
#define CONNECT         3
#define ACK_CONNECT     4

#define SYNC_START     11
#define DELAY_REQUEST  12
#define DELAY_RESPONSE 13

#define LEADER         21

#define GET_TIME       31
#define TIME           32

#define MAX_CONTACTS 1000

typedef struct {
    uint16_t count;
    struct sockaddr_in contacts[MAX_CONTACTS];
} hello_reply_message;

typedef struct {
    uint8_t synchronized;
} leader_message;

typedef struct {
    uint8_t synchronized;
    uint64_t timestamp;
} sync_and_time_message;

// Read functions
bool read_hello_reply_message(const uint8_t *buffer, const network_info *net, const size_t len, hello_reply_message *msg);
bool read_leader_message(const uint8_t *buffer, const size_t len, leader_message *msg);
bool read_sync_and_time_message(const uint8_t *buffer, size_t len, sync_and_time_message *msg);

// Write functions
size_t write_hello_message(uint8_t *buffer);
size_t write_hello_reply_message(const peers_info *peers, const struct sockaddr_in *src_addr, uint8_t *buffer);
size_t write_connect_message(uint8_t *buffer);
size_t write_ack_connect_message(uint8_t *buffer);
size_t write_time_message(const sync_info *sync, uint8_t *buffer, const uint64_t time);
size_t write_sync_start_message(const sync_info *sync, uint8_t *buffer, const uint64_t timestamp);
size_t write_delay_request_message(uint8_t *buffer);
size_t write_sync_response_message(const sync_info *sync, uint8_t *buffer, const uint64_t timestamp);

#endif /* MESSAGES_H */
