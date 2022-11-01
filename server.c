#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 100
#define BUFFERSIZE 1000

int buf[BUFFERSIZE];
int nfds;
fd_set read_backup;
fd_set write_backup;
fd_set readfds;   // 1024 bits darwin
fd_set writefds;  // 1024 bits darwin

int passive_sock() {
  int passive = socket(PF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_addr = {
      .sin_family = PF_INET,
      .sin_port = htons(4242),
      .sin_addr = inet_addr("127.0.0.1"),
      .sin_zero = 0,
  };

  int ret = 0;
  ret = bind(passive, (struct sockaddr *)&server_addr, sizeof(server_addr));
  ret = listen(passive, BACKLOG);

  if (passive == -1 || ret == -1) {
    write(2, "error\n", 6);
    exit(1);
  }
  FD_SET(passive, &read_backup);

  printf("passive socket: %d\n", passive);
  return passive;
}

void handle_connection(int passive) {
  struct sockaddr client_addr;
  socklen_t cli_len;

  int fd = accept(passive, &client_addr, &cli_len);
  printf("connected fd: %d\n", fd);
  FD_SET(fd, &read_backup);
  nfds = nfds > fd ? nfds : fd + 1;
}

void handle_disconnection(int fd) {
  FD_CLR(fd, &read_backup);
  FD_CLR(fd, &write_backup);
  close(fd);
  printf("connection closed fd: %d\n", fd);
  if (nfds == fd + 1) --nfds;
}

void read_event(int fd) {
  int read_size = read(fd, buf, BUFFERSIZE);
  printf("read: fd: %d, ret: %d\n", fd, read_size);
  if (read_size == 0) {
    handle_disconnection(fd);
    return;
  }
  FD_SET(fd, &write_backup);
}

void write_event(int fd) {
  int write_size = write(fd, buf, 27);
  printf("write: fd: %d, ret: %d\n", fd, write_size);
  FD_CLR(fd, &write_backup);
}

int main() {
  FD_ZERO(&read_backup);
  FD_ZERO(&write_backup);

  struct timeval time = {
      .tv_sec = 5,
      .tv_usec = 0,
  };

  int passive = passive_sock();

  nfds = passive + 1;
  while (1) {
    FD_COPY(&read_backup, &readfds);
    FD_COPY(&write_backup, &writefds);
    int n = select(nfds, &readfds, &writefds, NULL, &time);
    printf("select: ret: %d\n", n);
    printf("nfds: %d\n", nfds);
    if (FD_ISSET(passive, &readfds)) handle_connection(passive);
    for (int i = 0; i < nfds; i++) {
      if (passive == i) continue;
      if (FD_ISSET(i, &readfds)) read_event(i);
      if (FD_ISSET(i, &writefds)) write_event(i);
    }
  }
}