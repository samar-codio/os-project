#!/bin/bash


set -e

ADM_PID=$(pgrep -f "./admissions" || true)
if [ -z "$ADM_PID" ]; then
    echo "Admissions process not running."
else
    echo "Sending SIGTERM to admissions (PID $ADM_PID)..."
    kill -TERM "$ADM_PID" 2>/dev/null || true
    sleep 1
fi

rm -f /tmp/discharge_fifo /tmp/triage_fifo
ipcrm -M 0xbedf00d 2>/dev/null || true
ipcrm -S 0xbedf00d 2>/dev/null || true

rm -f /dev/shm/bed_bitmap_shm 2>/dev/null || true
for sem in /sem_icu /sem_iso /sem_queue_empty /sem_queue_full; do
    rm -f /dev/shm/$sem 2>/dev/null || true
done

echo "Hospital shut down. IPC cleaned."
