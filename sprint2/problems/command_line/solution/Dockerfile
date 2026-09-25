# Используем ту же ОС, что и для run-стадии
FROM ubuntu:22.04 as build

RUN apt-get update --fix-missing && \
    apt-get install -y --no-install-recommends \
      build-essential \
      python3-pip \
      cmake \
    && \
    rm -rf /var/lib/apt/lists/* && \
    pip3 install conan==1.*

# Сначала зависимости
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11

# Потом исходники
COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй stage: только запуск
FROM ubuntu:22.04 as run

# Создаём отдельного пользователя
RUN groupadd -r www && useradd -r -g www www

# Копируем собранный бинарник из stage build
COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

USER www

# Команда запуска контейнера
ENTRYPOINT ["/app/game_server", "-c", "/app/data/config.json", "-w", "/app/static"]
