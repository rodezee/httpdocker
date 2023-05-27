FROM alpine

RUN apk add --no-cache --virtual .compile build-base curl-dev docker-cli

ADD Makefile /root/
ADD src /root/src

WORKDIR /root

RUN make build

EXPOSE 8000

CMD make run
