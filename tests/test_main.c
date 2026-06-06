#include <assert.h>
#include <check.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "radio_message_parser.h"

static void
prvSetup(void);

static void
prvTeardown(void);

static rmp_api_handle_t  hAPI;
static rmp_data_handle_t hData;

START_TEST(ExampleForMAN)
{
    // Объявление дескриптора API. Валидный адрес буфер записан при вызове
    // RMP_Ctor()
    rmp_api_handle_t RMP_hAPI = NULL;

    // Инициализация
    do {
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
        if (RMP_hAPI == NULL) {
            // Объект не инициализирован, дальнейшая работа невозможна
        }
    } while (0);

    // Предположим, что aRxDMA содержит полученные данные от последовательного
    // порта ввода/вывода, при этом, данные содержатся с 15 по 33 байт (т.е.
    // содержит 18 байт).
    uint8_t aRxDMA[64] = {0};

    // Тогда запись в кольцевой буфер примет вид:
    size_t uRxBytesNumb = RMP_hAPI->Put(RMP_hAPI, (void *) &aRxDMA[15], 18u);

    if (uRxBytesNumb != 18u) {
        // Буфер переполнен, запись не выполнена
    }

    // Также, допустимой является побайтная запись (пример ниже приведен в
    // качестве иллюстрации, в реальном коде пример в данном виде избыточен,
    // но может использоваться при побайтном получении данных от приемника UART
    // если DMA недоступен)
    for (size_t i = 15u; i < 33; ++i) {
        size_t uWrittenBytesNumb =
            RMP_hAPI->Put(RMP_hAPI, (void *) &aRxDMA[i], 1u);

        if (uWrittenBytesNumb != 1u) {
            // Буфер переполнен, запись не выполнена
        }
    }

    // Т.к. все сообщения в рамках протокола имеют фиксированный размер, то
    // пользователю следует выделить область памяти для получения целого
    // сообщения из кольцевого буфера с использованием определения
    // <rmpONE_MESSAGE_SIZE_IN_BYTES>.
    uint8_t aDstMem[RMP_GetMessageSize(RMP_hAPI)];

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

    if (uRxMessageSize == rmpONE_MESSAGE_SIZE_IN_BYTES) {
        // Сообщение успешно записано в <aDstMem> и его контрольная сумма
        // достоверна.
        // Можно выполнять обработку сообщения в <aDstMem>.
    } else {
        // Сообщение не найдено
    }
}

START_TEST(StructInit)
{
    rmp_init_t xInit;
    memset((void *) &xInit, 0xFF, sizeof(xInit));
    RMP_StructInit(&xInit);

    ck_assert_ptr_null(xInit.pMemAlloc);
    ck_assert_uint_eq(0u, xInit.uMemAllocSizeInBytes);

    /* Необходимо убедиться, что размер структуры сообщения соответствует
     * заданному значению (по умолчанию) */
    ck_assert_uint_eq(rmpONE_MESSAGE_SIZE_IN_BYTES, xInit.uOneMessageSize);
}

START_TEST(CtorIfInvalidMem)
{
    rmp_init_t xInit;
    RMP_StructInit(&xInit);

    ck_assert_ptr_null(RMP_Ctor(&xInit));
}

START_TEST(CtorIfInvalidMemButInvalidSize)
{
    rmp_init_t xInit;
    RMP_StructInit(&xInit);

    uint8_t ucRbMemAlloc[128] = {0};
    xInit.pMemAlloc           = (void *) ucRbMemAlloc;

    ck_assert_ptr_null(RMP_Ctor(&xInit));
}

START_TEST(CtorIfValidSizeButInvalidMem)
{
    rmp_init_t xInit;
    RMP_StructInit(&xInit);
    xInit.uMemAllocSizeInBytes = 10u;

    ck_assert_ptr_null(RMP_Ctor(&xInit));
}

START_TEST(CtorIfValidMemAndSizeButInvalidData)
{
    rmp_init_t xInit;
    RMP_StructInit(&xInit);

    uint8_t ucRbMemAlloc[128]  = {0};
    xInit.pMemAlloc            = (void *) ucRbMemAlloc;
    xInit.uMemAllocSizeInBytes = sizeof(ucRbMemAlloc);

    ck_assert_ptr_null(RMP_Ctor(&xInit));
}

