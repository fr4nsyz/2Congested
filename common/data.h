#include <algorithm>
#include <format>
#include <array>
#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <exception>
#include <format>
#include <memory>
#include <netinet/in.h>
#include <queue>
#include <stdexcept>
#include <sys/socket.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;
using u64 = uint64_t;

enum class PacketType : uint8_t {
  HANDSHAKE = 0,
  HANDSHAKE_ACK,
  DATA,
  ACK,
  CLOSE
};

class Header {
public:
  u8 _type;
  u32 _conn_id;
  u64 _seq;
  u64 _last_contiguous_ack;
  u64 _ack_bit_map;
  u64 _timestamp_ns;
  u32 _payload_size;

  Header(PacketType type, u32 conn_id, u64 seq, u64 ack, u64 ack_bit_map,
         u64 timestamp_ns, u32 payload_size)
      : _type(static_cast<u8>(type)), _conn_id(conn_id), _seq(seq),
        _last_contiguous_ack(ack), _ack_bit_map(ack_bit_map),
        _timestamp_ns(timestamp_ns), _payload_size(payload_size) {}

  Header(PacketType type, u32 conn_id, u64 seq, u64 ack, u64 ack_bit_map,
         u32 payload_size)
      : _type(static_cast<u8>(type)), _conn_id(conn_id), _seq(seq),
        _last_contiguous_ack(ack), _ack_bit_map(ack_bit_map),
        _timestamp_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count()),
        _payload_size(payload_size) {}
};

class Packet {
  // NOTE: to stay within the limits of the MTU which for ethernet is 1500
  // bytes, keep Packet object size to <1400 bytes ideally.
public:
  Header header;
  std::vector<u8> payload;

  size_t size() const { return sizeof(Header) + payload.size(); }

  // Deleting because we don't want deep copies
  Packet(const Packet &) = delete;
  Packet &operator=(const Packet &) = delete;

  Packet(Packet &&) = default;
  Packet &operator=(Packet &&) = default;

  Packet(const std::vector<u8> &data, Header header) : header(header) {
    payload = std::move(data);
  };
};

class Connection {

  // Socket Things
  int _sockfd;
  struct sockaddr_in _local_addr;
  struct sockaddr_in _remote_addr;
  u16 _remote_port;

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

  // Deleting because we don't want deep copies
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;

  Connection(Connection &&) = default;
  Connection &operator=(Connection &&) = default;

  u64 build_ack_bit_map();

  template <typename T>
  void serialize_multi_byte(const T &v, std::vector<u8> &buf);

  template <typename T>
  void deserialize_multi_byte(const T &v, std::vector<u8> &buf);

  void update_ack_states(u64 seq);

  void update_in_flight_tracker(u64 header_ack, u64 ack_bit_map);

  u16 deserialize_all(int sockfd, std::array<u8, 1400> &buf);

public:
  Connection();

  void create_and_queue_for_sending(const std::vector<u8> &data);

  void flush_send_queue();


  Packet receive_packet();
};

// bytes_in_flight ≤ cwnd must always hold
