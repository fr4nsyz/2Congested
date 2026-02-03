#include "../common/data.h"
#include <exception>
#include <iostream>
int main() {
  Connection c = Connection();
  std::vector<u8> v = {0, 1, 2, 3, 4};
  for (;;) {
    try {
      c.receive_packet();
      std::cout << "Received" << std::endl;
    } catch (std::exception e) {
      std::cout << "receive error: " << e.what() << std::endl;
    }
    c.create_and_queue_for_sending(v);
    c.flush_send_queue();
    std::cout << "sent" << std::endl;
  }
}
