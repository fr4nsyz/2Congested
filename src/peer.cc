#include "../common/data.h"
#include <exception>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
int main(int argc, char *argv[]) {

  if (argc < 3) {
    std::cout << "gimme ports dum dum" << std::endl;
  }

  u16 local_port = atoi(argv[1]);
  u16 remote_port = atoi(argv[2]);

  Connection c = Connection(local_port, remote_port);
  std::vector<u8> v = {0, 1, 2, 3, 4};
  double counter = 1;

  for (;;) {
    try {
      c.receive_packet();
      std::cout << "Received" << std::endl;
      throw std::runtime_error(" I RECEVIED SOMETHING");
    } catch (std::exception e) {
      std::cout << "receive error: " << e.what() << std::endl;
    }
    c.create_and_queue_for_sending(v);
    c.flush_send_queue();
    std::cout << "sent" << std::endl;
    sleep(counter / 1);
    counter *= 0.90;
  }
}
