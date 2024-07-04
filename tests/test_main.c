#include <assert.h>
#include <check.h>
#include <limits.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

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
     * заданному значению */
    ck_assert_uint_eq(
        rmpONE_MESSAGE_SIZE_IN_BYTES,
        sizeof(rmp_package_generic_t));
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

    rmp_package_generic_t *pPackRef = (rmp_package_generic_t *) uaPackDef;

    uint16_t uCrc                   = RMP_GetPackCrc((void *) uaPackDef);

    ck_assert_uint_eq(pPackRef->uCrc, uCrc);
}

START_TEST(APIPutThenRead)
{
    uint8_t *pSrc    = "Hello World!";
    size_t   uSrlLen = strlen(pSrc);
    ck_assert_uint_eq(uSrlLen, hAPI->Put(hAPI, (void *) pSrc, uSrlLen));

    uint8_t uaDstStr[128] = {0};
    ck_assert_uint_eq(
        uSrlLen,
        RMP_Get(hAPI, (void *) uaDstStr, sizeof(uaDstStr)));

    ck_assert_str_eq(pSrc, uaDstStr);
}

START_TEST(APIPutThenReadInCycle)
{
    uint8_t     *pSrc                     = "Hello World!";
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

    ck_assert_str_eq(pSrc, uaDstStr);
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
        ck_assert_uint_ge(rmpONE_MESSAGE_SIZE_IN_BYTES, uFirstByteIdx);

        const size_t uSecondByteIdx = 2;
        ck_assert_uint_ge(rmpONE_MESSAGE_SIZE_IN_BYTES, uSecondByteIdx);

        uint8_t ucMessage[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};
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
        ck_assert_uint_ge(rmpONE_MESSAGE_SIZE_IN_BYTES, uFirstByteIdx);

        const size_t uSecondByteIdx = 7;
        ck_assert_uint_ge(rmpONE_MESSAGE_SIZE_IN_BYTES, uSecondByteIdx);

        uint8_t ucMessage[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};
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
        uint8_t uaSrcMem[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};
        uaSrcMem[0]  = rmpSTART_FRAME_FIRST_BYTE;
        uaSrcMem[1]  = rmpSTART_FRAME_SECOND_BYTE;
        uaSrcMem[10] = 123;

        uaSrcMem[rmpONE_MESSAGE_SIZE_IN_BYTES - rmpCRC_SIZE_IN_BYTES] =
            RMP_GetPackCrc((void *) &uaSrcMem[0]);

        /* Запись сообщения в буфер */
        hAPI->Put(hAPI, (void *) uaSrcMem, sizeof(uaSrcMem));

        /* Поиск и копирование сообщения в буфере */
        uint8_t uaDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};
        size_t  uReceiverMessageSize =
            hAPI->Processing(hAPI, (void *) uaDstMem, sizeof(uaDstMem));

        ck_assert_uint_eq(rmpONE_MESSAGE_SIZE_IN_BYTES, uReceiverMessageSize);

        ck_assert_mem_eq(uaSrcMem, uaDstMem, sizeof(uaDstMem));

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

        pStartMessage[rmpONE_MESSAGE_SIZE_IN_BYTES - 1u] =
            RMP_GetPackCrc((void *) uaSrcMem);

        uint8_t uaDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};
        /* Побайтная запись в буфер и периодическое чтение сообщений */
        for (size_t i = 0u; i < sizeof(uaSrcMem); ++i) {
            hAPI->Put(hAPI, &uaSrcMem[i], 1u);

            size_t uReadMessageSize =
                hAPI->Processing(hAPI, (void *) uaDstMem, sizeof(uaDstMem));

            if (uReadMessageSize != 0u) {
                ck_assert_mem_eq(pStartMessage, uaDstMem, sizeof(uaDstMem));
                /* В буфере обнаружено первое сообщение, необходимо выйти из
                 * цикла
                 */
                break;
            }
        }
    } while (0);

    /* Сформируем второе сообщение, выполним побайтную запись и чтение */
    do {
        uint8_t  uaSrcMem[256] = {0};
        uint8_t *pStartMessage = (uint8_t *) &uaSrcMem[0];
        pStartMessage[0]       = rmpSTART_FRAME_FIRST_BYTE;
        pStartMessage[1]       = rmpSTART_FRAME_SECOND_BYTE;
        pStartMessage[3]       = 123;

        pStartMessage[rmpONE_MESSAGE_SIZE_IN_BYTES - 1u] =
            RMP_GetPackCrc((void *) uaSrcMem);

        uint8_t uaDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};
        /* Побайтная запись в буфер и периодическое чтение сообщений */
        for (size_t i = 0u; i < sizeof(uaSrcMem); ++i) {
            hAPI->Put(hAPI, &uaSrcMem[i], 1u);

            size_t uReadMessageSize =
                hAPI->Processing(hAPI, (void *) uaDstMem, sizeof(uaDstMem));

            if (uReadMessageSize != 0u) {
                ck_assert_mem_eq(pStartMessage, uaDstMem, sizeof(uaDstMem));
                /* В буфере обнаружено первое сообщение, необходимо выйти из
                 * цикла
                 */
                break;
            }
        }
    } while (0);

    /* Сформируем третье сообщение без контрольной суммы */
    do {
        uint8_t uaSrcMem[128] = {0};
        uaSrcMem[0]           = rmpSTART_FRAME_FIRST_BYTE;
        uaSrcMem[1]           = rmpSTART_FRAME_SECOND_BYTE;
        uaSrcMem[3]           = 1u;
        uaSrcMem[4]           = 2u;
        uaSrcMem[5]           = 3u;
        uaSrcMem[6]           = 4u;
        uaSrcMem[7]           = 5u;

        uint8_t uaDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};

        /* Побайтная запись в буфер и периодическое чтение сообщений */
        for (size_t i = 0u; i < sizeof(uaSrcMem); ++i) {
            hAPI->Put(hAPI, &uaSrcMem[i], 1u);

            size_t uReadMessageSize =
                hAPI->Processing(hAPI, (void *) uaDstMem, sizeof(uaDstMem));

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
        uint8_t  uaSrcMem[128] = {0};
        uint8_t *pStartMessage = (uint8_t *) &uaSrcMem[0];
        pStartMessage[0]       = rmpSTART_FRAME_FIRST_BYTE;
        pStartMessage[1]       = rmpSTART_FRAME_SECOND_BYTE;
        pStartMessage[3]       = 123;

        pStartMessage[rmpONE_MESSAGE_SIZE_IN_BYTES - rmpCRC_SIZE_IN_BYTES] =
            RMP_GetPackCrc((void *) uaSrcMem);

        /* Размера целевой области памяти недостаточно */
        uint8_t uaDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES - 1u] = {0};
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
        uint8_t uaBigDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};
        ck_assert_uint_eq(
            rmpONE_MESSAGE_SIZE_IN_BYTES,
            hAPI->Processing(hAPI, (void *) uaBigDstMem, sizeof(uaBigDstMem)));
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

    uint8_t uaDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES] = {0};
    size_t  uReceiverMessageSize =
        hAPI->Processing(hAPI, (void *) uaDstMem, sizeof(uaDstMem));

    ck_assert_uint_eq(rmpONE_MESSAGE_SIZE_IN_BYTES, uReceiverMessageSize);
}

int
main(int argc, char *argv[], char *envp[])
{
    /* Создать тестовый объект */
    Suite *s = suite_create("Radio message parser");

    do {
        /* Создать тестовый набор */
        TCase *tc = tcase_create("Radio message parser with no fixture");

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
