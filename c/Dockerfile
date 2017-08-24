FROM alpine:3.6

ENV LANG en_US.utf8

RUN apk add --no-cache --virtual .builddeps gcc tcl make cmake musl-dev file

EXPOSE 80

CMD ./server

COPY . /usr/src/app
WORKDIR /usr/src/app

RUN mkdir /usr/src/app/build && cd /usr/src/app/build && \
    cmake .. && make && cp server .. && \
    apk del .builddeps