START_TEST(CtorIfValid)
{
    rmp_init_t xInit;
    RMP_StructInit(&xInit);

    uint8_t ucRbMemAlloc[128]  = {0};
    xInit.pMemAlloc            = (void *) ucRbMemAlloc;
    xInit.uMemAllocSizeInBytes = sizeof(ucRbMemAlloc);

    rmp_obj_t xDataMemAlloc;
    xInit.hData           = &xDataMemAlloc;

    rmp_api_handle_t hAPI = RMP_Ctor(&xInit);
    ck_assert_ptr_nonnull(hAPI);
    ck_assert_ptr_nonnull(hAPI->Processing);
    ck_assert_ptr_nonnull(hAPI->Put);
    ck_assert_ptr_nonnull(hAPI->PutISR);

    /* Проверка наличия адреса у функций-обработчиков состояний */
    rmp_data_handle_t hData = (rmp_data_handle_t) hAPI;
    for (size_t i = 0u; i < rmpSTATE_MAX_NUMB; ++i) {
        ck_assert_ptr_nonnull(hData->xStateAPI.aFn[i]);
    }

    ck_assert_uint_eq(true, RMP_Dtor(hAPI));
}

START_TEST(DtorIfNull)
{
    ck_assert_uint_eq(false, RMP_Dtor(NULL));
}

START_TEST(GetCrcByReferencePack)
{
    uint8_t uaPackDef[rmpONE_MESSAGE_SIZE_IN_BYTES] = {
        0xAA, 0x55, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xFA, 0x00, 0xB8, 0x20};

    ck_assert(RMP_IsCrcValid(hAPI, uaPackDef));
}

START_TEST(WriteCrcInMessageTail)
{
    const uint8_t uaPackDef[rmpONE_MESSAGE_SIZE_IN_BYTES] = {
        0xAA, 0x55, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xFA, 0x00, 0xB8, 0x20};

    uint8_t uaPackNoCrc[rmpONE_MESSAGE_SIZE_IN_BYTES] = {
        0xAA, 0x55, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xFA, 0x00, 0x00, 0x00};

    RMP_WriteCrcInMessageTail(hAPI, (void *) uaPackNoCrc);

    ck_assert_mem_eq(uaPackDef, uaPackNoCrc, sizeof(uaPackDef));
}

START_TEST(CheckCrcValidation)
{
    uint8_t uaPackDef[rmpONE_MESSAGE_SIZE_IN_BYTES] = {
        0xAA, 0x55, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x90, 0x59};

    ck_assert_uint_eq(true, RMP_IsCrcValid(hAPI, (void *) uaPackDef));
}

START_TEST(APIPutThenRead)
{
    const char *pSrc    = "Hello World!";
    size_t      uSrlLen = strlen(pSrc);
    ck_assert_uint_eq(uSrlLen, hAPI->Put(hAPI, (void *) pSrc, uSrlLen));

    uint8_t uaDstStr[128] = {0};
    ck_assert_uint_eq(
        uSrlLen,
        RMP_Get(hAPI, (void *) uaDstStr, sizeof(uaDstStr)));

    ck_assert_str_eq(pSrc, (const char *) uaDstStr);
}

START_TEST(APIPutThenReadInCycle)
{
    const char  *pSrc                     = "Hello World!";
    size_t       uSrlLen                  = strlen(pSrc);
    const size_t uBytesNumbInOneIteration = 1u;

    /* Побайтная запись сообщения в кольцевой буфер */
    for (size_t i = 0; i < uSrlLen; ++i) {
        size_t uWrittenBytesNumb =
            hAPI->Put(hAPI, (void *) &pSrc[i], uBytesNumbInOneIteration);

        ck_assert_uint_eq(uBytesNumbInOneIteration, uWrittenBytesNumb);
    }
    /*------------------------------------------------------------------------*/

    uint8_t uaDstStr[48] = {0};

    /* Побайтное чтение сообщения из кольцевого буфера */
    for (size_t i = 0; i < uSrlLen; ++i) {
        size_t uReadBytesNumb =
            RMP_Get(hAPI, (void *) &uaDstStr[i], uBytesNumbInOneIteration);

        ck_assert_uint_eq(uBytesNumbInOneIteration, uReadBytesNumb);
    }
    /*------------------------------------------------------------------------*/

    ck_assert_str_eq(pSrc, (const char *) uaDstStr);
}

