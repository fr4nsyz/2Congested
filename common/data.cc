#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <queue>
#include <stdexcept>
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

class Header {
  PacketType _type;
  u32 _conn_id;
  u64 _seq;
  u64 _last_contiguous_ack;
  u64 _ack_bits;
  u64 _timestamp_ns;

public:
  Header(PacketType type, u32 conn_id, u64 seq, u64 ack, u64 ack_bits)
      : _type(type), _conn_id(conn_id), _seq(seq), _last_contiguous_ack(ack),
        _ack_bits(ack_bits),
        _timestamp_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count()) {}
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

  // Socket Things
  int _sockfd;

  // Connection Things
  u32 _conn_id;
  u64 _seq_to_send;
  u64 _last_contiguous_ack; // Last ack marking point where contiguous received
                            // breaks (aka we lose packets)
  u64 _longest_contiguous_sequence;
  std::chrono::nanoseconds _rtt_smoothed;
  std::chrono::nanoseconds _rtt_variance;
  u64 _congestion_window;
  u64 _slow_start_threshold;
  u64 _inflight_bytes;
  std::unordered_map<u64, std::shared_ptr<Packet>> _inflight_tracker;
  std::unordered_set<u64> _received_ooo_packet_nums; // Received out of order
  std::queue<std::shared_ptr<Packet>> _send_queue;

  Connection()
      : _conn_id(UINT32_MAX), _seq_to_send(0), _last_contiguous_ack(-1),
        _longest_contiguous_sequence(0),
        _rtt_smoothed(std::chrono::nanoseconds(0)),
        _rtt_variance(std::chrono::nanoseconds(0)), _congestion_window(12000),
        _slow_start_threshold(INFINITY), _inflight_bytes(0) {
          // _conn_id is set to UINT32_MAX at the beginnning just to create an
          // invalid starting state. It gets updated in the init_handshake
          // method to return the client programmer's Connection with a _conn_id
          // from the server
        };

  u64 build_ack_bits() {
    u64 ONE_ULL = 1;
    u64 bits = 0;
    for (const auto seq : _received_ooo_packet_nums) {
      if (seq <= _last_contiguous_ack) {
        continue;
      }

      u64 i = seq - (_last_contiguous_ack + 1);

      if (i < 64) {
        bits |= (ONE_ULL << i); // Need to get bit position at i in the bit map
      }
    }
    return bits;
  }

public:
  static Connection &init_handshake(Header init_header) {
    try {
      static Connection c;
      // Talk to server, ask for conn_id
      std::vector<u8> config_info = {
          0}; // Future encryption things will be here I think, and maybe other
              // config info I might need
      Packet init_packet = Packet(config_info, init_header);
      // Read conn_id from server, set it in this-> and send with every
      // subsequent Packet
      return c;
    } catch (std::exception e) {
      throw std::runtime_error(
          std::format("[ FATAL ] COULD NOT CREATE CONNECTION {}", e.what()));
    }
  }

  void create_and_queue_for_sending(const std::vector<u8> &data) {

    auto packet = std::make_shared<Packet>(
        data, Header(PacketType::DATA, _conn_id, _seq_to_send,
                     _last_contiguous_ack, build_ack_bits()));

    _send_queue.push(packet);
    ++_seq_to_send;
  }

  void flush_send_queue() {}
};

// bytes_in_flight ≤ cwnd must always hold
