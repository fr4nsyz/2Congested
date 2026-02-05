#include "../common/data.h"
#include <arpa/inet.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <endian.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>

using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;
using u64 = uint64_t;

Header::Header(PacketType type, u32 conn_id, u64 seq, u64 ack, u64 ack_bit_map,
               u64 timestamp_ns, u32 payload_size)
    : _type(static_cast<u8>(type)), _conn_id(conn_id), _seq(seq),
      _last_contiguous_ack(ack), _ack_bit_map(ack_bit_map),
      _timestamp_ns(timestamp_ns), _payload_size(payload_size) {}

Header::Header(PacketType type, u32 conn_id, u64 seq, u64 ack, u64 ack_bit_map,
               u32 payload_size)
    : _type(static_cast<u8>(type)), _conn_id(conn_id), _seq(seq),
      _last_contiguous_ack(ack), _ack_bit_map(ack_bit_map),
      _timestamp_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count()),
      _payload_size(payload_size) {}

size_t Packet::size() const { return sizeof(Header) + _payload.size(); }

Packet::Packet(const std::vector<u8> &data, Header &header) : _header(header) {
  _payload = data;
};

u64 Connection::build_ack_bit_map() {
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

void Connection::update_ack_states(u64 seq) {
  if (seq == _last_contiguous_ack + 1) {
    while (_received_ooo_packet_nums.count(_last_contiguous_ack + 1)) {
      _received_ooo_packet_nums.erase(++_last_contiguous_ack);
    }
  } else if (seq > _last_contiguous_ack + 1) {
    _received_ooo_packet_nums.insert(seq);
  }
}

void Connection::update_in_flight_tracker(u64 header_ack, u64 ack_bit_map) {
  for (auto it = _inflight_tracker.begin(); it != _inflight_tracker.end();) {
    u32 seq = it->first;

    if (seq <= header_ack) {

      _inflight_bytes -= it->second->size();

      it = _inflight_tracker.erase(it);
    } else if (seq <= (header_ack + 64)) {
      u64 offset = (seq - (header_ack + 1));
      if ((ack_bit_map >> offset) & 1) {
        // Ack was set
        it = _inflight_tracker.erase(it);
        _inflight_bytes -= it->second->size();
      } else {
        // Ack wasn't set for seq, so do not remove
        ++it;
      }
    } else {
      ++it;
    }
  }
}

int Connection::deserialize_all(
    int sockfd,
    std::array<u8, 1400>
        &buf) { // ONLY NEEDS TO RETURN U16 SINCE ARR BOUNDS ARENT THAT LARGE

  socklen_t len = sizeof(_remote_addr);

  struct sockaddr_in from_addr;

  int bytes_read =
      recvfrom(_sockfd, buf.data(), buf.size(), 0,
               reinterpret_cast<struct sockaddr *>(&from_addr), &len);

  std::cout << bytes_read << std::endl;
  return bytes_read;
}

Connection::Connection(u16 local_port, u16 remote_port)
    : _local_port(local_port), _remote_port(remote_port), _conn_id(UINT32_MAX),
      _seq_to_send(0), _last_contiguous_ack(0), _longest_contiguous_sequence(0),
      _rtt_smoothed(std::chrono::nanoseconds(0)),
      _rtt_variance(std::chrono::nanoseconds(0)), _congestion_window(12000),
      _slow_start_threshold(UINT64_MAX), _inflight_bytes(0) {

  // _conn_id is set to UINT32_MAX at the beginnning just to create an
  // invalid starting state. It gets updated in the init_handshake
  // method to return the client programmer's Connection with a _conn_id
  // from the server
  try {
    // Create sockfd, set it in this object
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sockfd < 0) {
      throw std::runtime_error(
          "[FATAL] socket could not be created! _sockfd was error code");
    }

    int flags = fcntl(_sockfd, F_GETFL, 0);
    if (flags == -1) {
      perror("fcntl F_GETFL");
      exit(EXIT_FAILURE);
    }

    if (fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
      perror("fcntl F_SETFL");
      exit(EXIT_FAILURE);
    }

    struct sockaddr_in local_addr;

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;  // listen on all interfaces
    local_addr.sin_port = htons(_local_port); // your port number

    if (bind(_sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
      throw std::runtime_error("[FATAL] could not bind socket");
    }

    memset(&_remote_addr, 0, sizeof(_remote_addr));
    _remote_addr.sin_family = AF_INET;
    _remote_addr.sin_port = htons(_remote_port); // Destination port
    inet_pton(AF_INET, "127.0.0.1",
              &_remote_addr.sin_addr); // Destination IP

    std::vector<u8> config_info = {
        0}; // Future encryption things will be here I think, and maybe other
            // config info I might need
    Header h = Header(PacketType::HANDSHAKE, 0, _seq_to_send,
                      _last_contiguous_ack, build_ack_bit_map(), 1);
    Packet init_packet = Packet(config_info, h);
    // Read conn_id from server, set it in this-> and send with every
    // subsequent Packet
  } catch (std::exception e) {
    throw std::runtime_error(
        std::format("[ FATAL ] COULD NOT CREATE CONNECTION {}", e.what()));
  }
}

void Connection::create_and_queue_for_sending(const std::vector<u8> &data) {
  // NOTE THIS ASSUMES THE CALLER KNOWS THE VECTOR data IS WITHIN THE UDP
  // DATAGRAM PACKET SIZE TO ENSURE SENIDNG IS WITHIN BOUNDS OF MTU

  Header h = Header(PacketType::DATA, _conn_id, _seq_to_send,
                    _last_contiguous_ack, build_ack_bit_map(), data.size());
  auto packet = std::make_shared<Packet>(data, h);

  std::cout << "packet juices: " << packet->_header._seq << std::endl;

  std::cout << "sent packet" << *packet << std::endl;

  _inflight_tracker[_seq_to_send] = packet;
  _inflight_bytes += packet->size();

  _send_queue.push(packet);
  ++_seq_to_send;
}

void Connection::flush_send_queue() {

  if (_inflight_bytes >= _congestion_window) {
    _congestion_window /= 2;
    return;
  }

  if (_send_queue.empty()) {
    return;
  }

  auto p = _send_queue.front();

  std::vector<u8> buf;

  buf.reserve(1400); // Avoid too many memory reallocations for the push_backs

  // Encode header (296 bits == 40 bytes)

  buf.push_back(p->_header._type); // Single byte, no need to reorder

  serialize_multi_byte(htonl(p->_header._conn_id), buf);

  serialize_multi_byte(htobe64(p->_header._seq), buf);

  serialize_multi_byte(htobe64(p->_header._last_contiguous_ack), buf);

  serialize_multi_byte(htobe64(p->_header._ack_bit_map), buf);

  serialize_multi_byte(htobe64(p->_header._timestamp_ns), buf);

  // We have 1360 bytes left for the rest of the payload
  serialize_multi_byte(htonl(p->_payload.size()), buf);

  buf.insert(buf.end(), p->_payload.begin(), p->_payload.end());

  sendto(_sockfd, buf.data(), buf.size(), 0,
         reinterpret_cast<struct sockaddr *>(&_remote_addr),
         sizeof(_remote_addr));

  _send_queue.pop();
}

Packet Connection::receive_packet() {
  std::array<u8, 1400> buf;

  int packet_size = deserialize_all(_sockfd, buf);

  if (packet_size < 0) {
    throw std::runtime_error("no packet");
  }

  std::cout << "in receive_packet after deserializing all" << std::endl;

  u8 *ptr = buf.data();

  u8 type = *ptr;
  ++ptr;

  u32 conn_id;
  u64 seq;
  u64 last_contiguous_ack;
  u64 ack_bit_map;
  u64 timestamp_ns;
  u32 payload_size;

  std::memcpy(&conn_id, ptr, sizeof(conn_id));
  ptr += sizeof(conn_id);

  std::memcpy(&seq, ptr, sizeof(seq));
  ptr += sizeof(seq);

  std::memcpy(&last_contiguous_ack, ptr, sizeof(last_contiguous_ack));
  ptr += sizeof(last_contiguous_ack);

  std::memcpy(&ack_bit_map, ptr, sizeof(ack_bit_map));
  ptr += sizeof(ack_bit_map);

  std::memcpy(&timestamp_ns, ptr, sizeof(timestamp_ns));
  ptr += sizeof(timestamp_ns);

  std::memcpy(&payload_size, ptr, sizeof(payload_size));
  ptr += sizeof(payload_size);

  conn_id = ntohl(conn_id);
  seq = be64toh(seq);
  last_contiguous_ack = be64toh(last_contiguous_ack);
  ack_bit_map = be64toh(ack_bit_map);
  timestamp_ns = be64toh(timestamp_ns);
  payload_size = ntohl(payload_size);

  Header h =
      Header(static_cast<PacketType>(type), conn_id, seq, last_contiguous_ack,
             ack_bit_map, timestamp_ns, payload_size);

  update_in_flight_tracker(h._last_contiguous_ack, h._ack_bit_map);
  update_ack_states(seq);

  std::vector<u8> v(ptr, ptr + payload_size);

  return Packet(v, h);
}

std::ostream &operator<<(std::ostream &os, const Header &h) {
  os << "Header(seq=" << h._seq << ", timestamp=" << h._timestamp_ns
     << ", ack_bit_map=" << h._ack_bit_map << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Packet &p) {
  os << p._header << ", payload_size=" << p.size() - sizeof(Header);
  return os;
}

// bytes_in_flight ≤ cwnd must always hold
