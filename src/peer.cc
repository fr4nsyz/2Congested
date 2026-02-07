#include "../common/data.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
#include <unistd.h>

int main(int argc, char *argv[]) {

  if (argc < 3) {
    std::cout << "gimme ports dum dum" << std::endl;
    return 1;
  }

  u16 local_port = atoi(argv[1]);
  u16 remote_port = atoi(argv[2]);

  Connection c = Connection(local_port, remote_port);
  std::vector<u8> v = {0, 1, 2, 3, 4};

  int base = 250;
  double scaler = 0.5;

  using clock = std::chrono::steady_clock;
  auto next_send_time = clock::now();

  for (;;) {
    try {
      c.receive_packet();
      std::cout << "Received" << std::endl;
    } catch (std::exception &e) {
      std::cout << "receive error: " << e.what() << std::endl;
    }

    auto now = clock::now();
    if (now >= next_send_time) {
      c.create_and_queue_for_sending(v);
      c.flush_send_queue();
      std::cout << "sent" << std::endl;

      int delay = std::max(base, 1);
      next_send_time = now + std::chrono::milliseconds(delay);

      base *= scaler;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