START_TEST(SetNewState)
{
    ck_assert_uint_eq(false, RMP_SetState(hAPI, rmpSTATE_MAX_NUMB));
    ck_assert_uint_eq(false, RMP_SetState(hAPI, rmpSTATE_MAX_NUMB + 1));
    ck_assert_uint_eq(false, RMP_SetState(hAPI, -1));

    ck_assert_uint_eq(true, RMP_SetState(hAPI, rmpSTATE_FIND_FIRST_BYTE));
}

START_TEST(StateFindStartFrame)
{
    /* Случай, когда байты начала сообщения расположены последовательно */
    do {
        hAPI->Reset(hAPI);

        const size_t uFirstByteIdx = 1;
        ck_assert_uint_ge(RMP_GetMessageSize(hAPI), uFirstByteIdx);

        const size_t uSecondByteIdx = 2;
        ck_assert_uint_ge(RMP_GetMessageSize(hAPI), uSecondByteIdx);

        uint8_t ucMessage[RMP_GetMessageSize(hAPI)];
        ucMessage[uFirstByteIdx]  = rmpSTART_FRAME_FIRST_BYTE;
        ucMessage[uSecondByteIdx] = rmpSTART_FRAME_SECOND_BYTE;

        hAPI->Put(hAPI, (void *) ucMessage, sizeof(ucMessage));

        ck_assert_uint_eq(rmpIN_PROGRESS, RMP_FindFirstByte(hAPI, NULL, 0));
        ck_assert_uint_eq(rmpIN_PROGRESS, RMP_FindSecondByte(hAPI, NULL, 0));
    } while (0);
    /*------------------------------------------------------------------------*/

    /* Байты начала сообщения не расположены последовательно, соответственно,
     * считаем что начало сообщения не найдено */
    do {
        hAPI->Reset(hAPI);

        const size_t uFirstByteIdx = 5;
        ck_assert_uint_ge(RMP_GetMessageSize(hAPI), uFirstByteIdx);

        const size_t uSecondByteIdx = 7;
        ck_assert_uint_ge(RMP_GetMessageSize(hAPI), uSecondByteIdx);

        uint8_t ucMessage[RMP_GetMessageSize(hAPI)];
        ucMessage[uFirstByteIdx]  = rmpSTART_FRAME_FIRST_BYTE;
        ucMessage[uSecondByteIdx] = rmpSTART_FRAME_SECOND_BYTE;

        hAPI->Put(hAPI, (void *) ucMessage, sizeof(ucMessage));

        ck_assert_uint_eq(rmpIN_PROGRESS, RMP_FindFirstByte(hAPI, NULL, 0));
        ck_assert_uint_eq(rmpIN_PROGRESS, RMP_FindSecondByte(hAPI, NULL, 0));
    } while (0);
    /*------------------------------------------------------------------------*/
}

START_TEST(FindStartFrameAndCopyMessage)
{
    do {
        uint8_t uaSrcMem[RMP_GetMessageSize(hAPI)];
        uaSrcMem[0]  = rmpSTART_FRAME_FIRST_BYTE;
        uaSrcMem[1]  = rmpSTART_FRAME_SECOND_BYTE;
        uaSrcMem[10] = 123;

        RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem);

        /* Запись сообщения в буфер */
        hAPI->Put(hAPI, (void *) uaSrcMem, sizeof(uaSrcMem));

        /* Поиск и копирование сообщения в буфере */
        uint8_t xDstMem[hData->uOneMessageSize];
        size_t  uReceiverMessageSize =
            hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));

        ck_assert_uint_eq(hData->uOneMessageSize, uReceiverMessageSize);

        ck_assert_mem_eq(uaSrcMem, (void *) &xDstMem, sizeof(xDstMem));

        ck_assert_uint_eq(rmpSTATE_FIND_FIRST_BYTE, RMP_GetState(hAPI));
    } while (0);
}

