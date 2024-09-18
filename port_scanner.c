#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <ctype.h>

// Options 
typedef struct {
  char **targets;
  int target_count;
  char **ports;
  int port_count;
} Options;

// Parse CLI Args
Options parse_args(int argc, char *argv[]) {
  Options options;
  options.targets = NULL;
  options.target_count = 0;
  options.ports = NULL;
  options.port_count = 0;

  int opt;
  while ((opt = getopt(argc, argv, "t:p:")) != -1) {
    switch (opt) {
      case 't':
        options.targets = realloc(options.targets, sizeof(char *) * (options.target_count + 1));
        options.targets[options.target_count] = strdup(optarg);
        options.target_count++;
        break;
      case 'p':
        options.ports = realloc(options.ports, sizeof(char *) * (options.port_count + 1));
        options.ports[options.port_count] = strdup(optarg);
        options.port_count++;
        break;
      default:
        fprintf(stderr, "Usage: %s -t <target> -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  return options;
}

// Resolve Targets
char* resolve_targets(const char *target) {
  struct addrinfo hints, *res;
  int errcode;
  char *ipstr = malloc(sizeof(char) * INET6_ADDRSTRLEN);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(target, NULL, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo(%s): %s\n", target, gai_strerror(errcode));
    free(ipstr);
    return NULL;
  }

  void *addr;
  if (res->ai_family == AF_INET) {
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    addr = &(ipv4->sin_addr);
  } else {
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) res->ai_addr;
    addr = &(ipv6->sin6_addr);
  }

  inet_ntop(res->ai_family, addr, ipstr, INET6_ADDRSTRLEN);
  freeaddrinfo(res);
  return ipstr;
}

typedef struct {
  int *ports;
  int count;
} PortList;

// Parse Port Input
PortList parse_ports(const char *port_input) {
  PortList plist;
  plist.ports = NULL;
  plist.count = 0;

  char *input_copy = strdup(port_input);
  if (input_copy == NULL) {
    perror("Failed to allocate memory for port input");
    exit(EXIT_FAILURE);
  }

  char *token = strtok(input_copy, ",");
  while (token != NULL) {
    while (isspace((unsigned char) *token)) token++;

    if (strcmp(token, "common") == 0) {
      int common_ports[] =  {20, 21, 22, 23, 25, 53, 67, 68, 69, 80, 110, 123,
                                  135, 137, 138, 139, 143, 161, 162, 179, 194, 389,
                                  443, 445, 465, 514, 515, 587, 993, 995, 1433, 1434,
                                  1521, 1723, 2049, 2083, 2087, 3128, 3306, 3389, 5432,
                                  5900, 5985, 5986, 6379, 8080, 8443, 8888, 9090, 9200,
                                  10000, 27017};
      int num_common_ports = sizeof(common_ports) / sizeof(common_ports[0]);
      for (int i = 0; i < num_common_ports; i++) {
        plist.ports = realloc(plist.ports, sizeof(int) * (plist.count + 1));
        if (plist.ports == NULL) {
          perror("failed to allocate memory for port list.");
          free(input_copy);
          exit(EXIT_FAILURE);
        }
        plist.ports[plist.count++] = common_ports[i];
      }
    } else if (strcmp(token, "all") == 0) {
      for (int i = 1; i <= 65535; i++) {
        plist.ports = realloc(plist.ports, sizeof(int) * (plist.count + 1));
        if (plist.ports == NULL) {
          perror("failed to allocate memory for port list.");
          free(input_copy);
          exit(EXIT_FAILURE);
        }
        plist.ports[plist.count++] = i;
      }
    } else {
      char *dash = strchr(token, '-');
      if (dash != NULL) {
        *dash = '\0';
        int start = atoi(token);
        int end = atoi(dash + 1);
        if (start > end) {
          fprintf(stderr, "Invalid port range: %s\n", token);
          free(input_copy);
          exit(EXIT_FAILURE);
        }
        for (int i = start; i <= end; i++) {
          plist.ports = realloc(plist.ports, sizeof(int) * (plist.count + 1));
          if (plist.ports == NULL) {
            perror("failed to allocate memory for port list.");
            free(input_copy);
            exit(EXIT_FAILURE);
          }
          plist.ports[plist.count++] = i;
        }
      } else {
        int port = atoi(token);
        plist.ports = realloc(plist.ports, sizeof(int) * (plist.count + 1));
        if (plist.ports == NULL) {
          perror("failed to allocate memory for port list.");
          free(input_copy);
          exit(EXIT_FAILURE);
        }
        plist.ports[plist.count++] = port;
      }
    }

    token = strtok(NULL, ",");
  }

  free(input_copy);
  return plist;
}

// Scan Ports
bool scan_port(const char *ip, int port, int timeout_ms) {
  int sockfd;
  struct sockaddr_in addr;
  bool is_open = false;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket() failed");
    return false;
  }

  fcntl(sockfd, F_SETFL, O_NONBLOCK);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &addr.sin_addr);

  int res = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
  if (res < 0) {
    if (errno == EINPROGRESS) {
      struct timeval tv;
      fd_set fdset;

      tv.tv_sec = 0;
      tv.tv_usec = timeout_ms * 1000;

      FD_ZERO(&fdset);
      FD_SET(sockfd, &fdset);

      if (select(sockfd + 1, NULL, &fdset, NULL, &tv) > 0) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error == 0) {
          is_open = true;
        }
      }
    }
  } else {
    is_open = true;
  }

  close(sockfd);
  return is_open;
}

typedef struct {
  char *ip;
  int port;
} ScanArgs;

// Thread Scan
void *thread_scan(void *arg) {
  ScanArgs *args = (ScanArgs *)arg;
  bool is_open = scan_port(args->ip, args->port, 1000);
  if (is_open) {
    printf("%s:%d is open\n", args->ip, args->port);
  }
  free(args);
  return NULL;
}

#define MAX_THREADS 1000
int main(int argc, char *argv[]) {
  Options options = parse_args(argc, argv);

  if (options.port_count == 0 || options.target_count == 0) {
    fprintf(stderr, "Usage: %s -t <target> -p <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Resolve
  char **ips = malloc(sizeof(char *) * options.target_count);
  for (int i = 0; i < options.target_count; i++) {
    ips[i] = resolve_targets(options.targets[i]);
    if (ips[i] == NULL) {
      fprintf(stderr, "Failed to resolve target: %s\n", options.targets[i]);
      exit(EXIT_FAILURE);
    }
  }

  PortList plist = parse_ports(options.ports[0]);

  pthread_t threads[MAX_THREADS];
  int thread_count = 0;

  for (int i = 0; i < options.target_count; i++) {
    for (int j = 0; j < plist.count; j++) {
      ScanArgs *args = malloc(sizeof(ScanArgs));
      args->ip = ips[i];
      args->port = plist.ports[j];

      pthread_create(&threads[thread_count], NULL, thread_scan, args);
      thread_count++;

      if (thread_count == MAX_THREADS) {
        for (int k = 0; k < MAX_THREADS; k++) {
          pthread_join(threads[k], NULL);
        }
        thread_count = 0;
      }
    }
  }

  for (int i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  // Clean Up This is where I begin to understand how Golang was created
  // The amount that Golang facilitates in memory forgivenness and safety
  // In C you walk a fine line of keeping track of every single bit. UGH!
  for (int i = 0; i < options.target_count; i++) {
    free(ips[i]);
  }
  free(ips);
  free(plist.ports);
  free(options.ports);
  free(options.targets);

  return 0;
}
