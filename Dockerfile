# Multi-stage Dockerfile for C HTTP Backend & Frontend
FROM alpine:3.19 AS builder
RUN apk add --no-cache gcc musl-dev make
WORKDIR /app
COPY . .
RUN make -B server

FROM alpine:3.19
RUN apk add --no-cache musl
WORKDIR /app
COPY --from=builder /app/bin/delivery_server /app/bin/delivery_server
COPY --from=builder /app/frontend /app/frontend
COPY --from=builder /app/database /app/database

ENV PORT=8080
EXPOSE 8080
CMD ["./bin/delivery_server"]

