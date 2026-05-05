#!/bin/bash


set -e

if [ $# -ne 3 ]; then
    echo "Usage: $0 <name> <age> <severity 1-10>" >&2
    exit 1
fi

name="$1"
age="$2"
severity="$3"

if ! [[ "$name" =~ ^[A-Za-z\ ]+$ ]]; then
    echo "Error: Name must contain only letters and spaces." >&2
    exit 1
fi

if ! [[ "$age" =~ ^[0-9]+$ ]] || [ "$age" -lt 1 ] || [ "$age" -gt 120 ]; then
    echo "Error: Age must be an integer between 1 and 120." >&2
    exit 1
fi

if ! [[ "$severity" =~ ^[0-9]+$ ]] || [ "$severity" -lt 1 ] || [ "$severity" -gt 10 ]; then
    echo "Error: Severity must be an integer between 1 and 10." >&2
    exit 1
fi

if [ "$severity" -le 2 ]; then
    priority=1
elif [ "$severity" -le 4 ]; then
    priority=2
elif [ "$severity" -le 6 ]; then
    priority=3
elif [ "$severity" -le 8 ]; then
    priority=4
else
    priority=5
fi

echo "$name,$age,$severity,$priority"
