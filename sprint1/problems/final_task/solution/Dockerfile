FROM gcc:11-bookworm

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install --break-system-packages conan==1.*

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11

COPY . /app
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

RUN useradd -r -u 888 appuser
USER appuser

ENTRYPOINT ["/app/build/bin/game_server", "/app/data/config.json"]
