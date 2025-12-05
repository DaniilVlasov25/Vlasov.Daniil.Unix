FROM alpine
RUN apk add -q util-linux
COPY script.sh /s
RUN chmod +x /s
CMD ["/s"]
