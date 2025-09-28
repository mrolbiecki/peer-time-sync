#include <stdio.h>

#include "protocol.h"

#include <arpa/inet.h>

#include "peers.h"
#include "messages.h"
#include "network.h"
#include "common.h"

// Send SYNC_START to all connected peers
void start_sync(context_info *ctx) {
    ctx->sync.start_sync_send_time = get_time(&ctx->time);
    if (ctx->sync.my_sync_level > SYNC_LEVEL_MAX) return;
    for (size_t i = 0; i < ctx->peers.connected_peers_count; i++) {
        connected_peer *p = &ctx->peers.connected_peers[i];
        p->sync_state = PSS_WAITING_FOR_DELAY_REQUEST;
        size_t msg_len = write_sync_start_message(&ctx->sync, ctx->network.buffer, get_synced_time(&ctx->time));
        send_message(&ctx->network, msg_len, &p->addr);
    }
}

// Reset sync state to unsynchronized
void reset_sync_state(context_info *ctx) {
    ctx->sync.my_sync_level = SYNC_LEVEL_UNSYNCHRONIZED;
    ctx->sync.synced_with = NULL;
    set_time_offset(&ctx->time, 0);
    ctx->sync.last_sync_time = get_time(&ctx->time);
}

// Reset sync process
void reset_sync_process(context_info *ctx) {
    ctx->sync.sync_state = SS_IDLE;
    ctx->sync.syncing_with = NULL;
    ctx->sync.syncing_peer_sync_level = SYNC_LEVEL_UNSYNCHRONIZED;
}

// Send HELLO message to start discovering peers
void send_hello_message(context_info *ctx, const struct sockaddr_in *peer_addr) {
    add_new_known_peer(&ctx->peers, peer_addr, PS_WAITING_FOR_HELLO_REPLY);
    size_t msg_len = write_hello_message(ctx->network.buffer);
    send_message(&ctx->network, msg_len, peer_addr);
}

// Process HELLO message from peer
void handle_hello_message(context_info *ctx, const struct sockaddr_in *src_addr, const size_t message_len) {
    if (message_len != 1 || get_connected_peer(&ctx->peers, src_addr) != NULL)
        return error_msg(&ctx->network, message_len);

    add_new_connected_peer(&ctx->peers, src_addr);
    size_t msg_len = write_hello_reply_message(&ctx->peers, src_addr, ctx->network.buffer);
    send_message(&ctx->network, msg_len, src_addr);
}

// Process HELLO_REPLY message
void handle_hello_reply_message(context_info *ctx, const struct sockaddr_in *src_addr, const size_t message_len) {
    if (get_peer_state_or_not_known(&ctx->peers, src_addr) != PS_WAITING_FOR_HELLO_REPLY)
        return error_msg(&ctx->network, message_len);

    hello_reply_message msg;
    if (!read_hello_reply_message(ctx->network.buffer, &ctx->network, message_len, &msg))
        return error_msg(&ctx->network, message_len);

    add_new_connected_peer(&ctx->peers, src_addr);

    // Send CONNECT to each peer we learn about in the HELLO_REPLY message
    const size_t written = write_connect_message(ctx->network.buffer);
    for (size_t i = 0; i < msg.count; i++) {
        if (get_connected_peer(&ctx->peers, &msg.contacts[i]) == NULL) {
            add_new_known_peer(&ctx->peers, &msg.contacts[i], PS_WAITING_FOR_ACK_CONNECT);
            send_message(&ctx->network, written, &msg.contacts[i]);
        }
    }
}

// Process CONNECT message
void handle_connect_message(context_info *ctx, const struct sockaddr_in *src_addr, const size_t message_len) {
    if (message_len != 1 || get_connected_peer(&ctx->peers, src_addr) != NULL)
        return error_msg(&ctx->network, message_len);

    add_new_connected_peer(&ctx->peers, src_addr);
    size_t msg_len = write_ack_connect_message(ctx->network.buffer);
    send_message(&ctx->network, msg_len, src_addr);
}

// Process ACK_CONNECT message
void handle_ack_connect_message(context_info *ctx, const struct sockaddr_in *src_addr, const size_t message_len) {
    if (message_len != 1 || get_peer_state_or_not_known(&ctx->peers, src_addr) != PS_WAITING_FOR_ACK_CONNECT)
        return error_msg(&ctx->network, message_len);

    add_new_connected_peer(&ctx->peers, src_addr);
}

// Process LEADER message
void handle_leader_message(context_info *ctx, const size_t message_len) {
    leader_message msg;
    if (!read_leader_message(ctx->network.buffer, message_len, &msg))
        return error_msg(&ctx->network, message_len);

    if (msg.synchronized == SYNC_LEVEL_LEADER) {
        ctx->sync.my_sync_level = SYNC_LEVEL_LEADER;
        ctx->sync.leader_start_time = get_time(&ctx->time);
        ctx->sync.is_leader = true;
        ctx->sync.first_leader_sync = true;
    } else if (msg.synchronized == SYNC_LEVEL_UNSYNCHRONIZED && ctx->sync.is_leader) {
        ctx->sync.my_sync_level = SYNC_LEVEL_UNSYNCHRONIZED;
        ctx->sync.is_leader = false;
    } else
        error_msg(&ctx->network, message_len);
}

// Process GET_TIME message
void handle_get_time_message(context_info *ctx, const struct sockaddr_in *src_addr, const size_t message_len) {
    if (message_len != 1)
        return error_msg(&ctx->network, message_len);

    size_t msg_len = write_time_message(&ctx->sync, ctx->network.buffer, get_synced_time(&ctx->time));
    send_message(&ctx->network, msg_len, src_addr);
}

