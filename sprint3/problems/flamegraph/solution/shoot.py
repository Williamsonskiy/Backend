import argparse
import os
import random
import shlex
import signal
import subprocess
import time

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def build_flamegraph():
    # 1. Распаковываем сырые данные perf
    perf_script = subprocess.Popen(
        ['perf', 'script', '-i', 'perf.data'],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )

    # 2. Сворачиваем стеки вызовов с помощью Perl-скрипта stackcollapse-perf.pl
    stackcollapse_path = os.path.join('.', 'FlameGraph', 'stackcollapse-perf.pl')
    stackcollapse = subprocess.Popen(
        ['perl', stackcollapse_path],
        stdin=perf_script.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    perf_script.stdout.close()

    # 3. Генерируем итоговый graph.svg с помощью flamegraph.pl
    flamegraph_path = os.path.join('.', 'FlameGraph', 'flamegraph.pl')
    with open('graph.svg', 'w') as svg_file:
        flamegraph = subprocess.Popen(
            ['perl', flamegraph_path],
            stdin=stackcollapse.stdout,
            stdout=svg_file,
            stderr=subprocess.DEVNULL
        )
        stackcollapse.stdout.close()
        flamegraph.communicate()

    print('Flamegraph generated')


if __name__ == '__main__':
    # 1. Запускаем сервер
    server_cmd = start_server()
    server = run(server_cmd)
    
    # Небольшая пауза, чтобы сервер успел инициализироваться
    time.sleep(1)

    # 2. Запускаем perf record с привязкой к PID сервера
    perf_cmd = f'perf record -g -p {server.pid} -o perf.data'
    perf = run(perf_cmd)
    time.sleep(0.5)

    # 3. Выполняем обстрел запросами
    make_shots()

    # 4. Корректно останавливаем perf record, чтобы сбросить буфер в perf.data
    perf.send_signal(signal.SIGINT)
    perf.wait()

    # 5. Останавливаем сервер
    stop(server)

    # 6. Строим флеймграф
    build_flamegraph()

    print('Job done')