START_TEST(FindStartFrameAndCopySomeMessages)
{
    /* Сформируем первое сообщение, выполним побайтную запись и чтение */
    do {
        uint8_t  uaSrcMem[72]  = {0};
        uint8_t *pStartMessage = (uint8_t *) &uaSrcMem[1];
        pStartMessage[0]       = rmpSTART_FRAME_FIRST_BYTE;
        pStartMessage[1]       = rmpSTART_FRAME_SECOND_BYTE;
        pStartMessage[3]       = 123;

        RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem);

        uint8_t xDstMem[hData->uOneMessageSize];
        /* Побайтная запись в буфер и периодическое чтение сообщений */
        for (size_t i = 0u; i < sizeof(uaSrcMem); ++i) {
            hAPI->Put(hAPI, &uaSrcMem[i], 1u);

            size_t uReadMessageSize =
                hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));

            if (uReadMessageSize != 0u) {
                ck_assert_mem_eq(
                    pStartMessage,
                    (void *) &xDstMem,
                    sizeof(xDstMem));
                /* В буфере обнаружено первое сообщение, необходимо выйти из
                 * цикла
                 */
                break;
            }
        }
    } while (0);

    /* Сформируем второе сообщение, выполним побайтную запись и чтение */
    do {
        uint8_t uaSrcMem[256] = {0};
        uaSrcMem[0]           = rmpSTART_FRAME_FIRST_BYTE;
        uaSrcMem[1]           = rmpSTART_FRAME_SECOND_BYTE;
        uaSrcMem[3]           = 123;

        RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem);

        uint8_t xDstMem[hData->uOneMessageSize];

        /* Побайтная запись в буфер и периодическое чтение сообщений */
        for (size_t i = 0u; i < sizeof(uaSrcMem); ++i) {
            hAPI->Put(hAPI, &uaSrcMem[i], 1u);

            size_t uReadMessageSize =
                hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));

            if (uReadMessageSize != 0u) {
                ck_assert_mem_eq(uaSrcMem, (void *) &xDstMem, sizeof(xDstMem));
                /* В буфере обнаружено первое сообщение, необходимо выйти из
                 * цикла
                 */
                break;
            }
        }
    } while (0);

    /* Сформируем третье сообщение без контрольной суммы */
    do {
        uint8_t uaSrcMem[128]         = {0};
        uaSrcMem[0]                   = rmpSTART_FRAME_FIRST_BYTE;
        uaSrcMem[1]                   = rmpSTART_FRAME_SECOND_BYTE;
        uaSrcMem[3]                   = 1u;
        uaSrcMem[4]                   = 2u;
        uaSrcMem[5]                   = 3u;
        uaSrcMem[6]                   = 4u;
        uaSrcMem[7]                   = 5u;

        rmp_package_generic_t xDstMem = {0};

        /* Побайтная запись в буфер и периодическое чтение сообщений */
        for (size_t i = 0u; i < sizeof(uaSrcMem); ++i) {
            hAPI->Put(hAPI, &uaSrcMem[i], 1u);

            size_t uReadMessageSize =
                hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));

            /* Сообщение не должно быть считано т.к. контрольная сумма не
             * достоверна */
            if (uReadMessageSize != 0u) {
                ck_abort_msg("Buffer dont have valid message!");

                break;
            }
        }
    } while (0);
}

START_TEST(FindStartFrameAndCopyMessageInSmallDstBuff)
{
    /* Сформируем первое сообщение, выполним побайтную запись и чтение */
    do {
        uint8_t uaSrcMem[128] = {0};
        uaSrcMem[0]           = rmpSTART_FRAME_FIRST_BYTE;
        uaSrcMem[1]           = rmpSTART_FRAME_SECOND_BYTE;
        uaSrcMem[3]           = 123;

        RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem);

        /* Размера целевой области памяти недостаточно */
        uint8_t uaDstMem[RMP_GetMessageSize(hAPI) - 1u];
        /* Побайтная запись в буфер и периодическое чтение сообщений */
        for (size_t i = 0u; i < 64; ++i) {
            hAPI->Put(hAPI, &uaSrcMem[i], 1u);

            size_t uReadMessageSize =
                hAPI->Processing(hAPI, (void *) uaDstMem, sizeof(uaDstMem));

            if (uReadMessageSize != 0u) {
                ck_abort_msg("Buffer dont have valid message!");
            }
        }

        /* Теперь выполним чтение в область памяти достаточного размера */
        uint8_t xDstMem[RMP_GetMessageSize(hAPI)];
        ck_assert_uint_eq(
            rmpONE_MESSAGE_SIZE_IN_BYTES,
            hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem)));
    } while (0);
}

START_TEST(Reset)
{
    ck_assert_ptr_nonnull(hAPI);
    ck_assert_ptr_nonnull(hAPI->Reset);

    RMP_SetState(hAPI, rmpSTATE_FIND_SECOND_BYTE);

    hAPI->Reset(hAPI);

    ck_assert_uint_eq(rmpSTATE_FIND_FIRST_BYTE, RMP_GetState(hAPI));
}

