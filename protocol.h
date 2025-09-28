#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

// Initiate synchronization with all connected peers
void start_sync(context_info *ctx);

// Reset synchronization state
void reset_sync_state(context_info *ctx);

// Reset synchronization process
void reset_sync_process(context_info *ctx);

// Send a hello message to initiate connection with a peer
void send_hello_message(context_info *ctx, const struct sockaddr_in *peer_addr);

// Handle incoming messages
void handle_message(context_info *ctx);

#endif // PROTOCOL_H
