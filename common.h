#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <netdb.h>
#include <stdnoreturn.h>

#define BUFFER_SIZE 65536
#define MAX_PEERS_SIZE 65536

// Synchronization level constants
#define SYNC_LEVEL_LEADER 0
#define SYNC_LEVEL_UNSYNCHRONIZED 255
#define SYNC_LEVEL_MAX 254

// Time constants
#define SECOND 1000 // Number of milliseconds in a second

// Timeout constants
#define SYNC_INTERVAL 5000      // Time between SYNC_START messages (5 seconds)
#define LEADER_START_DELAY 2000 // Wait time before leader starts synchronizing (2 seconds)
#define SYNC_TIMEOUT 20000      // Time after which to reset sync if no updates (20 seconds)
#define SYNC_RESPONSE_TIMEOUT 5000 // Time to wait for SYNC_START or DELAY_REQUEST response (5 seconds)

typedef enum {
    PS_NOT_KNOWN = 0,
    PS_WAITING_FOR_HELLO_REPLY,
    PS_WAITING_FOR_ACK_CONNECT,
} known_peer_state;

typedef enum {
    PSS_IDLE = 0,                  // Not currently syncing
    PSS_WAITING_FOR_DELAY_REQUEST, // Sent SYNC_START, waiting for DELAY_REQUEST
} connected_peer_state;

typedef enum {
    SS_IDLE = 0,                  // Not currently syncing
    SS_WAITING_FOR_DELAY_RESPONSE // Received SYNC_START and decided to sync, waiting for DELAY_RESPONSE
} node_sync_state;

typedef struct {
    struct sockaddr_in addr;
    known_peer_state connect_state;
} known_peer;

typedef struct {
    struct sockaddr_in addr;
    connected_peer_state sync_state;
    uint64_t sync_start_time;    // last time we heard from this peer
} connected_peer;

typedef struct {
    int64_t offset;
    uint64_t start_time;
} time_info;

typedef struct {
    int socket_fd;
    struct sockaddr_in my_addr;
    uint8_t buffer[BUFFER_SIZE];
    struct in_addr *local_addresses;  // Array of local IP addresses
    int local_addr_count;             // Number of local IP addresses
} network_info;

typedef struct {
    uint16_t known_peers_count, connected_peers_count;
    known_peer known_peers[MAX_PEERS_SIZE];
    connected_peer connected_peers[MAX_PEERS_SIZE];
} peers_info;

typedef struct {
    bool is_leader, first_leader_sync;
    uint8_t my_sync_level, syncing_peer_sync_level;
    connected_peer *synced_with, *syncing_with;
    node_sync_state sync_state;
    uint64_t start_sync_send_time, last_sync_time, leader_start_time, t1, t2, t3, t4;
} sync_info;

typedef struct {
    time_info time;
    network_info network;
    peers_info peers;
    sync_info sync;
} context_info;

// Print information about a system error and quits.
noreturn void syserr(const char* fmt, ...);

// Print information about an error and quits.
noreturn void fatal(const char* fmt, ...);

// Print information about an error and return.
void error(const char* fmt, ...);

// Print information about an invalid message error
void error_msg(const network_info *net, size_t len);

// Initialize the timer
void init_timer(time_info *time);

// Get current time in milliseconds
uint64_t get_time(const time_info *time);

// Get synchronized time in milliseconds
uint64_t get_synced_time(const time_info *time);

// Set the time offset for synchronization
void set_time_offset(time_info *time, const int64_t new_offset);

#endif // COMMON_H
