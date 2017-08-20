# -*- docker-image-name: "highload" -*-

FROM python:alpine3.6

ENV PYTHONUNBUFFERED 1

ENV LANG en_US.utf8
ENV PG_MAJOR 9.6
ENV PG_VERSION 9.6.4
ENV PG_SHA256 2b3ab16d82e21cead54c08b95ce3ac480696944a68603b6c11b3205b7376ce13
ENV PATH /usr/lib/postgresql/$PG_MAJOR/bin:$PATH
ENV PGDATA /var/lib/postgresql/data
ENV POSTGRES_USER root
ENV POSTGRES_DB default
ENV MEMCACHED_VERSION 1.5.0
ENV MEMCACHED_SHA1 e12af93e63c05ab7e89398e4cfd0bfc7b7bff1c5

RUN apk add --no-cache dumb-init && \
    # install memcached, see docker-libary memcached/alpine/Dockerfile
    adduser -D memcache && \
    set -x \
    \
    && apk add --no-cache --virtual .memcached-build-deps \
        ca-certificates \
        coreutils \
        cyrus-sasl-dev \
        dpkg-dev dpkg \
        gcc \
        libc-dev \
        libevent-dev \
        libressl \
        linux-headers \
        make \
        perl \
        perl-utils \
        tar \
    \
    && wget -O memcached.tar.gz "https://memcached.org/files/memcached-$MEMCACHED_VERSION.tar.gz" \
    && echo "$MEMCACHED_SHA1  memcached.tar.gz" | sha1sum -c - \
    && mkdir -p /usr/src/memcached \
    && tar -xzf memcached.tar.gz -C /usr/src/memcached --strip-components=1 \
    && rm memcached.tar.gz \
    \
    && cd /usr/src/memcached \
    \
    && ./configure \
        --build="$(dpkg-architecture --query DEB_BUILD_GNU_TYPE)" \
        --enable-sasl \
    && make -j "$(nproc)" \
    \
    && make install \
    \
    && cd / && rm -rf /usr/src/memcached \
    \
    && runDeps="$( \
        scanelf --needed --nobanner --recursive /usr/local \
            | awk '{ gsub(/,/, "\nso:", $2); print "so:" $2 }' \
            | sort -u \
            | xargs -r apk info --installed \
            | sort -u \
    )" \
    && apk add --virtual .memcached-rundeps $runDeps \
    && \
    # install postgres, see docker-library postgres/9.6/alpine/Dockerfile
    set -ex; \
    postgresHome="$(getent passwd postgres)"; \
    postgresHome="$(echo "$postgresHome" | cut -d: -f6)"; \
    [ "$postgresHome" = '/var/lib/postgresql' ]; \
    mkdir -p "$postgresHome"; \
    chown -R postgres:postgres "$postgresHome" ; \
    mkdir /docker-entrypoint-initdb.d \
      && apk add --no-cache --virtual .fetch-deps \
          ca-certificates \
          openssl \
          tar \
      \
      && wget -O postgresql.tar.bz2 "https://ftp.postgresql.org/pub/source/v$PG_VERSION/postgresql-$PG_VERSION.tar.bz2" \
      && echo "$PG_SHA256 *postgresql.tar.bz2" | sha256sum -c - \
      && mkdir -p /usr/src/postgresql \
      && tar \
          --extract \
          --file postgresql.tar.bz2 \
          --directory /usr/src/postgresql \
          --strip-components 1 \
      && rm postgresql.tar.bz2 \
      \
      && apk add --no-cache --virtual .postgres-build-deps \
          bison \
          coreutils \
          dpkg-dev dpkg \
          flex \
          gcc \
          libc-dev \
          libedit-dev \
          libxml2-dev \
          libxslt-dev \
          make \
          openssl-dev \
          perl \
          perl-utils \
          perl-ipc-run \
          util-linux-dev \
          zlib-dev \
      \
      && cd /usr/src/postgresql \
      && awk '$1 == "#define" && $2 == "DEFAULT_PGSOCKET_DIR" && $3 == "\"/tmp\"" { $3 = "\"/var/run/postgresql\""; print; next } { print }' src/include/pg_config_manual.h > src/include/pg_config_manual.h.new \
      && grep '/var/run/postgresql' src/include/pg_config_manual.h.new \
      && mv src/include/pg_config_manual.h.new src/include/pg_config_manual.h \
      && gnuArch="$(dpkg-architecture --query DEB_BUILD_GNU_TYPE)" \
      && wget -O config/config.guess 'https://git.savannah.gnu.org/cgit/config.git/plain/config.guess?id=7d3d27baf8107b630586c962c057e22149653deb' \
      && wget -O config/config.sub 'https://git.savannah.gnu.org/cgit/config.git/plain/config.sub?id=7d3d27baf8107b630586c962c057e22149653deb' \
      && ./configure \
          --build="$gnuArch" \
          --enable-integer-datetimes \
          --enable-thread-safety \
          --enable-tap-tests \
          --disable-rpath \
          --with-uuid=e2fs \
          --with-gnu-ld \
          --with-pgport=5432 \
          --with-system-tzdata=/usr/share/zoneinfo \
          --prefix=/usr/local \
          --with-includes=/usr/local/include \
          --with-libraries=/usr/local/lib \
          \
          --with-openssl \
          --with-libxml \
          --with-libxslt \
      && make -j "$(nproc)" world \
      && make install-world \
      && make -C contrib install \
      \
      && runDeps="$( \
          scanelf --needed --nobanner --recursive /usr/local \
              | awk '{ gsub(/,/, "\nso:", $2); print "so:" $2 }' \
              | sort -u \
              | xargs -r apk info --installed \
              | sort -u \
      )" \
      && apk add --no-cache --virtual .postgresql-rundeps \
          $runDeps \
          bash \
          su-exec \
          tzdata \
      && apk del .fetch-deps \
      && cd / \
      && rm -rf \
          /usr/src/postgresql \
          /usr/local/share/man \
          /usr/local/share/doc \
          || true \
    && find /usr/local -name '*.a' -delete || true && \
    sed -ri "s!^#?(listen_addresses)\s*=\s*\S+.*!\1 = '*'!" /usr/local/share/postgresql/postgresql.conf.sample && \
    mkdir -p /var/run/postgresql && chown -R postgres:postgres /var/run/postgresql && chmod 2777 /var/run/postgresql && \
    mkdir -p "$PGDATA" && chown -R postgres:postgres "$PGDATA" && chmod 777 "$PGDATA"


EXPOSE 80

ENTRYPOINT ["/usr/bin/dumb-init", "--"]
CMD ./start.sh

COPY requirements.txt /usr/src/app/

RUN pip install --no-cache-dir -r /usr/src/app/requirements.txt && \
    python -O -m compileall -q /usr/src/app /usr/local/lib/python3.6 && \
    apk del .postgres-build-deps .memcached-build-deps


COPY . /usr/src/app
WORKDIR /usr/src/app
