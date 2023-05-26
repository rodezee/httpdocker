FROM alpine

RUN apk add --no-cache --virtual .compile build-base curl-dev

ADD . /root/

WORKDIR /root/

RUN make build

EXPOSE 8000

CMD ./httpdocker
