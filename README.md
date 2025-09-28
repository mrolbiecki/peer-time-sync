# Peer Time Sync

Peer Time Sync is a coursework project for the Computer Networks class that implements peer-to-peer clock synchronization over UDP/IPv4. Each node keeps a monotonic millisecond clock, discovers and connects to peers, and elects a leader that acts as the synchronization source.

Nodes exchange HELLO/CONNECT handshakes to learn about the network and then run a three-step SYNC sequence that measures message delays and adjusts the local clock offset so participants converge on the leader’s time. Command-line options let a node choose its bind address/port or bootstrap by contacting a known peer.
