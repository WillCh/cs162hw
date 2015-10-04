#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>
#include <stdbool.h>

#include "libhttp.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

void concate(char *server_path, char *client_path, char *newPath) {
  if (server_path[strlen(server_path) - 1] == '/') {
    // delete one /
    strcpy (newPath, server_path);
    int i = 0;
    int j = strlen(newPath);
    for (i = 0; i < strlen(client_path); i++) {
      if (i != strlen(client_path) - 1) {
        newPath[j] = client_path[i+1];
      } else {
        newPath[j] = 0;
      }
      j++;
    }
  } else {
    strcpy (newPath, server_path);
    int i = 0;
    int j = strlen(newPath);
    for (i = 0; i < strlen(client_path) + 1; i++) {
      if (i != strlen(client_path)) {
        newPath[j] = client_path[i];
      } else {
        newPath[j] = 0;
      }
      j++;
    }
  } 
}

// return -1 does not exisits
// return 0 is a file
// return 1 is a dir
int is_a_dir (char *path, int *file_size) {
  struct stat fileStat;
  int res = stat(path, &fileStat);
  if (res == -1) {
    return -1;
  }
  if (S_ISDIR(fileStat.st_mode)) {
    return 1;
  } 
  if (S_ISREG(fileStat.st_mode)) {
    *file_size = fileStat.st_size;
    return 0;
  } 
  return -1;
}
bool has_a_index (char *path, int *file_size) {
  char newPath[strlen(path) + 12 + 1];
  memset(newPath, '\0', sizeof(newPath));
  char *tmp = "/index.html";
  concate(path, tmp, newPath);
  int id = is_a_dir(newPath, file_size);
  if (id == 0) {
    return true;
  }
  return false;
}

void read_from_a_file_print (char *path, int fd) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    http_fatal_error("open local file failed");
  }
  char content;
  while (1) {
    content = fgetc(file);
    if (feof(file)) { 
      break ;
    }
    http_send_data(fd, &content, 1);
   }
  fclose(file);
}



