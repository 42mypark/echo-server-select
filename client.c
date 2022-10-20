#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 100
#define BUFFERSIZE 1000

int buf[BUFFERSIZE];

int main() {
  int active = socket(PF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_addr = {
      .sin_family = PF_INET,
      .sin_port = 4242,
      .sin_addr = inet_addr("127.0.0.1"),
      .sin_zero = 0,
  };

  int ret =
      connect(active, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (ret == -1) {
    write(1, "error\n", 6);
    return 1;
  }

  write(active, "hello socket communication\n", 27);
  read(active, buf, BUFFERSIZE);
  write(1, buf, 27);
}