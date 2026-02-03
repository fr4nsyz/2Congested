#include "../common/data.h"
int main() {
  Connection c = Connection();
  std::vector<u8> v = {0, 1, 2, 3, 4};
  for (;;) {
    c.receive_packet();
    c.create_and_queue_for_sending(v);
    c.flush_send_queue();
  }
}
