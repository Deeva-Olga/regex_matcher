#!/bin/bash
set -e

# Создаем временную папку
mkdir -p build

# Компилируем движок с оптимизациями и C++17
g++ -O2 -std=c++17 -o build/regex_matcher regex_matcher.cpp

# Проверяем, что бинарник создан
if [ ! -f build/regex_matcher ]; then
    echo "Ошибка компиляции: убедитесь, что установлен g++"
    exit 1
fi

chmod +x build/regex_matcher

# Упаковываем в архив. Флаг -C build гарантирует, что файл ляжет в корень архива,
# а при распаковке в /opt/test_bins окажется по нужному пути.
tar -czvf test_bins.tar.gz -C build regex_matcher

echo "Готово: test_bins.tar.gz создан в корне репозитория"