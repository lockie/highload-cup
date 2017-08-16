#!/usr/bin/env sh
# inspired by https://docs.docker.com/compose/startup-order

set -e

while ! pg_isready -h /var/run/postgresql -d default > /dev/null; do
	sleep 1
done

sleep 1
exec "$@"
