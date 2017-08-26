FROM alpine:3.6

ENV LANG en_US.utf8

RUN apk add --no-cache --virtual .builddeps gcc tcl make cmake musl-dev file

EXPOSE 80

CMD /usr/local/bin/server

COPY . /usr/src/app
WORKDIR /usr/src/app

RUN mkdir /usr/src/app/build && cd /usr/src/app/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && make && cp server /usr/local/bin/server && \
    apk del .builddeps
