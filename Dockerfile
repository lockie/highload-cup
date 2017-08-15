# -*- docker-image-name: "highload" -*-

FROM python:alpine3.6

ENV PYTHONUNBUFFERED 1

RUN apk add --no-cache dumb-init

COPY . /usr/src/app
WORKDIR /usr/src/app

RUN pip install --no-cache-dir -r /usr/src/app/requirements.txt

EXPOSE 80

ENTRYPOINT ["/usr/bin/dumb-init", "--"]
CMD ./start.sh
