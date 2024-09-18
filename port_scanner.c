#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>
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
    switch(opt) {
      case 't':
        options.targets = malloc(sizeof(char *) * (options.target_count + 1));
        options.targets[options.target_count] = optarg;
        options.target_count++;
        break;
      case 'p':
        options.ports = malloc(sizeof(char *) * (options.port_count + 1));
        options.ports[options.port_count] = optarg;
        options.port_count++;
        break;
      default:
        fprintf(stderr, "Usage: %s -t <target> -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  return options;
}


// RESOLVE single target
char* resolve_target(const char *target) {
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
  if (res -> ai_family == AF_INET) {
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res -> ai_addr;
    addr = &(ipv4 -> sin_addr);
  } else {
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res -> ai_addr;
    addr = &(ipv6 -> sin6_addr);
  }

  inet_ntop(res -> ai_family, addr, ipstr, INET6_ADDRSTRLEN);
  freeaddrinfo(res);
  return ipstr;
}


typedef struct {
  int *ports;
  int count;
} PortList;

// PARSE Port Input
PortList parse_ports(const char *port_input) {
    PortList plist;
    plist.ports = NULL;
    plist.count = 0;

    // Make a copy of port input
    char *input_copy = strdup(port_input);
    if (input_copy == NULL) {
        perror("Failed to allocate memory for port input copy");
        exit(EXIT_FAILURE);
    }

    char *token = strtok(input_copy, ",");
    while (token != NULL) {
        // Trim whitespace
        while (isspace((unsigned char)*token)) token++;

        // Handle Keywords like common and all
        if (strcmp(token, "common") == 0) {
            int common_ports[] = {20, 21, 22, 23, 25, 53, 67, 68, 69, 80, 110, 123,
                                  135, 137, 138, 139, 143, 161, 162, 179, 194, 389,
                                  443, 445, 465, 514, 515, 587, 993, 995, 1433, 1434,
                                  1521, 1723, 2049, 2083, 2087, 3128, 3306, 3389, 5432,
                                  5900, 5985, 5986, 6379, 8080, 8443, 8888, 9090, 9200,
                                  10000, 27017};
            int num_common_ports = sizeof(common_ports) / sizeof(common_ports[0]);
            for (int i = 0; i < num_common_ports; i++) {
                plist.ports = realloc(plist.ports, sizeof(int) * (plist.count + 1));
                if (plist.ports == NULL) {
                    perror("Failed to allocate memory for port list");
                    free(input_copy);
                    exit(EXIT_FAILURE);
                }
                plist.ports[plist.count++] = common_ports[i];
            }
        } else if (strcmp(token, "all") == 0) {
            for (int i = 1; i <= 65535; i++) {
                plist.ports = realloc(plist.ports, sizeof(int) * (plist.count + 1));
                if (plist.ports == NULL) {
                    perror("Failed to allocate memory for port list");
                    free(input_copy);
                    exit(EXIT_FAILURE);
                }
                plist.ports[plist.count++] = i;
            }
        } else {
            // Handle Ranges
            char *dash = strchr(token, '-');
            if (dash != NULL) {
                *dash = '\0';  // Split the string at the dash
                int start = atoi(token);
                int end = atoi(dash + 1);
                for (int i = start; i <= end; i++) {
                    plist.ports = realloc(plist.ports, sizeof(int) * (plist.count + 1));
                    if (plist.ports == NULL) {
                        perror("Failed to allocate memory for port list");
                        free(input_copy);
                        exit(EXIT_FAILURE);
                    }
                    plist.ports[plist.count++] = i;
                }
            } else {
                // Single port
                int port = atoi(token);
                plist.ports = realloc(plist.ports, sizeof(int) * (plist.count + 1));
                if (plist.ports == NULL) {
                    perror("Failed to allocate memory for port list");
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

// SCAN PORTS
bool scan_port(const char *ip, int port, int timeout_ms) {
  int sockfd;
  struct sockaddr_in addr;
  bool is_open = false;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket creation failed");
    return false;
  }

  // Set Socket to non-blocking
  fcntl(sockfd, F_SETFL, O_NONBLOCK);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &addr.sin_addr);

  int res = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
  if (res < 0) {
    if (errno == EINPROGRESS) {
      fd_set wait_set;
      struct timeval timeout;

      FD_ZERO(&wait_set);
      FD_SET(sockfd, &wait_set);

      timeout.tv_sec = timeout_ms / 1000;
      timeout.tv_usec = (timeout_ms % 1000) * 1000;

      res = select(sockfd + 1, NULL, &wait_set, NULL, &timeout);
      if (res > 0 && FD_ISSET(sockfd, &wait_set)) {
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


// THREADING
void * thread_scan(void *arg) {
  ScanArgs *sarg = (ScanArgs *)arg;
  bool is_open = scan_port(sarg -> ip, sarg -> port, 1000);
  if (is_open) {
    printf("%s:%d is open\n", sarg -> ip, sarg -> port);
  }
  free(sarg);
  return NULL;
}

#define MAX_THREADS 1000
int main(int argc, char *argv[]) {
  Options options = parse_args(argc, argv);

  // Resolve all targets
  char **ips = malloc(sizeof(char *) * options.target_count);
  for (int i=0; i < options.target_count; i++) {
    ips[i] = resolve_target(options.targets[i]);
    if (ips[i] == NULL) {
      fprintf(stderr, "Failed to resolve %s\n", options.targets[i]);
      exit(EXIT_FAILURE);
    }
  }

  PortList plist = parse_ports(options.ports[0]);

  // Create Thread Pool
  pthread_t threads[MAX_THREADS];
  int thread_count = 0;

  for (int i = 0; i < options.target_count; i++) {
    for (int j = 0; j < plist.count; j++) {
      ScanArgs *sarg = malloc(sizeof(ScanArgs));
      sarg -> ip = ips[i];
      sarg -> port = plist.ports[j];

      pthread_create(&threads[thread_count], NULL, thread_scan, sarg);
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

  // Cleanup
  for (int i = 0; i < options.target_count; i++) {
    free(ips[i]);
  }
  free(ips);
  free(plist.ports);
  free(options.targets);
  free(options.ports);

  return 0;
}


