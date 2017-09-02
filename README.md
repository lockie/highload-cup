С 10 августа по 31 августа 2017 года Mail.Ru Group впервые проводил соревнование разработчиков высоконагруженных систем [Highload Cup](https://highloadcup.ru).

> &mdash; Андрей, смотри, какая погода хорошая на улице, сходил бы прогулялся.
![кадр из к/ф Брат-2](/images/brother.jpg)
> &mdash; Не могу, я принимаю участие в соревновании разработчиков высоконагруженных систем.

Ниже - отчёт о моём участии в этом увлекательном мероприятии.

* [Техзадание](#Техзадание)
* [План](#План)
* [Хроника событий](#Хроника-событий)
* [Итоги](#Итоги)
* [Идеи для следующего Highload cup](#Идеи-для-следующего-highload-cup)


Сначала кратко продублирую техзадание.

## Техзадание
Имеются записи о трех сущностях: *User*, *Location* и *Visit*. Эти сущности описывают путешествия людей по разным достопримечательностям и могут быть основой для небольшого сервиса "В помощь путешественнику". Они содержат данные о собственно профиле пользователя, достопримечательности и посещении конкретным пользователем конкретного места.

API - это методы, которые должен обслуживать разработанный участником сервер, по протоколу HTTP.

### Методы выборки данных (GET):
* Получение данных о сущности: `/<entity>/<id>`, `<entity>` принимает одно из значений - `users`, `locations` или `visits`.
* Получение списка мест, которые посетил пользователь: `/users/<id>/visits`.
* Получение средней оценки достопримечательности: `/locations/<id>/avg`.

### Методы обновления данных (POST):
* Обновление данных о сущности: `/<entity>/<id>`.
* Добавление новой сущности: `/<entity>/new`.

Подробное техзадание размещено [здесь](https://highloadcup.ru/media/condition/howto.html).

## План
После прочтения ТЗ в голове возникло сразу несколько планов. Писать хотел на Python, но и от сурового очарования чистого C тоже не стал бы отказываться.

Для питонов я заготовил следующую программу:
* Давно хотел потыкать веточкой в стек aio-libs, а PostgreSQL я давно знаю, умею и практикую, поэтому aiohttp + aiopg [через sqlalchemy](https://aiopg.readthedocs.io/en/stable/sa.html).
* Позднее обернуть эту требуху в Gunicorn, чтобы утилизировать многопоточность (на тестовом стенде организаторы заявили 4-ядерный Xeon).
* Ещё позднее - поставить вперёд Redis/Memcached.
* Гораздо позднее - переписать всё на сишечке с использованием libevent и libpq.

Для сишечки были следующие идеи:
* Возможно MongoDB уже не так ужасна, как пяток лет назад, когда я в последний раз тыкал в неё веточкой, и стоит попробовать её новый механизм Intent locks.
* Можно было бы даже спрятать монгу за фронт-эндом в виде Redis или Memcached.
* Использовать Redis, через сишные либы https://github.com/redis/hiredis + https://github.com/aclisp/hiredispool . Идея была в том, чтобы тупо запихать все сущности в HSET'ы.
* После чтения [SO](https://stackoverflow.com/questions/10558465/memcached-vs-redis) мне стал больше импонировать Memcached, т.к. это тупо список пар ключ-значение в памяти, без возможности записи на диск и всяких новомодных структур данных, как в Redis - Memcached прост, как табуретка, делает одно дело и делает его хорошо, ровно на таких инструментах и стоит пресловутый UNIX way.
* Затем я склонился к идее использования Postgres за Memcached, т.к. с Postgres у меня уже есть опыт работы.
* Была мысль таки попробовать Tarantool, но я решил, мол, соревнование же устраивает mail.ru, Tarantool - их инструмент, поэтому это будет какой-то флюродрос. Нет уж, нормальные герои всегда идут в обход :sweat_smile:
* Что касается сетевого стека, для сишечки выбор был между библиотеками libmicrohttpd и libevent. В [этом](https://habrahabr.ru/post/207460) бенчмарке прочитал, что, мол, libmicrohttpd сегфолтится при нагрузках, поэтому склонился к libevent (хоть последняя из коробки и не умеет работать в многопоточном режиме, и надо закатывать солнце вручную с pthreads).


## Хроника событий

### Воскресенье, 13 августа
* Увидел на хабре [объявление](https://habrahabr.ru/company/mailru/blog/335384) о старте конкурса, обрадовался, зарегистрировался.
* Запушил [контейнер с hello world](https://github.com/lockie/highload-cup/commit/02ea34510ddec0910678df581ec5e66e692f34d0#diff-5bc02cefb3ea9e27f1a6776eabd1935d) на aiohttp в тестовую систему, долго смеялся, когда получил в ответ ~13% верных ответов. Подумав, понял, что это из-за совпадения в некоторых тестах требуемого кода возврата `404` с кодом, который hello world выдавал на все URL'ы, кроме `/`.

### Среда, 16 августа
* Добавил в Docker-контейнер PostgreSQL и [dumb-init](https://github.com/Yelp/dumb-init). Последнее было бы необязательно, если бы организаторы разрешили посылать конфигурации docker-compose, или использовать тот механизм оркестрации, который недавно встроили в ядро Docker, Swarm или как его там; однако нет, пришлось ютиться в одном контейнере.
* Добавил простенький скрипт для загрузки данных.
* Добавил SQLAlchemy'шные модели. Увлёкшись, не сразу заметил, что алхимический Query Builder, к использованию которого [принуждают](http://aiopg.readthedocs.io/en/stable/sa.html) авторы aiopg, за милую душу жрёт привычных  мне наследников `declarative_base()` вместо инстансов `sa.Table`. Удобно, чо.

### Пятница, 18 августа
* Запустил почти готовое решение на тестирование, но у него те же ~10% верных ответов, что и у хелловорлда. Расчехляю яндекс танк.
* Яндекс танк у меня так толком и не заводится; забегая вперёд, соревнование было великолепной антирекламой этого программного продукта. Зато заводится [тестилка](https://github.com/AterCattus/highloadcup_tester), написанная на Go конкурсантом Алексеем Акуловичем ([@AterCattus](https://github.com/AterCattus)), за которую ему вообще хочется выразить огромное человеческое спасибо.
* Начал с нарастающим отчаянием разбираться, почему у меня так мало верных ответов.
* На запросы вида `/users/badstringnotid` я, оказывается, должен отдавать код `404`, а не `400`. [Понятная картинка](http://www.netlore.ru/upload/files/1307/large_p17gnj292p1utdsa1112gebca6a4.jpeg).
* По ходу разбирательств починил проблемы с быстродействием: из-за ненастроенного `.dockerignore` я каждый раз отправлял Docker-демону огромный каталог с тестовыми данными, да ещё и каталоги `__pycache__` впридачу.
* `aiopg` съедает 36M оперативки и потихоньку тикает памятью с каждым запросом. Либо, что скорее, у меня где-то лыжи не едут.


### Суббота, 19 августа
* Ещё одно неприятное открытие: всё это время я использовал настройки PostgreSQL по умолчанию (которые у него заданы с тем расчётом, чтобы работать на компьютерах с 256M памяти). Впрочем, после втыкания правильного конфига всё равно 14% правильных ответов.
* Ближе к полуночи поныл в [телеграм-чяте](https://t.me/highloadcup), и, вняв намёку организаторов, начал писать гневный issue на [Github](https://github.com/sat2707/hlcupdocs/issues) и понял, что я ВСЁ ЭТО ВРЕМЯ ПУШИЛ DOCKER-КОНТЕЙНЕР НЕ В ТОТ ТЭГ. Тестирующая система снова и снова прогоняла через тесты тот самый первый hello world. Кто идиот? :raised_hand:
* К первому часу ночи разобрался с отправкой, 97% верных ответов, 186 место из 219.

### Воскресенье, 20 августа
* Починил SQL JOIN на запросе locations/avg, почти все тесты зелёные.
* За вечер впилил memcached через [aiomcache](https://github.com/aio-libs/aiomcache) :blush: Правда, стало ещё хуже. Вовсю сыпятся ошибки `accept4(): No file descriptors available`, я даже не могу понять, откуда. Решил, чтобы не ждать очередных 12 часов, тестировать у себя под нагрузкой.

### Понедельник, 21 августа
* Сделал конфиг для [siege](https://www.joedog.org/siege-home) и небольшой [скрипт](https://github.com/lockie/highload-cup/commit/b4077b2370430ce15413514b7d717ccc616b85b7#diff-de6dd4b4c889fe0882cfd3f6df5aa451), создающий `urls.txt` для него.
* Попробовал впилить в aiomcache поддержку подключения к memcached через UNIX domain socket, но нахрапом, простой заменой [`asyncio.open_connection`](https://docs.python.org/3/library/asyncio-stream.html#asyncio.open_connection) на [`asyncio.open_unix_connection`](https://docs.python.org/3/library/asyncio-stream.html#asyncio.open_unix_connection) в [соответствующем месте](https://github.com/aio-libs/aiomcache/blob/8800a6ea42d242dcbcb099e805e5cb9f4190e9d1/aiomcache/pool.py#L72) не получилось - вызовы `recv()` в aiomcache начали валиться с ошибкой 22 invalid argument. Решил забить.
* Решил попробовать Gunicorn, чтобы утилизировать все ядра процессора. *На заметку*: приложением Gunicorn'у можно передавать в том числе и произвольный питоновый callable, как, например, [тут](https://stackoverflow.com/q/39084433/1336774) `app:run()`. Заодно нашёл [клёвую презентацию](http://igordavydenko.com/talks/lvivpy-4) об Asyncio стеке, из которой, среди всего прочего, подцепил метод инициализации базы данных в aiohttp-приложении.


### Вторник, 22 августа
* В итоге Gunicorn разочаровал, т.к. ощутимого прироста производительности не дал, но навалились в полный рост проблемы с пулами соединений к PostgreSQL и Memcache.
* Да и Memcache не особо помогает с производительностью, хотя, скорее всего, я его просто неправильно готовил.
* Решил заново написать решение на C, в один поток, с libevent и in-memory SQLite, чтобы занять хотя бы какое-нибудь приличное место в рейтинге.

---

### Четверг, 24 августа
* Наткнулся на прекрасный веб-сервис - [REPL для C](https://repl.it/languages/c) :laughing: Там, кстати, и другие языки есть.
* После того, как дописал URL роутер, чуть менее, чем целиком, состоящий из `strcmp`, `isdigit` и такой-то матери, при тестовом запуске моё сишное решение даже ни разу не упало. Это успех :blush:
* Тестил сишные JSON-парсеры для разбора тел POST-запросов. Выбор был между [frozen](https://github.com/cesanta/frozen) и [cJSON](https://github.com/DaveGamble/cJSON). В итоге в синтетическом тесте по миллиону выцепливаний строки из JSON'a победил cJSON - 1.75с против 2.95с, его и взял.
* Добавил SQLite. Вволю поразвлекался с CMake, в итоге получился [CMakeLists.txt](https://github.com/lockie/highload-cup/blob/db4e0ac6c77cea94ad6feba65c5a283fc417e2eb/c/vendor/sqlite/CMakeLists.txt) для добавления SQLite с amalgamation в проект, который не стыдно и в другом проекте использовать.
* Запустил решение в Docker-контейнере - сегфолтнулось внутри `strlen`. Выглядит так, будто функция `strdup` косячит.

### Пятница, 25 августа
* Увидел в чате на скриншоте у парня с тарантулом 800 штрафных секунд. В тот момент я подумал, что если попаду в первую сотню, то буду вообще герой :)
* Починил вчерашний сегфолт, оказывается, установка `-std=c11` вместо `-std=gnu11` ломает `strdup`. Чудеса, да и только.
* Починил течи памяти, которых уже успел напускать в код :)
* Для запихивания данных в SQLite хотел было сначала поступить примерно так же, как и с питоном - сконвертировать всё на лету в три больших `.csv`-файла, а потом загрузить их через [CSV virtual table](https://sqlite.org/csv.html), но потом понял, что при статической линковке со SQLite-овым loadable extension будет многовато возни, поэтому решил сделать совершенно по рабоче-крестьянски - одну огромную транзакцию, в которой будут `INSERT`'ы для всех сущностей из архива. А работу с архивом через [miniz](https://github.com/uroni/miniz) - это вообще однофайловая библиотека.
* Ура, сделал импорт данных о юзерах! И до кучи отладочный URL для тыкания веточкой в SQLite. Словами не передать, сколько удовольствия я испытал, увидев, как моё решение живёт своей жизнью: ![SQLite screenshot](/images/SQL.png) На питоне это всё слишком просто и высокоуровнево, а на сишечке - прямо чуствуешь, как между тобой и машиной только тончайшая прослойка в виде незамысловатого компилятора, и ты этим довольно топорным инструментом гнёшь машину своей волей, как тебе заблагорассудится. Однажды испытав такое, бросить писать на C невозможно :)

### Суббота, 26 августа
* Начал день с рефакторинга :blush:
* Добавил методы для чтения/записи сущностей.

### Воскресенье, 27 августа
* 
```
if(params.country)
	params.country = strdup(params.country);
```
ЭТАШИДЕВР :sweat_smile: 
* Запутался в аллокациях памяти, да ещё и, кажется, заболел гриппом :cold_sweat:
* Метод user/visits работал катастрофично долго - 50 секунд на запрос. Переделал запрос к базе на `INNER JOIN`, подхимичил внутрянку SQLite (особенно убило его настойчивое желание писать на диск временные файлы при том, что база данных - `:memory:`), задал вопрос на [SO](https://stackoverflow.com/q/45904581/1336774) о prepared statement'ах, стало вполне сносно. Решил допилить второй и последний из сложных методов - locations/avg. Сделать его удалось неожиданно быстро.
* Добавил себе крутой парсинг параметров командной строки через инструмент [Gengetopt](https://www.gnu.org/software/gengetopt/gengetopt.html) просто потому, что могу.
* Глубокой ночью отправил решение на рейтинговый тест. Производительность довольно позорная, 95 тысяч секунд штрафа, 104 место, даже хуже, чем с PostgreSQL. Самая задница на последней фазе теста, где на 75 секунде из 120 приложение просто захлёбывается. Рискнул предположить, что из-за фрагментации памяти - SQLite, как утверждается в [этом](https://github.com/Restream/reindexer/tree/master/benchmarks) бенчмарке, сверх всякой меры дёргает `malloc` во время выполнения запроса. Решил завтра попробовать использовать встроенный в SQLite без-malloc-овый менеджер памяти `MEMSYS5`, за которым, как выяснилось, стоит [серьёзная научная статья](https://sqlite.org/malloc.html#nofrag).
* По ходу дела выяснил, что у меня на рабочей машине в ядре выключено `TRANSPAREHT_HUGETLB`, а это может повышать производительность. Перекомпилил ядро :)

### Понедельник, 28 августа
* Вовсю оптимизировал SQLite.
* Лол, `SQLITE_CONFIG_HEAP` принимает аргумент "длина" как `int`, поэтому больше двух гигабайт памяти ему не скормить. Хотя мне-то все равно хватит, данные для рейтингового теста спокойно влезают в ~700M.
* Нашёл прелюбопытнейший баг в SQLite-овском MEMSYS5: если в `realloc` ему передать нулевой указатель, он падает, пытаючись его разыменовать :sweat: Там даже в начале функции ассерт торчит: `assert( pPrior!=0 );` зачем так жить, пацаны?
* Ещё прекрасное: любое другое значение для параметра округления размера `MEMSYS5`, кроме `256`, вызывает крэш. Хотел поэкспериментировать, какое круче, но, видать, не судьба.
* Экспериментировал с выделением памяти своим приложением, случайно запустил его с параметром, заставляющим его выделить себе 8G рамы под пул памяти, чем отправил себе всю систему на десяток минут в глубокий своп. Кое-как прибил с tty1, стало занятой RAM 800М физической и 1,4G свопа - никогда такого не видел :)
* Со включённым Link-time optimization GCC не умеет генерировать ассемблерные листинги, в которые я регулярно одним глазком поглядывал. Грусть, печаль.
* Открыл для себя http://overload.yandex.ru, дизайном понравилось. Позабавило, что кнопкой на сайте останавливается локально бегущий у меня Docker-контейнер. Контейнер с яндекс-танком у меня на машине, кстати, еле ворочается, регулярно триггеря OOM, хотя у меня памяти 8G.
* После всех оптимизаций приложение захлёбывается не на 75, а на 89 секунде. Сдаётся мне, что дело не во фрагментации памяти.

### Вторник, 29 августа
* Решил попробовать прикрутить потоки и быстренько сделал. Кто молодец? :raised_hand:
* Намного лучше от потоков не стало - захлёбывается на 106 секунде, зато появилась проблема с синхронизацией - `sqlite3_reset`, `sqlite3_bind` и `sqlite3_step` сами по себе потокобезопасные, но их все равно всей пачкой нужно защищать mutex'ом, в противном случае начинают валиться ошибки типа: спрашивали `/users/10104`, а получили юзера с id=10215.
* Впрочем, у меня появилась гениальная идея - выкинуть SQLite и тупо распихать все сущности по массивам.

### Среда, 30 августа
* Взял GLib и быстро, решительно переписал хранение данных на [`GPtrArray`](https://developer.gnome.org/glib/stable/glib-Pointer-Arrays.html) и [`GSequence`](https://developer.gnome.org/glib/stable/glib-Sequences.html).
* С последними довольно долго возился, потому что, оказывается, любой чих рядом с `GSequence` вызывает перебалансировку дерева, а посреди запроса это не очень-то приятно делать несколько раз подряд.
* Ещё добавил [jemalloc](http://jemalloc.net), Glib больно уж лихо `malloc` всё время дёргает. Стало на пяток процентов быстрее.
* Поразвлекался с [Google perftools](https://github.com/gperftools/gperftools). [Профилировщик](https://gperftools.github.io/gperftools/cpuprofile.html) из коробки довольно бесполезен, но это до тех пор, пока не задашь ему `CPUPROFILE_FREQUENCY=1000000`.
* По ходу дела вроде бы наконец начал привыкать к стилю работы в Emacs, на который я уже в течение нескольких недель пытаюсь переехать с Vim. Если конкретизировать, Vim - это всё-таки просто клёвый текстовый редактор, а Emacs - это полноразмерная IDE, и, как следствие, частенько пытается писать код за тебя - по крайней мере, в вопросе расстановки отступов Emacs явно считает себя the smartest guy in the room.
* Долгое время работы на Python ~~за еду~~ определённо немношк отупляет - иногда в коде на C на автомате не ставлю в конце строк точку с запятой.
* К 7 утра вроде бы всё доделал, отправил на рейтинг. Штраф 100к секунд. В чате пишут, что у всех, даже у ребят из топа, по 100к штрафа, и это, очевидным образом, баг в тестирующей системе, который организаторы починят, как проснутся.
* Днём проснулся и на скорую руку докодил [прогрев](https://github.com/lockie/highload-cup/commit/eb53f7b3602c756946d6451b5c9c67da8b0ce26b), но у меня есть чёткий пруф того, что в финальное решение он не попал из-за какой-то странной баги в тестирующей системе. Впрочем, Будда с ним, с прогревом, и без него норм.

## Итоги
* Занял 88 место из 296, 325 штрафных секунд. Мог бы выступить лучше, если бы в начале не игрался с питоном (хм, звучит хуже, чем я думал :sweat_smile:) В сторонку: решение с тарантулом на 92 месте :stuck_out_tongue_winking_eye:
* Узнал и потыкал веточкой в целую прорву нового - aiolibs, Memcached, GUnicorn, libevent, внутрянка SQLite, бинарные деревья в GLib, gengetopt, miniz. К Emacs попривык :alien:
* Посмотрел, что вообще народ использует, или хотел бы использовать, для хранения данных: [unQLite](https://unqlite.org), [WhiteDB](https://github.com/priitj/whitedb), Facebook [RocksDB](https://github.com/facebook/rocksdb), [Aerospike](https://github.com/aerospike/aerospike-server), и даже российская разработка [Reindexer](https://github.com/Restream/reindexer). Хотя, как мне кажется, в плане производительности ничего не переплюнет вручную оттюнингованные, рукописные хэши и массивы :blush:
* Девиз этих двух сумасшедших недель - "глаза боятся, а руки делают".
* Ах да, ну и ТЗ надо повнимательнее читать, чтобы не повторялся вот такого рода диалог в чате:
> options.txt не лежит в архиве?

> нет

> он лежит РЯДОМ с архивом

> ну 3.14здец...

Резюмируя, с огромным удовольствием поучаствую в следующем соревновании, которое вроде как запланировали на пару месяцев позже. Возможно, попробую себя в других соревнованиях, например, в Mini AI Cup, который запланирован на 15 сентября.

## Идеи для следующего Highload cup
* Для тестирования использовать [wrk с его волшебными lua-скриптами](https://github.com/wg/wrk/blob/master/SCRIPTING).
* Выкинуть libevent и насладиться магией `epoll` в чистом виде :laughing:
* Вдобавок к уже заюзанным опциям сокетов `SO_REUSEADDR`, `SO_KEEPALIVE` и `TCP_NODELAY` попробовать следующие: `TCP_DEFER_ACCEPT`, `TCP_QUICKACK`, `SO_LINGER`, `SO_RCVLOWAT`, и вообще, почитать https://notes.shichao.io/unp/ch7 . Ещё подробно изучить вот [это](https://idea.popcount.org/2017-02-20-epoll-is-fundamentally-broken-12) и [это](https://blog.cloudflare.com/how-to-achieve-low-latency). Ещё в чате писали, что значение `timeout` в `0` для `epoll` (т.н. busy wait) - волшебная пилюля, определённо стоит попробовать. Ещё идея из чата - один epoll fd на все 4 потока с мьютексом по каждом fd, чтобы лишних syscall'ов не было,  типа work stealing - кто первый тот и обрабатывает.
* Идея по кэшированию: если количество оперативки будет позволять, заранее сгенерировать ответы на простые запросы по id и сразу их отдавать, может даже, HTTP-заголовки туда же.
* Можно ещё, например, попробовать ipv6, но это я уже совсем желаю странного.
* Использовать [кастомное `itoa` через SSE](https://github.com/miloyip/itoa-benchmark) :laughing:
* Таки осилить PGO в GCC, см, например, [тут](https://bitbucket.org/multicoreware/x265/src/fcd9154fa4e28ae9e3c11e16bfae20dbdb89101d/source/CMakeLists.txt?at=default&fileviewer=file-view-default).
* Попробовать [OProfile](http://oprofile.sourceforge.net), может, он окажется ещё круче, чем Google Perftools.
* Попробовать `sendfile`/`splice` из `mmap`ed области памяти. Должно быть вообще огонь по быстродействию, но в таком подходе [есть свои проблемы](https://stackoverflow.com/q/20008707/1336774).
* Совсем упороться и вместо Alpine в качестве базового Docker-контейнера использовать Gentoo :laughing: ; на самом деле, был как минимум один парень из финала с генточкой. Разумеется, использовать [профиль musl](https://wiki.gentoo.org/wiki/Project:Hardened_musl).
* Если всё-таки буду использовать готовые фреймворки, есть вот такой клёвый сайт с бенчмарками: https://www.techempower.com/benchmarks . Первые места в категории C/C++ там занимает библиотека [ULib](https://github.com/stefanocasazza/ULib), можно в неё потыкать веточкой, и, чем чёрт не шутит, даже поконтрибутить.
* Если какая-нибудь либа будет злоупотреблять `malloc`'ом, написать [кастомный](https://www.gnu.org/software/libc/manual/html_node/Replacing-malloc.html) аллокатор памяти, который никогда не осовобождает память :sweat_smile: этакий libdumballoc. Соответствующие флаги GCC: `-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free`
* Потестить [H2O](https://github.com/h2o/h2o), оно и в HTTP/1 умеет. По меньшей мере, стоит оттуда взять [picohttpparser](https://github.com/h2o/picohttpparser), оно malloc-less, просто указатели на переданный `char*` буфер расставляет.
* Идея по архитектур: запилить всю внутрянку на C, а интерфейс к entity - на C++, чтобы опять не было [poor man's OOP](https://github.com/lockie/highload-cup/blob/eb53f7b3602c756946d6451b5c9c67da8b0ce26b/c/src/entity.c#L12) :sweat_smile: Для интроспекции в плюсах заюзать `boost::fusion`, или [Ponder](https://billyquith.github.io/ponder), или [boost_relfect](https://bytemaster.github.io/boost_reflect), или тупо `boost::tuple`. Или совсем тупо пачку макросов :laughing:
* Если будет JSON, стоит использовать SAX-парсер из [RapidJSON](https://github.com/miloyip/rapidjson), самый быстрый, как-никак.
* Возможно, стоит использовать [Ragel](https://ru.wikipedia.org/wiki/Ragel) для разбора JSON и/или HTTP.