void read_dir_print (char *path, int fd) {
  http_send_string(fd, 
    "<center>"
    "<h1>links are:</h1>"
    "<hr>");
  DIR *dir;
  struct dirent *dp;
  if ((dir = opendir (path)) == NULL) {
    http_fatal_error("fail to read the current directory");
  }
  while ((dp = readdir (dir)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0) continue;
    char *linkName = dp->d_name;
    if (strcmp(dp->d_name, "..") == 0) {
      linkName = "Parent directory";
    }
    char tmp[2 * strlen(dp->d_name) + 38];
    memset (tmp, '\0', sizeof(tmp));
    char *head = "<p><a href=\"";
    strcpy (tmp, head);
    char *tail = "</a></p>";
    strcat (tmp, dp->d_name);
    if (strcmp(dp->d_name, "..") == 0) {
      strcat (tmp, "/\">");
    } else {
      strcat (tmp, "\">");
    }
    strcat (tmp, linkName);
    strcat (tmp, tail);
    http_send_string(fd, tmp);
  }
  http_send_string(fd, "</center>");
}
/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {

  /* YOUR CODE HERE (Feel free to delete/modify the existing code below) */

  struct http_request *request = http_request_parse(fd);
  // TODO: concatete to get the real path, need to delete the /
  char *newPath = (char *) malloc (strlen(request->path) +
    strlen(server_files_directory) + 1);

  concate(server_files_directory, request->path, newPath);
  if (strcmp(request->method, "GET") == 0) {
    int file_size = 0;
    int id = is_a_dir(newPath, &file_size);
    if (id == 0) {
      // has an exisitng file
      char file_size_tmp[288];
      sprintf(file_size_tmp, "%d", file_size);
      http_start_response(fd, 200);
      http_send_header(fd, "Content-type", http_get_mime_type(newPath));
      http_send_header(fd, "Content-length", file_size_tmp);
      http_end_headers(fd);
      read_from_a_file_print(newPath, fd);
    } else if (id == 1) {
      if (has_a_index(newPath, &file_size)) {
        // sent the html
        char file_size_tmp[288];
        sprintf(file_size_tmp, "%d", file_size);
        char updatedPath[strlen(newPath) + 12 + 4];
        char *tmp = "/index.html";
        concate(newPath, tmp, updatedPath);
        http_start_response(fd, 200);
        http_send_header(fd, "Content-type", http_get_mime_type(updatedPath));
        http_send_header(fd, "Content-length", file_size_tmp);
        http_end_headers(fd);
        read_from_a_file_print(updatedPath, fd);
      } else {
        // send the links of all children
        http_start_response(fd, 200);
        http_send_header(fd, "Content-type", "text/html");
        http_end_headers(fd);
        read_dir_print(newPath, fd);
      }
      
    } else {
      // NOthing, 404 error
      http_start_response(fd, 404);
      http_send_header(fd, "Content-type", "text/html");
      http_end_headers(fd);
      http_send_string(fd,
      "<center>"
      "<h1>404 ERROR!</h1>"
      "<hr>"
      "<p>Nothing's here yet.</p>"
      "</center>");
    }
  }
  free(newPath);
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /* YOUR CODE HERE */
  struct hostent *hostentRes = gethostbyname(server_proxy_hostname);
  if (hostentRes == NULL) {
    perror("No such host");
    exit(errno);
  }
  char *hostIP = hostentRes->h_addr;
  printf("the hostIP is %s\n", hostIP);

  // create a socket
  int socket_num = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_num == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  // re-set the socket
   // reset the socket
  int socket_option = 1;
  if (setsockopt(socket_num, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  // set the socket address
  struct sockaddr_in server_ad;
  memset (&server_ad, 0, sizeof(server_ad));
  server_ad.sin_family= AF_INET;
  //(server_ad.sin_addr.s_addr) = hostIP;
  bcopy((char *)hostentRes->h_addr, 
         (char *)&server_ad.sin_addr.s_addr,
         hostentRes->h_length);
  server_ad.sin_port = htons(server_proxy_port);

  if (connect (socket_num, (struct sockaddr *)&server_ad, sizeof(server_ad)) < 0) {
    perror ("fail to connect to the des");
    exit(errno);
  }

  pid_t pid = fork();
  if (pid == 0) {
    // child, socket_num -> fd
    char buf[1024];
    size_t nbytes;
    int bytes_read;
    int bytes_write;
    while (1) {
      nbytes = sizeof(buf);
      bytes_read = read(socket_num, buf, nbytes);
      if (bytes_read > 0) {
        bytes_write = write(fd, buf, bytes_read);
        if (bytes_write < 0) break;
      } else {
        break;
      }
    }
    // close socket_num
    close(socket_num);
    return;
  } else if (pid > 0) {
    // parents, fd -> socket_num
    char buf[1024];
    size_t nbytes;
    int bytes_read;
    int bytes_write;
    while (1) {
      nbytes = sizeof(buf);
      bytes_read = read(fd, buf, nbytes);
      if (bytes_read > 0) {
        bytes_write = write(socket_num, buf, sizeof(buf));
        if (bytes_write < 0) break;
      } else {
        break;
      }
    }
    // close socket_num
    close(fd);
    return;
  } else {
      perror("Failed to fork child");
      exit(errno);
  }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;
  pid_t pid;
 
  // int domain, int type, int protocol
  // new a socket
  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  // reset the socket
  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  // set the sockadd struct
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  // bind the created socket with the socket address, i.e. the port
  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  // 1024 here means the hint of the waiting queue for this listen, not the port number
  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  while (1) {

    // get the 1st connection from the queue.
    // create a new socket with the same socket type protocol and 
    // address family as the specified socket, and allocate a new
    // file descriptor for that socket.
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    pid = fork();
    if (pid > 0) {
      // in the parents process just closed the socket
      close(client_socket_number);
    } else if (pid == 0) {
      // Un-register signal handler (only parent should have it)
      // i.e. the child recover the SIGINT
      signal(SIGINT, SIG_DFL);
      // the socket should only run in the parents
      close(*socket_number);
      // acts on the client's request
      request_handler(client_socket_number);
      close(client_socket_number);
      exit(EXIT_SUCCESS);
    } else {
      perror("Failed to fork child");
      exit(errno);
    }
  }

  close(*socket_number);

}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000\n";

void exit_with_usage() {

  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  // if the signal ask to close the file, then need to close the serve.
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  server_files_directory = malloc(1024);
  getcwd(server_files_directory, 1024);
  server_proxy_hostname = "inst.eecs.berkeley.edu";
  server_proxy_port = 80;


  // pointer of function
  void (*request_handler)(int) = handle_files_request;

  int i;
  // parser the user input for the setup
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      // choose different handle function pointer
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        // cut the server_proxy by : and conver the 2nd part to be int
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        // use default server_proxy_port
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
