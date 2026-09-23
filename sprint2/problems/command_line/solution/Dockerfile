# 1. Стадия сборки (Build stage)
FROM gcc:11.3 AS build

# Удаляем проблемные репозитории security, обновляем apt и устанавливаем зависимости
RUN sed -i '/security/d' /etc/apt/sources.list && \
    apt update && \
    apt install -y \
        python3-pip \
        cmake \
    && \
    pip3 install conan==1.*

# Настройка профиля Conan
RUN conan profile new default --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем конфигурацию Conan и устанавливаем зависимости
COPY conanfile.txt /app/
WORKDIR /app
RUN mkdir build && cd build && \
    conan install .. --build=missing -s build_type=Release

# Копируем исходный код проекта и собираем
COPY . /app
RUN cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# 2. Финальная стадия запуска (Run stage)
FROM ubuntu:22.04 AS run

# Создаём пользователя без привилегий
RUN groupadd -r www && useradd -r -g www www

# Копируем собранный исполняемый файл и статические ресурсы
WORKDIR /app
COPY --from=build /app/build/bin/game_server /app/bin/game_server
COPY ./static /app/static
COPY ./data /app/data

# Переключаемся на непривилегированного пользователя
USER www

# Точка входа: только бинарник, аргументы по умолчанию — в CMD
ENTRYPOINT ["/app/bin/game_server"]
CMD ["-c", "/app/data/config.json", "-w", "/app/static"]
