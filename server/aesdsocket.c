#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define CHUNK_SIZE 1024

// Global flag for graceful termination
volatile sig_atomic_t caught_sig = 0;

void signal_handler(int sig_num) {
    if (sig_num == SIGINT || sig_num == SIGTERM) {
        caught_sig = 1;
    }
}

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    int server_fd, client_fd, data_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Check for daemon mode argument
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    }

    // Setup syslog
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    // Register signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Allow socket descriptor to be reusable (prevents "Address already in use" on rapid restarts)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // 2. Bind to port 9000
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Failed to bind to port %d: %s", PORT, strerror(errno));
        close(server_fd);
        return -1;
    }

    // 3. Daemonize IF requested (Must fork AFTER binding per instructions)
    if (daemon_mode) {
        syslog(LOG_INFO, "Running as daemon");
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
            close(server_fd);
            return -1;
        }
        if (pid > 0) {
            // Parent process exits successfully
            exit(0);
        }
        // Child process continues
        if (setsid() == -1) {
            syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
            return -1;
        }
        chdir("/");
        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null != -1) {
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }
    }

    // 4. Listen
    if (listen(server_fd, 10) == -1) {
        syslog(LOG_ERR, "Failed to listen: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // Open/Create the output file in append mode
    data_fd = open(DATA_FILE, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (data_fd == -1) {
        syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // 5. Main loop: Accept connections
    while (!caught_sig) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        
        // Handle interruption by signal gracefully
        if (client_fd == -1) {
            if (errno == EINTR && caught_sig) {
                break;
            }
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        // Log accepted connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Receive data into dynamically allocated buffer
        size_t buffer_size = CHUNK_SIZE;
        char *buffer = malloc(buffer_size);
        if (!buffer) {
            syslog(LOG_ERR, "Malloc failed");
            close(client_fd);
            continue;
        }

        size_t current_pos = 0;
        ssize_t bytes_received;
        bool packet_complete = false;

        while (!packet_complete && (bytes_received = recv(client_fd, buffer + current_pos, buffer_size - current_pos, 0)) > 0) {
            current_pos += bytes_received;

            // Check if we received a newline indicating the end of a packet
            if (memchr(buffer, '\n', current_pos) != NULL) {
                packet_complete = true;
                
                // Write the complete packet to the file
                if (write(data_fd, buffer, current_pos) == -1) {
                    syslog(LOG_ERR, "Failed to write to file: %s", strerror(errno));
                }

                // Echo the full content of the file back to the client
                lseek(data_fd, 0, SEEK_SET); // Rewind to start of file
                char send_buf[CHUNK_SIZE];
                ssize_t bytes_read;
                while ((bytes_read = read(data_fd, send_buf, CHUNK_SIZE)) > 0) {
                    send(client_fd, send_buf, bytes_read, 0);
                }

                // Reset file offset to the end for the next append
                lseek(data_fd, 0, SEEK_END);
            }

            // Expand buffer if it's getting full and newline hasn't been found
            if (!packet_complete && current_pos >= buffer_size) {
                buffer_size += CHUNK_SIZE;
                char *new_buffer = realloc(buffer, buffer_size);
                if (!new_buffer) {
                    syslog(LOG_ERR, "Realloc failed, discarding packet");
                    break; // Drop the packet to prevent crash
                }
                buffer = new_buffer;
            }
        }

        free(buffer);
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    // 6. Graceful cleanup on SIGINT/SIGTERM
    syslog(LOG_INFO, "Caught signal, exiting");
    close(data_fd);
    close(server_fd);
    remove(DATA_FILE);
    closelog();

    return 0;
}
