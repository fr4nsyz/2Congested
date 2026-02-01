#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <unordered_map>
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

class Header {
  PacketType type;
  u32 conn_id;
  u64 seq;
  u64 ack;
  u64 ack_bits;
  u64 timestamp_ns;

  Header(PacketType type, u32 conn_id, u64 seq, u64 ack, u64 ack_bits,
         u64 timestamp_ns)
      : type(type), conn_id(conn_id), seq(seq), ack(ack),
        timestamp_ns(timestamp_ns) {}
};

class Packet {
public:
  Header header;
  std::vector<u8> payload;
  size_t size() const { return sizeof(Header) + payload.size(); }
  Packet(const std::vector<u8> &data, Header header) : header(header) {
    payload = std::move(data);
  };
};

class Connection {
  u32 _conn_id;
  u64 _seq_to_send;
  u16 _longest_contiguous_sequence;
  std::chrono::nanoseconds _rtt_smoothed;
  std::chrono::nanoseconds _rtt_variance;
  u64 _congestion_window;
  u64 _slow_start_threshold;
  u64 _inflight_bytes;
  std::unordered_map<u64, std::shared_ptr<Packet>> _inflight_tracker;
  std::queue<std::shared_ptr<Packet>> _send_queue;

  Connection()
      : _seq_to_send(0), _longest_contiguous_sequence(0),
        _rtt_smoothed(std::chrono::nanoseconds(0)),
        _rtt_variance(std::chrono::nanoseconds(0)), _congestion_window(12000),
        _slow_start_threshold(INFINITY), _inflight_bytes(0) {};

public:
  static Connection &init_handshake(Header init_header) {
    static Connection c;
    // Talk to server, ask for conn_id
    std::vector<u8> config_info = {0}; // Future encryption things will be here I think, and maybe other config info I might need
    Packet init_packet = Packet(config_info, init_header);
    // Read conn_id from server, set it in this-> and send with every subsequent
    // Packet
    return c;
  }

  void create_and_queue(const std::vector<u8> &data) {


	Packet(data, Header(PacketType::DATA, _conn_id, _seq_to_send, a_ack));

    // Create Packet and add to queue
    // Header(PacketType type, u32 conn_id, u64 seq, u64 ack, u64 ack_bits,
    //        u64 timestamp_ns)
  }
};

// bytes_in_flight ≤ cwnd must always hold
