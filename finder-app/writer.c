#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments. Expected 2, got %d.", argc - 1);
        fprintf(stderr, "Usage: %s <file> <string>\n", argv[0]);
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file %s: %s", writefile, strerror(errno));
        closelog();
        return 1;
    }

    size_t len = strlen(writestr);
    ssize_t bytes_written = write(fd, writestr, len);
    
    if (bytes_written == -1) {
        syslog(LOG_ERR, "Failed to write to file %s: %s", writefile, strerror(errno));
        close(fd);
        closelog();
        return 1;
    } else if ((size_t)bytes_written != len) {
        syslog(LOG_ERR, "Partial write to file %s. Expected %zu bytes, wrote %zd bytes.", writefile, len, bytes_written);
        close(fd);
        closelog();
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    if (close(fd) == -1) {
        syslog(LOG_ERR, "Failed to close file %s: %s", writefile, strerror(errno));
        closelog();
        return 1;
    }

    closelog();
    return 0;
}