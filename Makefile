# Phase 1: Makefile
CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -lrt -lpthread

SRCDIR = src
SCRIPTSDIR = scripts

all: admissions patient_simulator

admissions: $(SRCDIR)/admissions.c $(SRCDIR)/bed_allocator.h
	$(CC) $(CFLAGS) -o admissions $(SRCDIR)/admissions.c $(LDFLAGS)

patient_simulator: $(SRCDIR)/patient_simulator.c
	$(CC) $(CFLAGS) -o patient_simulator $(SRCDIR)/patient_simulator.c

clean:
	rm -f admissions patient_simulator
	rm -f /tmp/discharge_fifo /tmp/triage_fifo

run: all
	./scripts/start_hospital.sh

test: all
	@echo "Starting stress test: 20 rapid patient arrivals"
	./scripts/start_hospital.sh &
	sleep 1
	for i in $$(seq 1 20); do \
		./scripts/triage.sh "Test Patient" $$((20+$$i)) $$(( (RANDOM % 10) + 1 )) > /tmp/triage_fifo ; \
	done
	sleep 10
	./scripts/stop_hospital.sh

.PHONY: all clean run test
