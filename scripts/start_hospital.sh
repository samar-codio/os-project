#!/bin/bash

set -e

echo "Starting Hospital Patient Triage & Bed Allocator..."

rm -f /tmp/discharge_fifo /tmp/triage_fifo
ipcrm -M 0xbedf00d 2>/dev/null || true
ipcrm -S 0xbedf00d 2>/dev/null || true

mkfifo /tmp/discharge_fifo
chmod 666 /tmp/discharge_fifo

mkfifo /tmp/triage_fifo
chmod 666 /tmp/triage_fifo

echo "============================================"
echo " Ward Configuration"
echo " ICU beds: 4 (3 care units each)"
echo " Isolation beds: 4 (2 care units each)"
echo " General beds: 12 (1 care unit each)"
echo "============================================"

./admissions "$@" &  
ADM_PID=$!
echo "Admissions Manager PID: $ADM_PID"
echo "Use ./scripts/stop_hospital.sh to shut down."
wait $ADM_PID
