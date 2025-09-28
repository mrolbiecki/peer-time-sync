#include <errno.h>
#include <sys/types.h>
#include <inttypes.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"
#include "network.h"
#include "protocol.h"
#include "messages.h"

void read_parameters(int argc, char *argv[], char **address, uint16_t *port, char **peer_address, uint16_t *peer_port);
bool is_time_for_sync(sync_info *sync, uint64_t current_time);
void install_int_handler();

static bool finish = false;
static context_info ctx;

int main(int argc, char *argv[]) {
    install_int_handler();

    // Initialize time info
    ctx.time = (time_info){
        .offset = 0,
        .start_time = 0
    };
    init_timer(&ctx.time);

    // Initialize peers info
    ctx.peers = (peers_info){
        .known_peers_count = 0,
        .connected_peers_count = 0
    };

    // Initialize sync info
    ctx.sync = (sync_info){
        .is_leader = false,
        .first_leader_sync = false,
        .my_sync_level = SYNC_LEVEL_UNSYNCHRONIZED,
        .syncing_peer_sync_level = SYNC_LEVEL_UNSYNCHRONIZED,
        .synced_with = NULL,
        .syncing_with = NULL,
        .sync_state = SS_IDLE,
        .leader_start_time = 0,
        .start_sync_send_time = 0,
        .last_sync_time = 0
    };

    // Initialize network info
    ctx.network = (network_info){
        .socket_fd = -1,
        .my_addr = {0}
    };

    char *bind_address = NULL, *peer_address = NULL;
    uint16_t port = 0, peer_port = 0;
    read_parameters(argc, argv, &bind_address, &port, &peer_address, &peer_port);
    network_init(&ctx.network, bind_address, port);

    if (peer_address != NULL) {
        const struct sockaddr_in first_peer_addr = new_address(peer_port, peer_address);
        send_hello_message(&ctx, &first_peer_addr);
    }

    while (!finish) {
        handle_message(&ctx);

        uint64_t current_time = get_time(&ctx.time);

        if (is_time_for_sync(&ctx.sync, current_time))
            start_sync(&ctx);

        if (!ctx.sync.is_leader && current_time - ctx.sync.last_sync_time > SYNC_TIMEOUT)
            reset_sync_state(&ctx);

        if (ctx.sync.sync_state == SS_WAITING_FOR_DELAY_RESPONSE && current_time - ctx.sync.t3 > SYNC_RESPONSE_TIMEOUT) {
            reset_sync_process(&ctx);
        }
    }

    network_close(&ctx.network);
    return 0;
}

bool is_time_for_sync(sync_info *sync, uint64_t current_time) {
    // Regular sync interval has passed since last sync attempt
    if (!sync->first_leader_sync && current_time - sync->start_sync_send_time > SYNC_INTERVAL)
        return true;

    // Special case: first sync after becoming leader
    if (sync->is_leader && sync->first_leader_sync &&
        current_time - sync->leader_start_time > LEADER_START_DELAY) {
        sync->first_leader_sync = false;
        return true;
    }

    return false;
}

uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    const unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port > UINT16_MAX)
        fatal("%s is not a valid port number", string);
    return port;
}

void read_parameters(int argc, char *argv[], char **address, uint16_t *port, char **peer_address, uint16_t *peer_port) {
    bool a_arg = false, r_arg = false;
    int opt;
    while ((opt = getopt(argc, argv, "b:p:a:r:")) != -1) {
        switch (opt) {
            case 'b':
                *address = optarg;
            break;
            case 'p':
                *port = read_port(optarg);
            break;
            case 'a':
                a_arg = true;
            *peer_address = optarg;
            break;
            case 'r':
                r_arg = true;
            *peer_port = read_port(optarg);
            break;
            default:
                fatal("Usage: %s [-b bind_address] [-p port] [-a peer_address] [-r peer_port]", argv[0]);
        }
    }

    if (a_arg != r_arg)
        fatal("Both -a and -r must be provided together");
}

static void catch_int() {
    finish = true;
}

void install_int_handler() {
    struct sigaction action;
    sigset_t block_mask;

    sigemptyset(&block_mask);
    action.sa_handler = catch_int;
    action.sa_mask = block_mask;
    action.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &action, NULL) < 0) {
        syserr("installing signal handler");
    }
}
