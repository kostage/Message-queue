# Решение задачки про MessageQueue

Я написал приложение из задания.
Оно запускает на некоторое время 2 потока записи в очередь
и 1 поток чтения.

Для того, чтобы потоки записи засыпали при достижении HWM,
используется статическая переменная состояния, на которой спят все
потоки записи.

Собрать на линуксе можно командой
```
$ make
$ ./app
```
# Тесты
В папке тест поинтереснее. Там 4 теста:
1. Тест приоритетов.
Очередь полностью расходящимся рядом чисел каждое с большим приоритетом,
чем предыдущее. Затем достаем элементы из очереди, проверяя, что они выходят
в обратном порядке.
2. Тест на потокобезопасность. 
Запускаю 2 потока чтения и 2 потока записи. С помощью статических атомарных 
счетчиков контролирую, что кол-во элементов вошедших в очередь равно кол-ву
вышедших.

3. То же, что предыдущее, только с установкой нотификаторов методом
set_events(). Нотификатор ничего не делает, но проверка необходима, потому что
работает другой код.

4. Проверка вотермарков и старт/стоп нотификаторов.
Устанавливаю нотификаторы:
 - on_stop - nop
 - on_start - запускает поток записи
 - on_hwm - останавливает поток записи, запускает поток чтения
 - on_lwm - останавливает поток чтения, останавливает очередь

Вдобавок каждый нотификатор инкрементирует атомарный счетчик вызовов

Стартую очередь, жду окончания работы потоков, проверяю, что каждый 
нотификатор был вызван ровно один раз.

## Запуск тестов.
Makefile страшный. Собирает библиотеку gtest зачем-то.

Путь к gtest захардкожен внутри Makefile.
Поменять на свой:
```
GTEST_DIR = /home/kostage/googletest/googletest
```

```
$ cd test
$ make
$ ./tests
```

