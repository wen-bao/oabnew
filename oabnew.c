#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "httpd.h"
#include "slog.h"

#define DEFAULT_PORT 8080
#define MAX_EVENT_NUM 1024
#define INFTIM -1
#define LOG_LEVEL S_INFO

void process(int);

void handle_subprocess_exit(int);

void run();

void oabnew_help();

typedef struct oabnew_param {
  int start;
  int stop;
  int restart;
} oabnew_param;

static oabnew_param s_oabnew_param[1] = {{0}};

static struct option g_long_options[] = {{"help", 0, NULL, 'h'},
                                         {"start", 0, NULL, 'a'},
                                         {"stop", 1, NULL, 'o'},
                                         {"restart", 0, NULL, 'r'},
                                         {0, 0, 0, 0}};

const char *short_opt = "hao:r";
void oabnew_opt(int argc, char **argv) {
  while (1) {
    int option_index = 0;
    int c = getopt_long(argc, argv, short_opt, g_long_options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 'h':
        oabnew_help();
        exit(0);
        break;
      case 'a':
        s_oabnew_param->start = 1;
        break;
      case 'o':
        s_oabnew_param->stop = 1;
        break;
      case 'r':
        s_oabnew_param->restart = 1;
        break;
      case ':':
      case '?':
        oabnew_help();
        exit(1);
      default:
        break;
    }
  }
}

int main(int argc, char *argv[]) {
  oabnew_opt(argc, argv);

  if (init_logger("./logs", LOG_LEVEL) != TRUE) {
    SLOG_ERROR("Init logger failed:%s\n", strerror(errno));
    exit(1);
  }

  int port = DEFAULT_PORT;
  if (argc == 3 && isdigit(*argv[2])) {
    port = atoi(argv[2]);
  }

  if (s_oabnew_param->start) {
    SLOG_INFO("solve start ...\n");
    run(port);
  } else if (s_oabnew_param->stop) {
    SLOG_INFO("solve stop ...");
  } else if (s_oabnew_param->restart) {
    SLOG_INFO("solve restart");
  } else {
    oabnew_help();
    exit(1);
  }
  return 0;
}

void oabnew_help() {
  // printf("USAGE:\n\toabnew <start | stop | restart> [port]\n");
  printf("OABNEW WEB SERVER!\n");
  printf("usage: oabnew -hco:r  [port]\n");
  printf("\t-h,--help          help\n");
  // printf("\t-p,--path          operate file path\n");
  printf("\t-a,--start         start op. if not specify, decrypt op.\n");
  printf(
      "\t-o,--stop          result output path. if not specify, output to "
      "stdout!\n");
  printf("\t-r,--restart       result output operate path self\n");

  printf("\nsample:\n");
  printf("    oabnew -a 8080\n");
  printf("    oabnew -c 8080\n");
  printf("    oabnew -r 8080\n");
}

void run(int port) {
  struct sockaddr_in server_addr;
  int listen_fd;
  int cpu_core_num;
  int on = 1;
  int pro_child[16] = {0};

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  fcntl(listen_fd, F_SETFL, O_NONBLOCK);  // 设置 listen_fd 为非阻塞
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    SLOG_ERROR("Bind port:%d error, message: %s\n", port, strerror(errno));
    exit(1);
  }

  if (listen(listen_fd, 5) == -1) {
    SLOG_ERROR("Listen port:%d error, message: %s\n", port, strerror(errno));
    exit(1);
  }

  SLOG_INFO("Listening port:%d\n", port);

  signal(SIGCHLD, handle_subprocess_exit);

  cpu_core_num = get_nprocs();
  SLOG_INFO("CPU core num: %d\n", cpu_core_num);
  // 根据 CPU 数量创建子进程，为了演示“惊群现象”，这里多创建一些子进程
  int i = 0;
  for (i = 0; i < cpu_core_num * 2; i++) {
    pid_t pid = fork();
    if (pid == 0) {  // 子进程执行此条件分支
      process(listen_fd);
      exit(0);
    } else {  // 父进程执行
      pro_child[i] = pid;
      SLOG_INFO("Child_pid:%d\n", pid);
    }
  }

  while (1) {
    sleep(1);
  }
}

void process(int listen_fd) {
  int conn_fd;
  int ready_fd_num;
  struct sockaddr_in client_addr;
  int client_addr_size = sizeof(client_addr);
  char buf[128];

  struct epoll_event ev, events[MAX_EVENT_NUM];
  // 创建 epoll 实例，并返回 epoll 文件描述符
  int epoll_fd = epoll_create(MAX_EVENT_NUM);
  ev.data.fd = listen_fd;
  ev.events = EPOLLIN;

  // 将 listen_fd 注册到刚刚创建的 epoll 中
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
    SLOG_ERROR("Epoll_ctl error, message: %s\n", strerror(errno));
    exit(1);
  }

  while (1) {
    // 等待事件发生
    ready_fd_num = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, INFTIM);
    SLOG_DEBUG("[pid %d] ? 震惊！我又被唤醒了...\n", getpid());
    if (ready_fd_num == -1) {
      SLOG_ERROR("Epoll_wait error, message: %s\n", strerror(errno));
      continue;
    }
    int i = 0;
    for (i = 0; i < ready_fd_num; i++) {
      if (events[i].data.fd == listen_fd) {  // 有新的连接
        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr,
                         &client_addr_size);
        if (conn_fd == -1) {
          SLOG_DEBUG("[pid %d]Accept 出错了: %s\n", getpid(), strerror(errno));
          continue;
        }

        // 设置 conn_fd 为非阻塞
        if (fcntl(conn_fd, F_SETFL, fcntl(conn_fd, F_GETFD, 0) | O_NONBLOCK) ==
            -1) {
          continue;
        }

        ev.data.fd = conn_fd;
        ev.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
          SLOG_ERROR("Epoll_ctl error, message: %s\n", strerror(errno));
          close(conn_fd);
        }
        SLOG_INFO("[pid %d] ? 收到来自 %s:%d 的请求\n", getpid(),
                  inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
      } else if (events[i].events &
                 EPOLLIN) {  // 某个 socket 数据已准备好，可以读取了
        SLOG_INFO("[pid %d]处理来自 %s:%d 的请求\n", getpid(),
                  inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
        conn_fd = events[i].data.fd;
        // 调用 TinyHttpd 的 accept_request 函数处理请求
        accept_request(conn_fd, &client_addr);
        close(conn_fd);
      } else if (events[i].events & EPOLLERR) {
        SLOG_ERROR("Epoll error:%s\n", strerror(errno));
        close(conn_fd);
      }
    }
  }
}

void handle_subprocess_exit(int signo) {
  SLOG_INFO("Clean subprocess.\n");
  int status;
  while (waitpid(-1, &status, WNOHANG) > 0)
    ;
}