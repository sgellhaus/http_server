#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int server;

void signal_handler(int signum) {
  if (signum == SIGINT) {
    printf("Server shutting down...\n");
    close(server);
    exit(0);
  }
}

char *parse_request(char *request) {
  char *method = strtok(request, " ");
  char *path = strtok(NULL, " ");
  char *version = strtok(NULL, "\r\n");
  if (strcmp(method, "GET") == 0) {
    return path;
  }
  return NULL;
}

char *get_file_content(const char *filename) {
  printf("Reading file: %s\n", filename);
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("File opening failed");
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *content = malloc(length + 1);
  if (content == NULL) {
    perror("Memory allocation failed");
    fclose(file);
    return NULL;
  }
  fread(content, 1, length, file);
  content[length] = '\0';
  fclose(file);
  return content;
}

int main(int argc, char *argv[]) {
  int port = 8080; // Default port
  char *dir = "."; // Default directory

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      port = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
      dir = argv[++i];
    } else {
      printf("Unknown or incomplete argument: %s\n", argv[i]);
      printf("Usage: %s [--port PORT] [--dir DIRECTORY]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  signal(SIGINT, signal_handler);

  printf("Server will start on port %d and serve directory %s\n", port, dir);
  int server_socket;
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  int code =
      bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (code < 0) {
    perror("Bind failed");
    printf("Code: %d\n", code);
    close(server_socket);
    exit(EXIT_FAILURE);
  }
  if (listen(server_socket, 5) < 0) {
    perror("Listen failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }
  printf("Server is listening on port %d\n", port);
  while (1) {
    int connection = accept(server_socket, NULL, NULL);
    printf("Connection accepted\n");
    if (connection < 0) {
      perror("Accept failed");
      close(server_socket);
      exit(EXIT_FAILURE);
    }
    char buffer[1024];

    while (1) {
      ssize_t bytes_received = recv(connection, buffer, sizeof(buffer) - 1, 0);
      if (bytes_received < 0) {
        perror("Receive failed");
        close(connection);
        close(server_socket);
        exit(EXIT_FAILURE);
      }

      if (bytes_received == 0) {
        printf("No data received, closing connection\n");
        close(connection);
        break; // No data received, close connection
      }

      buffer[bytes_received] = '\0';
      printf("Received request:\n%s\n", buffer);

      if (strstr(buffer, "GET / ") != NULL) {
        const char *response = "HTTP/1.1 303 See Other\r\n"
                               "Location: /index.html\r\n"
                               "Content-Length: 0\r\n"
                               "\r\n";
        send(connection, response, strlen(response), 0);
        continue;
      }

      char *file_name = parse_request(buffer);
      if (file_name == NULL) {
        const char *response = "HTTP/1.1 400 Bad Request\r\n"
                               "Content-Length: 0\r\n"
                               "\r\n";
        send(connection, response, strlen(response), 0);
        close(connection);
        continue;
      }

      char file_path[1024];
      snprintf(file_path, sizeof(file_path), "%s%s", dir, file_name);
      char *content = get_file_content(file_path);

      if (content == NULL) {
        const char *response = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Length: 0\r\n"
                               "\r\n";
        send(connection, response, strlen(response), 0);
        close(connection);
        continue;
      }

      char response[1024];
      snprintf(response, sizeof(response),
               "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html\r\n"
               "Content-Length: %zu\r\n"
               "\r\n%s",
               strlen(content), content);
      send(connection, response, strlen(response), 0);
      free(content);
    }
  }
  close(server_socket);
  return 0;
}