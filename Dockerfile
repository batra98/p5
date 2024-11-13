FROM --platform=linux/amd64 ubuntu:22.04
ENV TZ=Asia/Kolkata \
    DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential qemu qemu-system-x86 gdb git gawk expect
