#include "peers.h"

#include <stdio.h>
#include <arpa/inet.h>

connected_peer* get_connected_peer(peers_info *peers, const struct sockaddr_in *addr) {
    for (size_t i = 0; i < peers->connected_peers_count; i++) {
        if (peers->connected_peers[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            peers->connected_peers[i].addr.sin_port == addr->sin_port) {
            return &peers->connected_peers[i];
        }
    }
    return NULL;
}

void add_new_known_peer(peers_info *peers, const struct sockaddr_in *addr, const known_peer_state state) {
    known_peer *p = &peers->known_peers[peers->known_peers_count++];
    p->addr = *addr;
    p->connect_state = state;
}

known_peer_state get_peer_state_or_not_known(const peers_info *peers, const struct sockaddr_in *addr) {
    for (size_t i = 0; i < peers->known_peers_count; i++) {
        if (peers->known_peers[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            peers->known_peers[i].addr.sin_port == addr->sin_port) {
            return peers->known_peers[i].connect_state;
        }
    }
    return PS_NOT_KNOWN;
}

connected_peer* add_new_connected_peer(peers_info *peers, const struct sockaddr_in *addr) {
    // First remove this peer from known_peers if it exists there
    uint16_t i = 0;
    for (; i < peers->known_peers_count; i++) {
        if (peers->known_peers[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            peers->known_peers[i].addr.sin_port == addr->sin_port) {
            peers->known_peers_count--;
            break;
        }
    }

    // Shift remaining known_peers to fill the gap
    for (; i < peers->known_peers_count; i++)
        peers->known_peers[i] = peers->known_peers[i + 1];

    connected_peer *p = &peers->connected_peers[peers->connected_peers_count++];
    p->addr = *addr;
    p->sync_state = PSS_IDLE;
    p->sync_start_time = 0;
    return p;
}
