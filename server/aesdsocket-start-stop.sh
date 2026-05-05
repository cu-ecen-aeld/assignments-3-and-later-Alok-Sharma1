#!/bin/sh
# /etc/init.d/S99aesdsocket

DAEMON=/usr/bin/aesdsocket
NAME=aesdsocket

case "$1" in
    start)
        echo "Starting $NAME..."
        # -S starts the daemon
        # -x specifies the executable
        # -- -d passes the -d argument to aesdsocket
        start-stop-daemon -S -n $NAME -x $DAEMON -- -d
        ;;
    stop)
        echo "Stopping $NAME..."
        # -K stops the daemon (sends SIGTERM by default)
        start-stop-daemon -K -n $NAME
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0