START_TEST(JoyCommand)
{
    uint8_t uaPackDef[rmpONE_MESSAGE_SIZE_IN_BYTES] = {
        0xAA, 0x55, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x90, 0x59};

    ck_assert_ptr_nonnull(hAPI);
    ck_assert_ptr_nonnull(hAPI->Reset);

    hAPI->Put(hAPI, (void *) uaPackDef, sizeof(uaPackDef));
    RMP_SetState(hAPI, rmpSTATE_FIND_FIRST_BYTE);

    uint8_t xDstMem[RMP_GetMessageSize(hAPI)];
    size_t  uReceiverMessageSize =
        hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));

    ck_assert_uint_eq(hData->uOneMessageSize, uReceiverMessageSize);
}

START_TEST(CtorWithZeroOneMessageSize)
{
    rmp_init_t xInit;
    RMP_StructInit(&xInit);

    uint8_t ucRbMemAlloc[128]  = {0};
    xInit.pMemAlloc            = (void *) ucRbMemAlloc;
    xInit.uMemAllocSizeInBytes = sizeof(ucRbMemAlloc);

    rmp_obj_t xDataMemAlloc;
    xInit.hData           = &xDataMemAlloc;

    xInit.uOneMessageSize = 0u;
    ck_assert_ptr_null(RMP_Ctor(&xInit));
}

START_TEST(CtorWithSmallOneMessageSize)
{
    rmp_init_t xInit;
    RMP_StructInit(&xInit);

    uint8_t ucRbMemAlloc[128]  = {0};
    xInit.pMemAlloc            = (void *) ucRbMemAlloc;
    xInit.uMemAllocSizeInBytes = sizeof(ucRbMemAlloc);

    rmp_obj_t xDataMemAlloc;
    xInit.hData           = &xDataMemAlloc;

    xInit.uOneMessageSize = 3u;
    ck_assert_ptr_null(RMP_Ctor(&xInit));
}

START_TEST(CtorWithZeroThreshold)
{
    rmp_init_t xInit;
    RMP_StructInit(&xInit);

    uint8_t ucRbMemAlloc[128]  = {0};
    xInit.pMemAlloc            = (void *) ucRbMemAlloc;
    xInit.uMemAllocSizeInBytes = sizeof(ucRbMemAlloc);

    rmp_obj_t xDataMemAlloc;
    xInit.hData               = &xDataMemAlloc;

    xInit.uReadBytesThreshold = 0u;
    ck_assert_ptr_null(RMP_Ctor(&xInit));
}

START_TEST(ProcessingEmptyBuffer)
{
    hAPI->Reset(hAPI);

    uint8_t xDstMem[RMP_GetMessageSize(hAPI)];
    size_t  uRxMessageSize =
        hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));

    ck_assert_uint_eq(0u, uRxMessageSize);
}

START_TEST(ProcessingMessageSplitAcrossCalls)
{
    hAPI->Reset(hAPI);

    uint8_t uaSrcMem[RMP_GetMessageSize(hAPI)];
    memset(uaSrcMem, 0, sizeof(uaSrcMem));
    uaSrcMem[0]  = rmpSTART_FRAME_FIRST_BYTE;
    uaSrcMem[1]  = rmpSTART_FRAME_SECOND_BYTE;
    uaSrcMem[10] = 42;

    RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem);

    /* Записываем первую половину */
    size_t uFirstHalf = RMP_GetMessageSize(hAPI) / 2;
    hAPI->Put(hAPI, uaSrcMem, uFirstHalf);

    uint8_t xDstMem[RMP_GetMessageSize(hAPI)];
    size_t  uRxMessageSize =
        hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(0u, uRxMessageSize);

    /* Записываем вторую половину */
    hAPI->Put(hAPI, &uaSrcMem[uFirstHalf], sizeof(uaSrcMem) - uFirstHalf);
    uRxMessageSize = hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(RMP_GetMessageSize(hAPI), uRxMessageSize);
    ck_assert_mem_eq(uaSrcMem, (void *) &xDstMem, sizeof(xDstMem));
}

