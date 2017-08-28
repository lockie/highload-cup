FROM alpine:3.6

ENV LANG en_US.utf8

RUN apk add --no-cache --virtual .builddeps gcc tcl make cmake musl-dev file gengetopt

EXPOSE 80

CMD /usr/local/bin/server -v --phase-hack -p 80 -d /tmp/data/data.zip

COPY . /usr/src/app
WORKDIR /usr/src/app

RUN mkdir /usr/src/app/build && cd /usr/src/app/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && make && cp server /usr/local/bin/server && \
    cd .. && rm -fr build && apk del .builddeps && du -h /usr/local/bin/server