// Process SYNC_START message
void handle_sync_start_message(context_info *ctx, const struct sockaddr_in *src_addr, const size_t message_len, const uint64_t receive_time) {
    // Only handle sync requests when idle
    if (ctx->sync.sync_state != SS_IDLE)
        return error_msg(&ctx->network, message_len);

    connected_peer *p = get_connected_peer(&ctx->peers, src_addr);
    if (p == NULL)
        return error_msg(&ctx->network, message_len);

    sync_and_time_message msg;
    if (!read_sync_and_time_message(ctx->network.buffer, message_len, &msg))
        return error_msg(&ctx->network, message_len);

    // Only sync with a peer if their sync level is appropriate:
    // - If already synced with this peer, we need higher sync level than them
    // - If syncing with a new peer, we need higher sync level than theirs + 1
    if ((p == ctx->sync.synced_with && ctx->sync.my_sync_level <= msg.synchronized)
        || (p != ctx->sync.synced_with && ctx->sync.my_sync_level <= msg.synchronized + 1))
        return; //ignore this message

    // Start the synchronization process: t1,t2 are used in delay calculation
    ctx->sync.syncing_with = p;
    ctx->sync.sync_state = SS_WAITING_FOR_DELAY_RESPONSE;
    ctx->sync.syncing_peer_sync_level = msg.synchronized;
    ctx->sync.t1 = msg.timestamp;
    ctx->sync.t2 = receive_time;
    ctx->sync.t3 = get_time(&ctx->time);
    size_t msg_len = write_delay_request_message(ctx->network.buffer);
    send_message(&ctx->network, msg_len, src_addr);
}

// Process DELAY_REQUEST message
void handle_delay_request_message(context_info *ctx, const struct sockaddr_in *src_addr, const size_t message_len, uint64_t synced_receive_time) {
    if (message_len != 1)
        return error_msg(&ctx->network, message_len);

    connected_peer *p = get_connected_peer(&ctx->peers, src_addr);
    if (p == NULL || p->sync_state != PSS_WAITING_FOR_DELAY_REQUEST)
        return error_msg(&ctx->network, message_len);

    // Mark this peer as done with sync and respond with our timestamp
    p->sync_state = PSS_IDLE;
    size_t msg_len = write_sync_response_message(&ctx->sync, ctx->network.buffer, synced_receive_time);
    send_message(&ctx->network, msg_len, src_addr);
}

// Process DELAY_RESPONSE message and calculate time offset
void handle_delay_response_message(context_info *ctx, const struct sockaddr_in *src_addr, const size_t message_len) {
    if (ctx->sync.sync_state != SS_WAITING_FOR_DELAY_RESPONSE)
        return error_msg(&ctx->network, message_len);

    connected_peer *p = get_connected_peer(&ctx->peers, src_addr);
    if (p == NULL || ctx->sync.syncing_with != p)
        return error_msg(&ctx->network, message_len);

    sync_and_time_message msg;
    if (!read_sync_and_time_message(ctx->network.buffer, message_len, &msg))
        return error_msg(&ctx->network, message_len);

    if (msg.synchronized != ctx->sync.syncing_peer_sync_level) {
        if (ctx->sync.syncing_with == ctx->sync.synced_with)
            reset_sync_state(ctx);
        reset_sync_process(ctx);
        return error_msg(&ctx->network, message_len);
    }

    // Complete synchronization by calculating the clock offset
    // t1: Their timestamp when they sent SYNC_START
    // t2: Our timestamp when we received SYNC_START
    // t3: Our timestamp when sending DELAY_REQUEST
    // t4: Their timestamp when they received DELAY_REQUEST
    // The time offset formula accounts for network delay in both directions:
    // offset = ((t2 - t1) + (t3 - t4)) / 2
    ctx->sync.t4 = msg.timestamp;
    ctx->sync.my_sync_level = msg.synchronized + 1;
    ctx->sync.last_sync_time = get_time(&ctx->time);
    set_time_offset(&ctx->time, ((int64_t)(ctx->sync.t2 - ctx->sync.t1) + (int64_t)(ctx->sync.t3 - ctx->sync.t4)) / 2);
    ctx->sync.synced_with = p;
    ctx->sync.sync_state = SS_IDLE;
}

// Main message handling function - dispatches messages by type
void handle_message(context_info *ctx) {
    struct sockaddr_in src_addr;
    const size_t message_len = receive_message(&ctx->network, &src_addr);

    if (message_len < 1) return;

    const uint8_t msg_type = ctx->network.buffer[0];

    switch (msg_type) {
        case HELLO:
            handle_hello_message(ctx, &src_addr, message_len);
            break;

        case HELLO_REPLY:
            handle_hello_reply_message(ctx, &src_addr, message_len);
            break;

        case CONNECT:
            handle_connect_message(ctx, &src_addr, message_len);
            break;

        case ACK_CONNECT:
            handle_ack_connect_message(ctx, &src_addr, message_len);
            break;

        case SYNC_START:
            handle_sync_start_message(ctx, &src_addr, message_len, get_time(&ctx->time));
            break;

        case DELAY_REQUEST:
            handle_delay_request_message(ctx, &src_addr, message_len, get_synced_time(&ctx->time));
            break;

        case DELAY_RESPONSE:
            handle_delay_response_message(ctx, &src_addr, message_len);
            break;

        case LEADER:
            handle_leader_message(ctx, message_len);
            break;

        case GET_TIME:
            handle_get_time_message(ctx, &src_addr, message_len);
            break;

        default:
            error_msg(&ctx->network, message_len);
            break;
    }
}
