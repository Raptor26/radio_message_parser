# Bytes flow radio message parser

Библиотека реализует парсер сообщений фиксированной длинны последовательного ввода/вывода.

## Особенности

- библиотека очень легковесна и рассчитана на применение во встраиваемых системах;
- стандарт языка программирования `С11`;
- не использует динамическое выделение памяти;
- рассчитана на выполнение в стиле `bare metal`;
- не имеет внешних зависимостей;
- построена с использованием конечных автоматов;
- реентерабельна - вы можете создавать необходимое количество экземпляров парсера и использовать с различными потоками данных;
- написана через `TDD`.

## NAME

RMP - Radio Message Parser

## SYNOPSIS

Библиотека RMP предоставляет пользователю API согласно интерфейсу `rmp_api_t`, а также следующий набор функций:
- RMP_StructInit()
- RMP_Ctor()
- RMP_Dtor()
- RMP_GetState()

## DESCRIPTION

При получении байт от последовательного порта ввода/вывода, пользовательски код выполняет запись полученного потока байт в кольцевой буфер с помощью Put() и/или PutISR(). В бесконечном цикле, пользователю необходимо поместить вызов функции-обработчика Processing() которая отвечает за анализ записанного в кольцевой буфер потока байт, определения границ сообщений фиксированной длины, проверку контрольной суммы и запись сообщения в область памяти, предоставляемую пользовательским кодом.

Пользовательский код должен гарантировать, что количество записываемых сообщений в единицу времени не превышает количество вызовов Processing(). Например, частота получаемых сообщений составляет 100 Гц. Тогда, частота вызова `Processing()` должна удовлетворять условию `ProcessingFreq >= 100 Гц`. В этом случае гарантируется, что записанные в буфер данные не будут потеряны.

Реализация библиотеки **не обеспечивает** атомарность. В случае необходимости одновременного доступа к API, пользовательский код должен самостоятельно обернуть вызов API в критическую секцию.

## RETURN_CODES

Обработчик Processing() возвращает:
-   `0` если в буфере не обнаружено сообщения
-   размер, указанный в `rmpONE_MESSAGE_SIZE_IN_BYTES` если обнаружено валидное сообщение в кольцевом буфере и выполнено его копирования в пользовательскую область памяти (см. прототип функции Processing()).

## CODE_EXAMPLE

Наиболее актуальный пример использования библиотеки вы можете найти в `tests/test_main.c` в функции `START_TEST(ExampleForMAN)`

```c

#include "radio_message_parser.h"

int main(void)
{
    // Объявление дескриптора API. Валидный адрес буфер записан при вызове
    // RMP_Ctor()
    rmp_api_handle_t RMP_hAPI = NULL;

    // Инициализация
    do
    {
        rmp_init_t xInit;
        RMP_StructInit(&xInit);

        // Выделение памяти под кольцевой буфер (обратите внимание на
        // квалификатор static, он гарантирует, что время жизни выделенной
        // области памяти не меньше времени жизни экземпляра <RMP_hAPI>)
        static uint8_t ucRbMemAlloc[128] = {0};
        xInit.pMemAlloc                  = (void *) ucRbMemAlloc;
        xInit.uMemAllocSizeInBytes       = sizeof(ucRbMemAlloc);

        // Выделение пользовательским кодом памяти под управляющую структуру
        // (обратите внимание на квалификатор static, он гарантирует, что время
        // жизни выделенной области памяти не меньше времени жизни экземпляра
        // <RMP_hAPI>)
        static rmp_obj_t xDataMemAlloc;
        xInit.hData = &xDataMemAlloc;

        RMP_hAPI    = RMP_Ctor(&xInit);
        if (RMP_hAPI == NULL)
        {
            // Объект не инициализирован, дальнейшая работа невозможна
        }
    } while (0);

    // Предположим, что aRxDMA содержит полученные данные от последовательного
    // порта ввода/вывода, при этом, данные содержатся с 15 по 33 байт (т.е.
    // содержит 18 байт).
    uint8_t aRxDMA[64] = {0};

    // Тогда запись в кольцевой буфер примет вид:
    size_t uRxBytesNumb = RMP_hAPI->Put(RMP_hAPI, (void *) &aRxDMA[15], 18u);

    if (uRxBytesNumb != 18u)
    {
        // Буфер переполнен, запись не выполнена
    }

    // Также, допустимой является побайтная запись (пример ниже приведен в
    // качестве иллюстрации, в реальном коде пример в данном виде избыточен,
    // но может использоваться при побайтном получении данных от приемника UART
    // если DMA недоступен)
    for (size_t i = 15u; i < 33; ++i)
    {
        size_t uWrittenBytesNumb =
            RMP_hAPI->Put(RMP_hAPI, (void *) &aRxDMA[i], 1u);

        if (uWrittenBytesNumb != 1u)
        {
            // Буфер переполнен, запись не выполнена
        }
    }

    // Т.к. все сообщения в рамках протокола имеют фиксированный размер, то
    // пользователю следует выделить область памяти для получения целого
    // сообщения из кольцевого буфера с использованием определения
    // <rmpONE_MESSAGE_SIZE_IN_BYTES>.
    uint8_t aDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES];

    // Обратите внимание, что вызов Processing() должен осуществляться
    // периодически и с частотой, не меньше частоты поступаемые сообщений
    // (именно сообщений, а не потока байт).
    size_t uRxMessageSize =
        RMP_hAPI->Processing(RMP_hAPI, (void *) aDstMem, sizeof(aDstMem));

    // Обратите внимание, что Processing() вначале записывает сообщение в
    // aDstMem, затем проверяет контрольную сумму. Таким образом, в aDstMem
    // может оказаться невалидное сообщение. С целью избежать обработки
    // невалидного сообщения, используете условие <if (uRxMessageSize ==
    // rmpONE_MESSAGE_SIZE_IN_BYTES)> (см. пример ниже):

    if (uRxMessageSize == rmpONE_MESSAGE_SIZE_IN_BYTES)
    {
        // Сообщение успешно записано в <aDstMem> и его контрольная сумма
        // достоверна.
        // Можно выполнять обработку сообщения в <aDstMem>.
    }
    else
    {
        // Сообщение не найдено
    }

    return 0;
}
```
