# Не просто создаём образ, но даём stage имя build
FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
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
