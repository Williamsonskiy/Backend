FROM gcc:11.3

RUN apt-get update && \
    apt-get install -y \
      python3-pip \
      cmake \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install conan==1.59.0

COPY conanfile.txt /app/
WORKDIR /app

RUN conan install . --build=missing -s compiler.libcxx=libstdc++11

COPY . /app

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build

ENTRYPOINT ["/app/build/bin/game_server", "/app/data/config.json", "/app/static"]
