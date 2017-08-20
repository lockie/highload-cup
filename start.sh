#!/usr/bin/env sh

set -e

./postgres-docker-entrypoint.sh postgres -k /var/run/postgresql &
memcached -m 512 -t 4 -R 1 -C -X -u memcache -l 127.0.0.1 &
python3 bootstrap.py && python3 main.py
