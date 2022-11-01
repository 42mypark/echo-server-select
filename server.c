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

typedef struct session {
  int fd;
  int id;
  char *buf;
  struct session *next;
} session;

int client_id;
char g_buf[BUFFERSIZE];
int nfds;
session *session_head;
fd_set read_backup;
fd_set write_backup;
fd_set readfds;   // 1024 bits darwin
fd_set writefds;  // 1024 bits darwin

void ft_putstr(char *str, int fd) {
  int ret;
  while (*str) {
    ret = write(fd, str++, 1);
    if (ret == -1) {
      while (write(2, "error\n", 6) != -1)
        ;
      exit(1);
    }
  }
}

void syscall_check(int ret) {
  if (ret < 0) {
    ft_putstr("error\n", 2);
    exit(1);
  }
}

void malloc_check(void *ret) {
  if (ret == 0) {
    ft_putstr("error\n", 2);
    exit(1);
  }
}

session *new_session(int fd) {
  session s = {
      .fd = fd,
      .id = client_id++,
      .buf = NULL,
      .next = NULL,
  };
  session *ret = malloc(sizeof(session));
  *ret = s;
  ret->buf = malloc(1);
  malloc_check(ret->buf);
  ret->buf[0] = 0;
  return ret;
}

void free_session(session *s) {
  free(s->buf);
  free(s);
}

session *add_session(int fd) {
  session *s = new_session(fd);
  session *curr;
  if (session_head == NULL) {
    session_head = s;
  } else {
    curr = session_head;
    while (curr->next) curr = curr->next;
    curr->next = s;
  }
  return s;
}

void delete_session(int fd) {
  session *curr = session_head;
  session *prev = session_head;
  while (curr) {
    if (curr->fd == fd) {
      if (curr == session_head) {
        session_head = curr->next;
      } else {
        prev->next = curr->next;
      }
      free_session(curr);
      break;
    }
    prev = curr;
    curr = curr->next;
  }
}

session *get_session(int fd) {
  session *curr = session_head;
  while (curr) {
    if (curr->fd == fd) return curr;
    curr = curr->next;
  }
  return NULL;
}

char *ft_strjoin(char *s1, char *s2) {
  int len1 = strlen(s1);
  int len2 = strlen(s2);

  char *s3 = malloc(len1 + len2 + 1);
  malloc_check(s3);
  strcpy(s3, s1);
  strcpy(s3 + len1, s2);

  s3[len1 + len2] = 0;
  return s3;
}

int passive_sock() {
  int passive = socket(PF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_addr = {
      .sin_family = PF_INET,
      .sin_port = htons(4242),
      .sin_addr = inet_addr("127.0.0.1"),
      .sin_zero = 0,
  };

  int ret = 0;
  ret += bind(passive, (struct sockaddr *)&server_addr, sizeof(server_addr));
  ret += listen(passive, BACKLOG);

  int reuse = 1;
  ret += setsockopt(passive, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  syscall_check(passive);
  syscall_check(ret);

  FD_SET(passive, &read_backup);
  return passive;
}

void broadcast_msg(int fd, char *msg) {
  session *curr = session_head;
  while (curr) {
    if (curr->fd != fd) {
      char *buf = curr->buf;
      curr->buf = ft_strjoin(buf, msg);
      // printf("\nafter join: id: %d\n%s\n\n", curr->id, curr->buf);  // debug
      free(buf);
    }
    curr = curr->next;
  }
}

void handle_connection(int passive) {
  struct sockaddr client_addr;
  socklen_t cli_len;

  int fd = accept(passive, &client_addr, &cli_len);
  syscall_check(fd);
  session *user = add_session(fd);
  FD_SET(fd, &read_backup);
  FD_SET(fd, &write_backup);
  nfds = nfds > fd ? nfds : fd + 1;

  char msg[1000];
  sprintf(msg, "server: client %d arrived\n", user->id);
  broadcast_msg(fd, msg);
  // printf("%s\n", msg);  // debug
}

void handle_disconnection(int fd) {
  session *user = get_session(fd);
  char msg[1000];
  sprintf(msg, "server: client %d left\n", user->id);
  broadcast_msg(fd, msg);
  // printf("%s\n", msg);  // debug

  FD_CLR(fd, &read_backup);
  FD_CLR(fd, &write_backup);
  FD_CLR(fd, &writefds);
  int ret = close(fd);
  syscall_check(ret);
  delete_session(fd);
  if (nfds == fd + 1) --nfds;
}

void read_event(int fd) {
  int read_size = read(fd, g_buf, BUFFERSIZE);
  g_buf[read_size] = 0;

  syscall_check(read_size);
  if (read_size == 0) {
    handle_disconnection(fd);
    return;
  }

  // printf("read: fd: %d, buf: %s\n", fd, g_buf);  // debug

  char *msg = malloc(read_size + 1000);
  malloc_check(msg);
  session *user = get_session(fd);
  sprintf(msg, "client %d: %s", user->id, g_buf);
  broadcast_msg(fd, msg);
  free(msg);
}

void write_event(int fd) {
  session *user = get_session(fd);
  char *buf = user->buf;
  int buf_len = strlen(buf);
  int write_size = write(fd, buf, buf_len);
  syscall_check(write_size);
  buf += write_size;
  int i = 0;
  for (; buf[i]; ++i) {
    user->buf[i] = buf[i];
  }
  user->buf[i] = 0;
}

int main() {
  FD_ZERO(&read_backup);
  FD_ZERO(&write_backup);

  struct timeval time = {
      .tv_sec = 100,
      .tv_usec = 0,
  };

  int passive = passive_sock();

  nfds = passive + 1;
  while (1) {
    FD_COPY(&read_backup, &readfds);
    FD_COPY(&write_backup, &writefds);
    int n = select(nfds, &readfds, &writefds, NULL, &time);
    if (FD_ISSET(passive, &readfds)) handle_connection(passive);
    for (int i = 0; i < nfds; i++) {
      if (passive == i) continue;
      if (FD_ISSET(i, &readfds)) read_event(i);
      if (FD_ISSET(i, &writefds)) write_event(i);
    }
  }
}