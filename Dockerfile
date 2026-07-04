FROM alpine:3.20 AS builder

RUN apk add --no-cache \
    g++ \
    cmake \
    make \
    musl-dev \
    python3

WORKDIR /app
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j$(nproc)

# ---- Runtime stage ----
FROM alpine:3.20

RUN apk add --no-cache libstdc++

WORKDIR /app

COPY --from=builder /app/build/webserver .
COPY config.json .
COPY www/ ./www/

EXPOSE 8081
CMD ["/bin/sh", "-c", "exec ./webserver > /dev/null 2>&1"]
