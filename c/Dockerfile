FROM alpine:3.6

ENV LANG en_US.utf8

RUN apk add --no-cache --virtual .builddeps gcc tcl make cmake musl-dev glib-dev jemalloc-dev file gengetopt &&\
    apk add --no-cache glib jemalloc curl

EXPOSE 80

COPY . /usr/src/app
WORKDIR /usr/src/app

RUN mkdir /usr/src/app/build && cd /usr/src/app/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && make -j5 && cp server /usr/local/bin/server && \
    cd .. && rm -fr build && apk del .builddeps && du -h /usr/local/bin/server

CMD /usr/local/bin/server -v -p 80 -d /tmp/data/data.zip
