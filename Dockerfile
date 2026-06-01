FROM alpine:3.20 AS builder

RUN apk add --no-cache \
    g++ \
    cmake \
    make \
    musl-dev

WORKDIR /app
COPY . .

RUN cmake -B build
RUN cmake --build build

# runtime

FROM alpine:3.20

RUN apk add --no-cache libstdc++

WORKDIR /app
COPY --from=builder /app/build/server .

EXPOSE 8080
CMD ["./webserver"]