START_TEST(ProcessingGarbageBetweenMessages)
{
    hAPI->Reset(hAPI);

    uint8_t uaSrcMem1[RMP_GetMessageSize(hAPI)];
    memset(uaSrcMem1, 0, sizeof(uaSrcMem1));
    uaSrcMem1[0] = rmpSTART_FRAME_FIRST_BYTE;
    uaSrcMem1[1] = rmpSTART_FRAME_SECOND_BYTE;
    uaSrcMem1[2] = 1;
    RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem1);

    uint8_t uaSrcMem2[RMP_GetMessageSize(hAPI)];
    memset(uaSrcMem2, 0, sizeof(uaSrcMem2));
    uaSrcMem2[0] = rmpSTART_FRAME_FIRST_BYTE;
    uaSrcMem2[1] = rmpSTART_FRAME_SECOND_BYTE;
    uaSrcMem2[2] = 2;
    RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem2);

    uint8_t uaStream[64] = {0};
    size_t  uOffset      = 3;
    memcpy(&uaStream[uOffset], uaSrcMem1, sizeof(uaSrcMem1));
    uOffset             += sizeof(uaSrcMem1);
    uaStream[uOffset++]  = 0xDE;
    uaStream[uOffset++]  = 0xAD;
    uaStream[uOffset++]  = 0xBE;
    uaStream[uOffset++]  = 0xEF;
    memcpy(&uaStream[uOffset], uaSrcMem2, sizeof(uaSrcMem2));
    uOffset += sizeof(uaSrcMem2);

    hAPI->Put(hAPI, uaStream, uOffset);

    uint8_t xDstMem[RMP_GetMessageSize(hAPI)];
    size_t  uRxMessageSize =
        hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(RMP_GetMessageSize(hAPI), uRxMessageSize);
    ck_assert_mem_eq(uaSrcMem1, (void *) &xDstMem, sizeof(xDstMem));

    uRxMessageSize = hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(RMP_GetMessageSize(hAPI), uRxMessageSize);
    ck_assert_mem_eq(uaSrcMem2, (void *) &xDstMem, sizeof(xDstMem));
}

START_TEST(ProcessingMultipleMessagesAtOnce)
{
    hAPI->Reset(hAPI);

    uint8_t uaSrcMem1[RMP_GetMessageSize(hAPI)];
    memset(uaSrcMem1, 0, sizeof(uaSrcMem1));
    uaSrcMem1[0] = rmpSTART_FRAME_FIRST_BYTE;
    uaSrcMem1[1] = rmpSTART_FRAME_SECOND_BYTE;
    uaSrcMem1[2] = 11;
    RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem1);

    uint8_t uaSrcMem2[RMP_GetMessageSize(hAPI)];
    memset(uaSrcMem2, 0, sizeof(uaSrcMem2));
    uaSrcMem2[0] = rmpSTART_FRAME_FIRST_BYTE;
    uaSrcMem2[1] = rmpSTART_FRAME_SECOND_BYTE;
    uaSrcMem2[2] = 22;
    RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem2);

    uint8_t uaStream[RMP_GetMessageSize(hAPI) * 2];
    memcpy(uaStream, uaSrcMem1, sizeof(uaSrcMem1));
    memcpy(&uaStream[sizeof(uaSrcMem1)], uaSrcMem2, sizeof(uaSrcMem2));

    hAPI->Put(hAPI, uaStream, sizeof(uaStream));

    uint8_t xDstMem[RMP_GetMessageSize(hAPI)];
    size_t  uRxMessageSize =
        hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(RMP_GetMessageSize(hAPI), uRxMessageSize);
    ck_assert_mem_eq(uaSrcMem1, (void *) &xDstMem, sizeof(xDstMem));

    uRxMessageSize = hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(RMP_GetMessageSize(hAPI), uRxMessageSize);
    ck_assert_mem_eq(uaSrcMem2, (void *) &xDstMem, sizeof(xDstMem));
}

START_TEST(PutOverflow)
{
    hAPI->Reset(hAPI);

    uint8_t uaBigData[256] = {0};
    size_t  uWritten       = hAPI->Put(hAPI, uaBigData, sizeof(uaBigData));

    /* Буфер на 128 байт, максимум записи <= 128 */
    ck_assert_uint_lt(uWritten, sizeof(uaBigData));
}

START_TEST(IsCrcValidInvalidCrc)
{
    uint8_t uaPackDef[rmpONE_MESSAGE_SIZE_IN_BYTES] = {
        0xAA, 0x55, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xFA, 0x00, 0xB8, 0x20};

    /* Портим CRC */
    uaPackDef[rmpONE_MESSAGE_SIZE_IN_BYTES - 1] ^= 0xFF;

    ck_assert_uint_eq(false, RMP_IsCrcValid(hAPI, (void *) uaPackDef));
}

START_TEST(GetMessageSize)
{
    ck_assert_uint_eq(rmpONE_MESSAGE_SIZE_IN_BYTES, RMP_GetMessageSize(hAPI));
}

