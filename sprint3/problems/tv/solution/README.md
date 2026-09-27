# 📺 TV

## 📌 Описание

Практическая задача Sprint 3 посвящена разработке полноценной модели телевизора и контроллера команд.

В отличие от предыдущих уроков, где тестировались отдельные методы класса `TV`, здесь появляется полноценное приложение:

```text
TV
↓
Controller
↓
Menu
↓
Консольный интерфейс
```

Решение использует только Catch2, поскольку по условию достаточно одного тестового фреймворка.

---

## 🧠 Что изучается

| 📌 Тема          | 📖 Что реализуется                     | ✅ Результат                          |
| ---------------- | -------------------------------------- | ------------------------------------ |
| TV               | Состояние телевизора и логика каналов  | Полноценная модель устройства        |
| Controller       | Обработка команд пользователя          | Связка Menu и TV                     |
| Menu             | Выполнение текстовых команд            | Консольный интерфейс                 |
| Catch2           | Unit и интеграционные тесты            | Проверка поведения системы           |
| Exceptions       | `logic_error`, `out_of_range`          | Корректная обработка ошибок          |
| Previous Channel | Переключение между последними каналами | Поведение как у реального телевизора |

---

## 📂 Структура решения

```text
solution/
├── CMakeLists.txt
├── conanfile.txt
├── src/
│   ├── main.cpp
│   ├── tv.h
│   ├── menu.h
│   └── controller.h
└── tests/
    ├── catch_tv_tests.cpp
    └── catch_controller_tests.cpp
```

Используются только тесты Catch2. Остальные тестовые файлы из precode не подключаются к сборке.

---

## 📦 conanfile.txt

```ini
[requires]
catch2/3.1.0

[generators]
cmake_multi
```

---

## ⚙️ CMakeLists.txt

Проект собирает два исполняемых файла:

```text
app
catch_tv_tests
```

* `app` — основная программа;
* `catch_tv_tests` — тесты.

```cmake
add_executable(app ...)
add_executable(catch_tv_tests ...)
target_link_libraries(catch_tv_tests PRIVATE CONAN_PKG::catch2)
add_test(NAME catch_tv_tests COMMAND catch_tv_tests)
```

---

# 📺 Класс TV

Класс хранит состояние:

```cpp
bool is_turned_on_ = false;
int channel_ = 1;
int last_channel_ = 1;
```

---

## Поддерживаемые операции

```cpp
TurnOn()
TurnOff()
SelectChannel()
SelectLastViewedChannel()
GetChannel()
IsTurnedOn()
```

---

## Выбор канала

При выборе нового канала:

```cpp
SelectChannel(15);
```

старый канал сохраняется:

```cpp
last_channel_ = channel_;
channel_ = 15;
```

---

## Предыдущий канал

Переключение между последними каналами выполняется через:

```cpp
std::swap(channel_, last_channel_);
```

Поэтому последовательность:

```text
8
↓
15
↓
Previous
↓
8
↓
Previous
↓
15
```

работает автоматически.

---

## Диапазон каналов

Допустимые значения:

```cpp
TV::MIN_CHANNEL = 1
TV::MAX_CHANNEL = 99
```

Попытка выбрать:

```text
0
100
```

вызывает:

```cpp
std::out_of_range
```

---

## Выключенный телевизор

Если телевизор выключен:

```cpp
tv.SelectChannel(...)
```

или

```cpp
tv.SelectLastViewedChannel()
```

выбрасывается:

```cpp
std::logic_error
```

---

# 🎛️ Controller

`Controller` не хранит состояние телевизора.

Он только связывает:

```text
Menu
↓
TV
```

и регистрирует команды:

```text
Info
TurnOn
TurnOff
SelectChannel
SelectPreviousChannel
```

---

## Пример

Команда:

```text
SelectChannel 8
```

превращается в вызов:

```cpp
tv_.SelectChannel(8);
```

Команда:

```text
TurnOn
```

превращается в:

```cpp
tv_.TurnOn();
```

---

## Сообщения об ошибках

Контроллер преобразует исключения в понятные сообщения:

```text
TV is turned off
Channel is out of range
Invalid channel
```

---

# 🧪 Тестирование

Используется Catch2.

---

## catch_tv_tests.cpp

Проверяет непосредственно класс TV:

```text
TV выключен по умолчанию
TV не показывает канал в выключенном состоянии
После включения показывает канал 1
Можно выбрать канал 1..99
Нельзя выбрать канал вне диапазона
Запоминает канал после выключения
Поддерживает предыдущий канал
```

---

## catch_controller_tests.cpp

Проверяет взаимодействие:

```text
Controller
+
Menu
+
TV
```

Например:

```text
Info
SelectChannel 8
TurnOff
```

и корректность текстового вывода пользователю.

---

# 🏗️ Сборка

```bash
cd ~/cppbackend/sprint3/problems/tv/solution

rm -rf build

mkdir build

cd build

source ~/conan-venv/bin/activate

conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

cmake .. -DCMAKE_BUILD_TYPE=Release

cmake --build .
```

---

# ▶️ Запуск тестов

Запустить напрямую:

```bash
./catch_tv_tests
```

или через CTest:

```bash
ctest --output-on-failure
```

---

# 🖥️ Ручная проверка

Запустить приложение:

```bash
./app
```

Последовательно выполнить:

```text
Info
TurnOn
Info
SelectChannel 8
Info
SelectChannel 15
Info
SelectPreviousChannel
Info
TurnOff
Info
Exit
```

Ожидаемая логика:

```text
TV is turned off
↓
TurnOn
↓
Channel 1
↓
Channel 8
↓
Channel 15
↓
Previous → Channel 8
↓
TurnOff
↓
TV is turned off
```

---

# 🏁 Итог

Проект объединяет несколько тем Sprint 3:

```text
TDD
↓
TV
↓
Controller
↓
Menu
↓
Catch2
↓
Интеграционное тестирование
```

Главная идея задачи:

```text
Бизнес-логика должна находиться в TV,
Controller должен только связывать команды пользователя
с методами модели.
```

---

## ⬅️ Назад

[Вернуться к Testing and Debugging](../../../../lessons/sprint_19_20/testing_and_debugging/README.md)

[Вернуться в общий README репозитория](../../../../README.md)
