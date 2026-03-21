# Stalcraft Auction Bot

Десктопное приложение для анализа аукциона Stalcraft. Помогает находить выгодные сделки для перекупства.

## Зависимости

- **Qt 6** (Widgets, Network, Charts)
- **PostgreSQL** (libpq)
- **spdlog** (логирование)
- **nlohmann/json** (парсинг JSON)
- **CMake 3.20+**

## Установка зависимостей (Windows / vcpkg)

```bash
vcpkg install qt6-base qt6-charts spdlog nlohmann-json libpq
```

## Сборка

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## Настройка базы данных

1. Установите PostgreSQL
2. Создайте базу данных:
```sql
CREATE DATABASE stalcraft_bot;
```
3. Таблицы создаются автоматически при первом запуске (миграция из `sql/schema.sql`)

## Первый запуск

1. Запустите приложение
2. Перейдите на вкладку **Настройки**
3. Укажите Bearer access_token для Stalcraft API (сохраняется через DPAPI в профиле Windows; альтернатива — переменная окружения `STALCRAFT_API_BEARER`)
4. Настройте подключение к PostgreSQL
5. Перейдите на вкладку **Предметы** и добавьте предметы для отслеживания

## Загрузка каталога предметов

Каталог загружается из [stalcraft-database](https://github.com/EXBO-Studio/stalcraft-database/tree/main/ru/items).
Можно загрузить из локальной копии репозитория или скачать через GitHub API (кнопка в интерфейсе).

## Структура проекта

```
src/
  core/     - бизнес-логика (API, БД, анализ, планировщик)
  models/   - структуры данных (Item, Lot, PriceSnapshot, Alert)
  ui/       - виджеты Qt (таблицы, графики, настройки)
  utils/    - утилиты (логирование)
sql/        - DDL-схема PostgreSQL
```
