#!/bin/bash

FILE="server.c"
timeframe=0.1667

gcc server.c -o server -lws2_32 -lmswsock
./server

while true; do
    if find . -name "$FILE" -mmin -$timeframe | grep -q "$FILE"; then
        if kill -0 $(cat server.pid) 2>/dev/null; then
            kill $(cat server.pid)
        fi

        gcc server.c -o server -lws2_32 -lmswsock

        if [ $? -eq 0 ]; then
            echo "Recompiled and Running server" 
            ./server &
            echo $! > server.pid
        fi
        
    fi
    sleep 1
done
