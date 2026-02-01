#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;
using u64 = uint64_t;

const u32 MTU = 1200;

enum class PacketType : uint8_t {
  HANDSHAKE = 0,
  HANDSHAKE_ACK,
  DATA,
  ACK,
  CLOSE
};

struct Header {
  PacketType type;
  u32 conn_id;
  u64 seq;
  u64 ack;
  u64 ack_bits;
  u64 timestamp_ns;
};

class Packet {
public:
  Header header;
  std::vector<u8> payload;
  size_t size() const { return sizeof(Header) + payload.size(); }
  Packet() {};
};

class Connection {
  u64 seq_to_send;
  u16 longest_contiguous_sequence;
  std::chrono::nanoseconds rtt_smoothed;
  std::chrono::nanoseconds rtt_variance;
  u64 congestion_window;
  u64 slow_start_threshold;
  u64 inflight_bytes;
  std::unordered_map<u64, std::shared_ptr<Packet>> inflight_tracker;
  std::queue<std::shared_ptr<Packet>> send_queue;

  Connection()
      : seq_to_send(0), longest_contiguous_sequence(0),
        rtt_smoothed(std::chrono::nanoseconds(0)),
        rtt_variance(std::chrono::nanoseconds(0)), congestion_window(12000),
        slow_start_threshold() {};

public:
  static Connection &init_handshake() {
    static Connection c;
    return c;
    // talk to server, ask for conn_id
    // read conn_id from server and send with every subsequent packet
  }
};

// bytes_in_flight ≤ cwnd must always hold
