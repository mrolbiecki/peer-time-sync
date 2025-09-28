#ifndef PEERS_H
#define PEERS_H

#include "common.h"

connected_peer* get_connected_peer(peers_info *peers, const struct sockaddr_in *addr);
void add_new_known_peer(peers_info *peers, const struct sockaddr_in *addr, const known_peer_state state);
known_peer_state get_peer_state_or_not_known(const peers_info *peers, const struct sockaddr_in *addr);
connected_peer* add_new_connected_peer(peers_info *peers, const struct sockaddr_in *addr);

#endif // PEERS_H
