#include "../common/data.h"
#include <future>

int main(int argc, char *argv[]) {

  if (argc < 3) {
    std::cout << "gimme ports dum dum" << std::endl;
    return 1;
  }

  u16 local_port = atoi(argv[1]);
  u16 remote_port = atoi(argv[2]);

  Connection c = Connection(local_port, remote_port);
  std::vector<u8> v = {0, 1, 2, 3, 4};
  std::vector<u8> v2 = {1, 2, 3, 4, 5};
  std::vector<u8> v3 = {2, 3, 4, 5, 6};

  std::future<void> f = std::async(std::launch::async, &Connection::start, &c);

  c.send(v);
  std::cout << "sent v" << std::endl;
  c.send(v2);
  std::cout << "sent v2" << std::endl;
  c.send(v3);
  std::cout << "sent v3" << std::endl;
}
