FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt install -y \
      build-essential \
      g++ \
      cmake \
      python3-pip \
    && \
    pip3 install conan==1.*

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11

COPY . /app
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

RUN useradd -r -u 888 appuser
USER appuser

# Важно: оставляем только бинарник. Все аргументы передадут сами автотесты
ENTRYPOINT ["/app/build/bin/game_server"]