START_TEST(ResetNonEmptyBuffer)
{
    hAPI->Reset(hAPI);

    uint8_t uaData[10] =
        {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    hAPI->Put(hAPI, uaData, sizeof(uaData));

    size_t uBytesBeforeReset = hAPI->Reset(hAPI);
    ck_assert_uint_eq(sizeof(uaData), uBytesBeforeReset);
}

START_TEST(FalseStartThenValidMessage)
{
    hAPI->Reset(hAPI);

    uint8_t uaSrcMem[RMP_GetMessageSize(hAPI)];
    memset(uaSrcMem, 0, sizeof(uaSrcMem));
    uaSrcMem[0] = rmpSTART_FRAME_FIRST_BYTE;
    uaSrcMem[1] = rmpSTART_FRAME_SECOND_BYTE;
    uaSrcMem[2] = 77;
    RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem);

    uint8_t uaStream[64] = {0};
    /* Ложный старт: 0xAA без 0x55 */
    uaStream[0] = rmpSTART_FRAME_FIRST_BYTE;
    uaStream[1] = 0x00;
    memcpy(&uaStream[2], uaSrcMem, sizeof(uaSrcMem));

    hAPI->Put(hAPI, uaStream, 2 + sizeof(uaSrcMem));

    uint8_t xDstMem[RMP_GetMessageSize(hAPI)];
    size_t  uRxMessageSize =
        hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(RMP_GetMessageSize(hAPI), uRxMessageSize);
    ck_assert_mem_eq(uaSrcMem, (void *) &xDstMem, sizeof(xDstMem));
}

START_TEST(WaitAndCopyMessageInsufficientData)
{
    hAPI->Reset(hAPI);

    uint8_t uaSrcMem[RMP_GetMessageSize(hAPI)];
    memset(uaSrcMem, 0, sizeof(uaSrcMem));
    uaSrcMem[0] = rmpSTART_FRAME_FIRST_BYTE;
    uaSrcMem[1] = rmpSTART_FRAME_SECOND_BYTE;
    RMP_WriteCrcInMessageTail(hAPI, (void *) uaSrcMem);

    /* Записываем только заголовок + 5 байт payload (меньше чем нужно) */
    size_t uPartial = sizeof(rmp_package_head_t) + 5;
    hAPI->Put(hAPI, uaSrcMem, uPartial);

    uint8_t xDstMem[RMP_GetMessageSize(hAPI)];
    size_t  uRxMessageSize =
        hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(0u, uRxMessageSize);
    ck_assert_uint_eq(rmpSTATE_WAIT_AND_COPY_MESSAGE, RMP_GetState(hAPI));

    /* Дописываем остаток */
    hAPI->Put(hAPI, &uaSrcMem[uPartial], sizeof(uaSrcMem) - uPartial);
    uRxMessageSize = hAPI->Processing(hAPI, (void *) &xDstMem, sizeof(xDstMem));
    ck_assert_uint_eq(RMP_GetMessageSize(hAPI), uRxMessageSize);
    ck_assert_mem_eq(uaSrcMem, (void *) &xDstMem, sizeof(xDstMem));
}

START_TEST(FindFirstByteThreshold)
{
    hAPI->Reset(hAPI);

    /* Заполняем буфер 50 байтами мусора (не 0xAA) */
    uint8_t uaGarbage[50];
    for (size_t i = 0; i < sizeof(uaGarbage); ++i) {
        uaGarbage[i] = (uint8_t) (i + 1);
    }
    hAPI->Put(hAPI, uaGarbage, sizeof(uaGarbage));

    /* Порог по умолчанию 40, должны прочитать ровно 40 байт и остановиться */
    ck_assert_uint_eq(rmpBREAK, RMP_FindFirstByte(hAPI, NULL, 0));

    /* В буфере осталось 10 байт */
    ck_assert_uint_eq(10u, hAPI->Reset(hAPI));
}

START_TEST(FindFirstByteNoStartByte)
{
    hAPI->Reset(hAPI);

    uint8_t uaGarbage[10];
    for (size_t i = 0; i < sizeof(uaGarbage); ++i) {
        uaGarbage[i] = (uint8_t) (i + 1);
    }
    hAPI->Put(hAPI, uaGarbage, sizeof(uaGarbage));

    ck_assert_uint_eq(rmpBREAK, RMP_FindFirstByte(hAPI, NULL, 0));

    /* Все байты прочитаны */
    ck_assert_uint_eq(0u, hAPI->Reset(hAPI));
}

