#!/usr/bin/env sh

set -e

./postgres-docker-entrypoint.sh postgres -k /var/run/postgresql &
./wait-for-postgres.sh python3 bootstrap.py && python3 main.py
