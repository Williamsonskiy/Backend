FROM gcc:11.3 as build

RUN apt-get update --fix-missing && \
    apt-get install -y --no-install-recommends \
      build-essential \
      python3-pip \
      cmake \
    && \
    rm -rf /var/lib/apt/lists/* && \
    pip3 install conan==1.*

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11

COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04 as run

RUN groupadd -r www && useradd -r -g www www

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

USER www

ENTRYPOINT ["/app/game_server", "-c", "/app/data/config.json", "-w", "/app/static"]
