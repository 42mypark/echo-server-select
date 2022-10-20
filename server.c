#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 100
#define BUFFERSIZE 1000

int buf[BUFFERSIZE];

int main() {
  int passive = socket(PF_INET, SOCK_STREAM, 0);

  int ret;
  struct sockaddr_in server_addr = {
      .sin_family = PF_INET,
      .sin_port = 4242,
      .sin_addr = inet_addr("127.0.0.1"),
      .sin_zero = 0,
  };

  ret = bind(passive, (struct sockaddr *)&server_addr, sizeof(server_addr));
  ret = listen(passive, BACKLOG);

  if (passive == -1 || ret == -1) {
    write(2, "error\n", 6);
    return 1;
  }

  struct sockaddr client_addr;
  socklen_t cli_len;
  struct fd_set readfds;   // 1024 bits darwin
  struct fd_set writefds;  // 1024 bits darwin
  struct timeval time = {
      .tv_sec = 10,
      .tv_usec = 0,
  };
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(passive, &readfds);
  int nfds = passive + 1;
  printf("passive socket: %d\n", passive);
  fcntl(passive, F_SETFL, O_NONBLOCK);

  int read_size, write_size;
  while (1) {
    FD_SET(passive, &readfds);
    int n = select(nfds, &readfds, &writefds, NULL, NULL);
    printf("%d\n", n);
    if (FD_ISSET(passive, &readfds)) {
      int fd = accept(passive, &client_addr, &cli_len);
      printf("connected fd: %d\n", fd);
      FD_SET(fd, &readfds);
      nfds = nfds > fd ? nfds + 1 : fd + 1;
      FD_CLR(passive, &readfds);
    }
    for (int i = 3; i < nfds; i++) {
      if (FD_ISSET(i, &readfds)) {
        read_size = read(i, buf, BUFFERSIZE);
        if (read_size == 0) {
          FD_CLR(i, &readfds);
          close(i);
          printf("connection closed fd: %d\n", i);
        }
        write_size = write(i, buf, read_size);
      }
    }
    sleep(3);
  }
}