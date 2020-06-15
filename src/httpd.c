/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include "httpd.h"
#include "slog.h"

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: OABNEW-httpd/1.0\r\n"
#define STDIN 0
#define STDOUT 1
#define STDERR 2

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client, struct sockaddr_in *client_addr, const char *root) {
  char buf[1024];
  char time_buf[50];
  char *log_buf[5];
  size_t numchars;
  char method[255];
  char url[255];
  char path[512];
  size_t i, j;
  struct stat st;
  int cgi = 0; /* becomes true if server decides this is a CGI
                * program */
  char *query_string = NULL;

  log_buf[0] = inet_ntoa(client_addr->sin_addr);

  // 获取请求头的第一行
  numchars = get_line(client, buf, sizeof(buf));

  log_buf[1] = buf;

  SLOG_INFO("%s - - [%s] %s\n", log_buf[0], get_time_str(time_buf), log_buf[1]);

  // 获取请求方法
  i = 0;
  j = 0;
  while (!ISspace(buf[i]) && (i < sizeof(method) - 1)) {
    method[i] = buf[i];
    i++;
  }
  j = i;
  method[i] = '\0';

  // 判断请求方法是否所允许的方法 GET 和 POST
  if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
    unimplemented(client);
    return;
  }

  if (strcasecmp(method, "POST") == 0) cgi = 1;

  i = 0;
  // 消耗 http method 后面多余的空格
  while (ISspace(buf[j]) && (j < numchars)) j++;
  // 获取请求路径
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars)) {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';

  // 处理 http method 为 GET 的请求
  if (strcasecmp(method, "GET") == 0) {
    query_string = url;
    // 定位 url 中 ? 的位置
    while ((*query_string != '?') && (*query_string != '\0')) query_string++;
    // 将 ? 替换为 \0
    if (*query_string == '?') {
      cgi = 1;
      *query_string = '\0';
      query_string++;
    }
  }

  // 根据 url 拼接资源路径，比如请求 /test.html，拼接后的路径为 htdocs/test.html
  sprintf(path, "%s%s", root, url);
  if (path[strlen(path) - 1] == '/') strcat(path, "index.html");
  // 读取资源
  if (stat(path, &st) == -1) {
    // 读取并丢弃其他的请求头
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
    // 返回404响应
    not_found(client);
  } else {
    if ((st.st_mode & S_IFMT) == S_IFDIR) strcat(path, "/index.html");
    if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) ||
        (st.st_mode & S_IXOTH))
      cgi = 1;
    if (!cgi)
      // 返回200响应
      serve_file(client, path);
    else
      execute_cgi(client, path, method, query_string);
  }
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<P>Your browser sent a bad request, ");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource) {
  char buf[1024];

  fgets(buf, sizeof(buf), resource);
  while (!feof(resource)) {
    send(client, buf, strlen(buf), 0);
    fgets(buf, sizeof(buf), resource);
  }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc) {
  SLOG_ERROR("%s:%s\n", sc, strerror(errno));
  exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path, const char *method,
                 const char *query_string) {
  char buf[1024];
  int cgi_output[2];
  int cgi_input[2];
  pid_t pid;
  int status;
  int i;
  char c;
  int numchars = 1;
  int content_length = -1;

  buf[0] = 'A';
  buf[1] = '\0';
  if (strcasecmp(method, "GET") == 0)
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
  else if (strcasecmp(method, "POST") == 0) /*POST*/
  {
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf)) {
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&(buf[16]));
      numchars = get_line(client, buf, sizeof(buf));
    }
    if (content_length == -1) {
      bad_request(client);
      return;
    }
  } else /*HEAD or other*/
  {
  }

  if (pipe(cgi_output) < 0) {
    cannot_execute(client);
    return;
  }
  if (pipe(cgi_input) < 0) {
    cannot_execute(client);
    return;
  }

  if ((pid = fork()) < 0) {
    cannot_execute(client);
    return;
  }
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  if (pid == 0) /* child: CGI script */
  {
    char meth_env[255];
    char query_env[255];
    char length_env[255];

    dup2(cgi_output[1], STDOUT);
    dup2(cgi_input[0], STDIN);
    close(cgi_output[0]);
    close(cgi_input[1]);
    sprintf(meth_env, "REQUEST_METHOD=%s", method);
    putenv(meth_env);
    if (strcasecmp(method, "GET") == 0) {
      sprintf(query_env, "QUERY_STRING=%s", query_string);
      putenv(query_env);
    } else { /* POST */
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
      putenv(length_env);
    }
    execl(path, NULL);
    exit(0);
  } else { /* parent */
    close(cgi_output[1]);
    close(cgi_input[0]);
    if (strcasecmp(method, "POST") == 0)
      for (i = 0; i < content_length; i++) {
        recv(client, &c, 1, 0);
        write(cgi_input[1], &c, 1);
      }
    while (read(cgi_output[0], &c, 1) > 0) send(client, &c, 1, 0);

    close(cgi_output[0]);
    close(cgi_input[1]);
    waitpid(pid, &status, 0);
  }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size) {
  int i = 0;
  char c = '\0';
  int n;

  while ((i < size - 1) && (c != '\n')) {
    n = recv(sock, &c, 1, 0);
    /* DEBUG printf("%02X\n", c); */
    if (n > 0) {
      if (c == '\r') {
        n = recv(sock, &c, 1, MSG_PEEK);
        /* DEBUG printf("%02X\n", c); */
        if ((n > 0) && (c == '\n'))
          recv(sock, &c, 1, 0);
        else
          c = '\n';
      }
      buf[i] = c;
      i++;
    } else
      c = '\n';
  }
  buf[i] = '\0';

  return (i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename) {
  char buf[1024];
  char suffix[8];
  char type[24];
  get_file_suffix(filename, suffix);
  suffix2type(suffix, type);

  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: %s\r\n", type);
  send(client, buf, strlen(buf), 0);
  strcpy(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
}

void get_file_suffix(const char *filename, char *suffix) {
  int suffix_len = 0;
  for (int i = strlen(filename) - 1; i > 0; i--) {
    if (filename[i] != '.') {
      suffix_len++;
      continue;
    }

    strcpy(suffix, filename + i + 1);
    suffix[suffix_len] = '\0';
    break;
  }
}

void suffix2type(const char *suffix, char *type) {
  if (strcasecmp(suffix, "html") == 0) {
    strcpy(type, "text/html");
  } else if (strcasecmp(suffix, "htm") == 0) {
    strcpy(type, "text/html");
  } else if (strcasecmp(suffix, "txt") == 0) {
    strcpy(type, "text/plain");
  } else if (strcasecmp(suffix, "xml") == 0) {
    strcpy(type, "text/xml");
  } else if (strcasecmp(suffix, "js") == 0) {
    strcpy(type, "application/javascript");
  } else if (strcasecmp(suffix, "css") == 0) {
    strcpy(type, "text/css");
  } else if (strcasecmp(suffix, "pdf") == 0) {
    strcpy(type, "application/pdf");
  } else if (strcasecmp(suffix, "json") == 0) {
    strcpy(type, "application/json");
  } else if (strcasecmp(suffix, "jpg") == 0) {
    strcpy(type, "image/jpeg");
  } else if (strcasecmp(suffix, "png") == 0) {
    strcpy(type, "image/png");
  } else if (strcasecmp(suffix, "ico") == 0) {
    strcpy(type, "image/x-icon");
  } else if (strcasecmp(suffix, "gif") == 0) {
    strcpy(type, "image/gif");
  } else if (strcasecmp(suffix, "tif") == 0) {
    strcpy(type, "image/tiff");
  } else if (strcasecmp(suffix, "bmp") == 0) {
    strcpy(type, "application/x-bmp");
  } else {
    strcpy(type, "application/octet-stream");
  }
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexistent.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename) {
  FILE *resource = NULL;
  int numchars = 1;
  char buf[1024];
  int n;

  buf[0] = 'A';
  buf[1] = '\0';
  // 读取并丢弃其他消息头
  while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
    numchars = get_line(client, buf, sizeof(buf));

  resource = fopen(filename, is_text_type(filename) ? "r" : "rb");
  if (resource == NULL)
    not_found(client);
  else {
    headers(client, filename);
    cat(client, resource);
  }
  fclose(resource);
}

int is_text_type(const char *filename) {
  char suffix[8];
  get_file_suffix(filename, suffix);

  if (strcasecmp(suffix, "html") == 0) {
    return 1;
  } else if (strcasecmp(suffix, "htm") == 0) {
    return 1;
  } else if (strcasecmp(suffix, "txt") == 0) {
    return 1;
  } else if (strcasecmp(suffix, "xml") == 0) {
    return 1;
  } else if (strcasecmp(suffix, "js") == 0) {
    return 1;
  } else if (strcasecmp(suffix, "css") == 0) {
    return 1;
  } else if (strcasecmp(suffix, "json") == 0) {
    return 1;
  } else {
    return 0;
  }
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port) {
  int httpd = 0;
  int on = 1;
  struct sockaddr_in name;

  httpd = socket(PF_INET, SOCK_STREAM, 0);
  if (httpd == -1) error_die("Socket");
  memset(&name, 0, sizeof(name));
  name.sin_family = AF_INET;
  name.sin_port = htons(*port);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {
    error_die("Setsockopt failed");
  }
  if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    error_die("Bind");
  if (*port == 0) /* if dynamically allocating a port */
  {
    socklen_t namelen = sizeof(name);
    if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
      error_die("Getsockname");
    *port = ntohs(name.sin_port);
  }
  if (listen(httpd, 5) < 0) error_die("Listen");
  return (httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

char *get_time_str(char buf[]) {
  time_t now;
  struct tm *tm;
  time(&now);
  tm = localtime(&now);
  sprintf(buf, "%s", asctime(tm));
  buf[strlen(buf) - 1] = '\0';
  return buf;
}