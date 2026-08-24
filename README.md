# Black Box
![Logo](media/Black_box.png)

![Version](https://img.shields.io/github/v/tag/Rozmusel/Black_box)
![License](https://img.shields.io/github/license/Rozmusel/Black_box)

Телеграмм бот для хранения лекций и семинаров и получения доступа к ним

Бот размещён здесь @RZM_Black_box_bot

## Статус
- [x] Локальный ТГ сервер
- [x] Регистрация пользователей
- [x] Загрузка материалов
- [x] Скачивание размещённых материалов
- [ ] Скачивание интервалов и полного списка
- [ ] Настройки
- [x] Администраторская
- [ ] Альтернативная загрузка (ссылка на Яндекс диск)

## Запуск
### Windows
```shell
vcpkg install
cmake --preset windows
cmake --build build
```
### Linux
```shell
vcpkg install
cmake --preset linux
cmake --build build
```

### Сервер
Инструкция по [сборке](https://tdlib.github.io/telegram-bot-api/build.html)

[Получение](https://my.telegram.org/apps) api id и api hash

Если возникают ошибки при попытке получить api id и api hash
1. Отключить VPN
2. Открыть файл C:\Windows\System32\drivers\etc\hosts
3. Добавить строку в конец файла 149.154.167.220 my.telegram.org
4. Сохранить файл
5. Перезапустить браузер

Запускать сервер командой:

```
telegram-bot-api --local --api-id=<api_id> --api-hash=<api_hash> --dir=<path_to_files>
```

![Logo](media/logo.png)