int
main(int argc, char *argv[], char *envp[])
{
    /* Создать тестовый объект */
    Suite *s = suite_create("Radio message parser");

    do {
        /* Создать тестовый набор */
        TCase *tc = tcase_create("Radio message parser with no fixture");
        tcase_add_checked_fixture(tc, prvSetup, prvTeardown);

        /* Регистрация тестов в тестовом наборе */
        tcase_add_test(tc, ExampleForMAN);
        tcase_add_test(tc, StructInit);
        tcase_add_test(tc, CtorIfInvalidMem);
        tcase_add_test(tc, CtorIfInvalidMemButInvalidSize);
        tcase_add_test(tc, CtorIfValidSizeButInvalidMem);
        tcase_add_test(tc, CtorIfValidMemAndSizeButInvalidData);
        tcase_add_test(tc, CtorIfValid);
        tcase_add_test(tc, DtorIfNull);
        tcase_add_test(tc, GetCrcByReferencePack);
        tcase_add_test(tc, WriteCrcInMessageTail);
        tcase_add_test(tc, CheckCrcValidation);
        tcase_add_test(tc, CtorWithZeroOneMessageSize);
        tcase_add_test(tc, CtorWithSmallOneMessageSize);
        tcase_add_test(tc, CtorWithZeroThreshold);

        /*--------------------------------------------------------------------*/

        /* Добавить тестовый набор к тестовому объекту */
        suite_add_tcase(s, tc);
    } while (0);
    /*------------------------------------------------------------------------*/

    do {
        /* Создать тестовый набор */
        TCase *tc = tcase_create("Radio message parser API with fixture");
        tcase_add_checked_fixture(tc, prvSetup, prvTeardown);

        tcase_add_test(tc, APIPutThenRead);
        tcase_add_test(tc, APIPutThenReadInCycle);

        /* Добавить тестовый набор к тестовому объекту */
        suite_add_tcase(s, tc);
    } while (0);

    do {
        /* Создать тестовый набор */
        TCase *tc = tcase_create("Radio message state API with fixture");
        tcase_add_checked_fixture(tc, prvSetup, prvTeardown);

        tcase_add_test(tc, SetNewState);
        tcase_add_test(tc, StateFindStartFrame);
        tcase_add_test(tc, FindStartFrameAndCopyMessage);
        tcase_add_test(tc, FindStartFrameAndCopySomeMessages);
        tcase_add_test(tc, FindStartFrameAndCopyMessageInSmallDstBuff);
        tcase_add_test(tc, Reset);
        tcase_add_test(tc, JoyCommand);
        tcase_add_test(tc, ProcessingEmptyBuffer);
        tcase_add_test(tc, ProcessingMessageSplitAcrossCalls);
        tcase_add_test(tc, ProcessingGarbageBetweenMessages);
        tcase_add_test(tc, ProcessingMultipleMessagesAtOnce);
        tcase_add_test(tc, PutOverflow);
        tcase_add_test(tc, IsCrcValidInvalidCrc);
        tcase_add_test(tc, GetMessageSize);
        tcase_add_test(tc, ResetNonEmptyBuffer);
        tcase_add_test(tc, FalseStartThenValidMessage);
        tcase_add_test(tc, WaitAndCopyMessageInsufficientData);
        tcase_add_test(tc, FindFirstByteThreshold);
        tcase_add_test(tc, FindFirstByteNoStartByte);

        /* Добавить тестовый набор к тестовому объекту */
        suite_add_tcase(s, tc);
    } while (0);

    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void
prvSetup(void)
{
    ck_assert_ptr_null(hAPI);

    rmp_init_t xInit;
    RMP_StructInit(&xInit);

    static uint8_t ucRbMemAlloc[128] = {0};
    xInit.pMemAlloc                  = (void *) ucRbMemAlloc;
    xInit.uMemAllocSizeInBytes       = sizeof(ucRbMemAlloc);

    static rmp_obj_t xDataMemAlloc;
    xInit.hData = &xDataMemAlloc;

    hAPI        = RMP_Ctor(&xInit);
    ck_assert_ptr_nonnull(hAPI);
    hData = (rmp_data_handle_t) hAPI;
}

static void
prvTeardown(void)
{
    ck_assert_uint_eq(true, RMP_Dtor(hAPI));

    hAPI  = NULL;
    hData = NULL;
}
