#!/usr/bin/env sh

set -e

./postgres-docker-entrypoint.sh postgres -k /var/run/postgresql &
python3 bootstrap.py && python3 main.py
