FROM alpine:3.6

ENV CPROXY_VERSION 4.1-dev

COPY src/tcp_proxy.c c-proxy/tcp_proxy.c

WORKDIR /c-proxy

RUN mkdir bin

RUN apk --no-cache add --update \
    bash \
    gcc \
    tcpdump \
    iproute2 \
    build-base \
    supervisor

RUN gcc -pthread -o bin/c_proxy_daemon tcp_proxy.c

COPY config/supervisord.conf /etc/supervisor/conf.d/supervisord.conf
CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/conf.d/supervisord.conf"]
