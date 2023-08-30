/**
 * @file radio_message_parser.h
 * @author Mickle Isaev (mrraptor26@gmail.com)
 *
 * @brief RMP расшифровывается как <Radio Message Parser>. Библиотека содержит
 * программную реализацию парсера сообщений фиксированной длины и предназначена
 * для выполнения в стиле <Bare Metal>. Реализация парсера соответствует
 * протоколу информационного обмена принятого в автопилота БЛА Альбатрос.
 *
 * @version 1.0
 * @date 26-07-2023
 *
 * @details
 * NAME
 *      RMP -- Radio Message Parser
 *
 * SYNOPSIS
 *      Библиотека RMP предоставляет пользователю API согласно
 *      интерфейсу <rmp_api_t>, а также следующий набор функций:
 *
 *          - RMP_StructInit()
 *
 *          - RMP_Ctor()
 *
 *          - RMP_Dtor()
 *
 *          - RMP_GetState()
 *
 * DESCRIPTION
 *      При получении байт от последовательного порта ввода/вывода,
 *      пользовательски код выполняет запись полученного потока байт в кольцевой
 *      буфер с помощью Put() и/или PutISR(). В бесконечном цикле, пользователю
 *      необходимо поместить вызов функции-обработчика Processing() которая
 *      отвечает за анализ записанного в кольцевой буфер потока байт,
 *      определения границ сообщений фиксированной длины, проверку контрольной
 *      суммы и запись сообщения в область памяти, предоставляемую
 *      пользовательским кодом.
 *
 *      Пользовательский код должен гарантировать, что количество записываемых
 *      сообщений в единицу времени не превышает количество вызовов
 *      Processing().
 *      Например, частота получаемых сообщений составляет 100 Гц. Обратите
 *      внимание, что частота получаемых сообщений меньше или равна частоте
 *      вызова Put() и/или PutISR() т.к. данные функции допускают как пакетную
 *      запись, так и побайтную. Тогда, частота вызова Processing() должна
 *      удовлетворять условию <ProcessingFreq >= 100 Гц>. В этом случае
 *      гарантируется, что записанные в буфер данные не будут потеряны.
 *
 *      Реализация библиотеки не обеспечивает атомарность. В случае
 *      необходимости одновременного доступа к API, пользовательский код должен
 *      самостоятельно обернуть вызов API в критическую секцию.
 *
 * RETURN_CODES
 *      Обработчик Processing() возвращает:
 *          -   <0> если в буфере не обнаружено сообщения
 *
 *          -   <размер равный rmpONE_MESSAGE_SIZE_IN_BYTES> в
 *              случае обнаружения валидного сообщения в кольцевом буфере и его
 *              копирования в пользовательскую область памяти (см. прототип
 *              функции Processing()).
 *
 * CODE_EXAMPLE
 *      Наиболее актуальный пример использования библиотеки вы можете найти в
 *      tests/test_main.c в функции <START_TEST(ExampleForMAN)>
 *
 *  // Объявление дескриптора API. Валидный адрес буфер записан при вызове
 *  // RMP_Ctor()
 *  rmp_api_handle_t RMP_hAPI = NULL;
 *
 *  // Инициализация
 *  do
 *  {
 *      rmp_init_t xInit;
 *      RMP_StructInit(&xInit);
 *
 *       // Выделение памяти под кольцевой буфер (обратите внимание на
 *       // квалификатор static, он гарантирует, что время жизни выделенной
 *       // области памяти не меньше времени жизни экземпляра <RMP_hAPI>)
 *       static uint8_t ucRbMemAlloc[128] = {0};
 *       xInit.pMemAlloc                  = (void *) ucRbMemAlloc;
 *       xInit.uMemAllocSizeInBytes       = sizeof(ucRbMemAlloc);
 *
 *      // Выделение пользовательским кодом памяти под управляющую структуру
 *      // (обратите внимание на квалификатор static, он гарантирует, что время
 *      // жизни выделенной области памяти не меньше времени жизни экземпляра
 *      // <RMP_hAPI>)
 *      static rmp_obj_t xDataMemAlloc;
 *      xInit.hData = &xDataMemAlloc;
 *
 *      RMP_hAPI    = RMP_Ctor(&xInit);
 *      if (RMP_hAPI == NULL)
 *      {
 *          // Объект не инициализирован, дальнейшая работа невозможна
 *      }
 *  } while (0);
 *
 *  // Предположим, что aRxDMA содержит полученные данные от последовательного
 *  // порта ввода/вывода, при этом, данные содержатся с 15 по 33 байт (т.е.
 *  // содержит 18 байт).
 *  uint8_t aRxDMA[64] = {0};
 *
 *  // Тогда запись в кольцевой буфер примет вид:
 *  size_t uRxBytesNumb = RMP_hAPI->Put(RMP_hAPI, (void *) &aRxDMA[15], 18u);
 *
 *  if (uRxBytesNumb != 18u)
 *  {
 *      // Буфер переполнен, запись не выполнена
 *  }
 *
 *  // Также, допустимой является побайтная запись (пример ниже приведен в
 *  // качестве иллюстрации, в реальном коде пример в данном виде избыточен,
 *  // но может использоваться при побайтном получении данных от приемника UART
 *  // если DMA недоступен)
 *  for (size_t i = 15u; i < 33; ++i)
 *  {
 *      size_t uWrittenBytesNumb =
 *          RMP_hAPI->Put(RMP_hAPI, (void *) &aRxDMA[i], 1u);
 *
 *      if (uWrittenBytesNumb != 1u)
 *      {
 *          // Буфер переполнен, запись не выполнена
 *      }
 *  }
 *
 *  // Т.к. все сообщения в рамках протокола имеют фиксированный размер, то
 *  // пользователю следует выделить область памяти для записи с использованием
 *  // определения <rmpONE_MESSAGE_SIZE_IN_BYTES>.
 *  uint8_t aDstMem[rmpONE_MESSAGE_SIZE_IN_BYTES];
 *
 *  // Обратите внимание, что вызов Processing() должен осуществляться
 *  // периодически и с частотой, не меньше частоты поступаемые сообщений
 *  // (именно сообщений, а не потока байт).
 *  size_t uRxMessageSize =
 *      RMP_hAPI->Processing(RMP_hAPI, (void *) aDstMem, sizeof(aDstMem));
 *
 *  // Обратите внимание, что Processing() вначале записывает сообщение в
 *  // aDstMem, затем проверяет контрольную сумму. Таким образом, в aDstMem
 *  // может оказаться невалидное сообщение. С целью избежать обработки
 *  // невалидного сообщения, используете условие <if (uRxMessageSize ==
 *  // rmpONE_MESSAGE_SIZE_IN_BYTES)> (см. пример ниже):
 *
 *  if (uRxMessageSize == rmpONE_MESSAGE_SIZE_IN_BYTES)
 *  {
 *      // Сообщение успешно записано в <aDstMem> и его контрольная сумма
 *      // достоверна.
 *      // Можно выполнять обработку сообщения в <aDstMem>.
 *  }
 *  else
 *  {
 *      // Сообщение не найдено
 *  }
 *
 * @copyright Copyright (c) 2023 StilSoft
 *
 * MIT License:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the 'Software'), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef RADIO_MESSAGE_PARSER_H
#define RADIO_MESSAGE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lwrb.h"

#if (rmpTEST_ENABLE == 1)
    #define rmpPRIVATE
#else
    #define rmpPRIVATE static
#endif
/*----------------------------------------------------------------------------*/

#ifndef rmpSTART_FRAME
    #define rmpSTART_FRAME ((uint16_t) (0xFFFF))
#endif
/*----------------------------------------------------------------------------*/

#ifndef rmpSTART_FRAME_FIRST_BYTE
    #define rmpSTART_FRAME_FIRST_BYTE ((uint8_t) 0xFF)
#endif

#ifndef rmpSTART_FRAME_SECOND_BYTE
    #define rmpSTART_FRAME_SECOND_BYTE ((uint8_t) 0xFF)
#endif
/*----------------------------------------------------------------------------*/

#ifndef rmpONE_MESSAGE_SIZE_IN_BYTES
    #define rmpONE_MESSAGE_SIZE_IN_BYTES (18)
#endif
/*----------------------------------------------------------------------------*/

#define rmpCRC_SIZE_IN_BYTES (1)

typedef enum
{
    rmpSTATE_FIND_FIRST_BYTE = 0,
    rmpSTATE_FIND_SECOND_BYTE,
    rmpSTATE_WAIT_AND_COPY_MESSAGE,

    rmpSTATE_MAX_NUMB,
} rmp_state_e;
/*----------------------------------------------------------------------------*/

typedef enum
{
    /**
     * @brief При возврате данного кода функция Processing() циклически
     * продолжает свое выполнение.
     */
    rmpIN_PROGRESS = 0,

    /**
     * @brief Конечный автомат выполнил копирование сообщения из кольцевого
     * буфера в целевую область памяти.
     */
    rmpMESSAGE_COPIED,

    /**
     * @brief Данный код возврата обеспечивает выход из цикла в Processing().
     */
    rmpBREAK,
} rmp_return_code;

/**
 * @brief Набор API, предоставляемый библиотекой пользовательскому коду.
 */
typedef struct
{
    /**
     * @brief Запись байт в кольцевой буфер для последующей обработки при вызове
     * Processing()
     *
     * @param[out] vObj: Указатель на объект обработчика сообщений.
     *
     * @param[in] pSrc: Указатель на область памяти из которой необходимо
     * записать данные в кольцевой буфер.
     *
     * @param[in] uBytesNumb: Количество байт для записи в кольцевой буфер из
     * области памяти <pSrc>.
     *
     * @return Количество записанных байт в кольцевой буфер. В случае успешной
     * записи, возвращаемое значение равняется <uBytesNumb.
     */
    size_t (*Put)(void *vObj, void *pSrc, size_t uBytesNumb);

    size_t (*PutISR)(void *vObj, void *pSrc, size_t uBytesNumb);

    /**
     * @brief Обработчик байт в кольцевом буфере. Если в процессе обработки
     * обнаружено сообщение, то оно будет записано по адресу, указанному в
     * <pDst>.
     *
     * @param[out] vObj: Указатель на объект обработчика сообщений.
     *
     * @param[out] pDst: Указатель на область памяти, в которую будет записано
     * сообщение, если оно обнаружено в кольцевом буфере.
     *
     * @param[in] uDstMemSize: Размер области памяти на которую указывает
     * <pDst>. Используется для контроля выхода за пределы массива.
     *
     * @return Возвращает размер записанного в <pDst> сообщения. Если функция
     * вернула <0>, то сообщение в кольцевом буфере не обнаружено (или размер
     * области памяти <pDst> меньше <rmpONE_MESSAGE_SIZE_IN_BYTES>).
     */
    size_t (*Processing)(void *vObj, void *pDst, size_t uDstMemSize);

    /**
     * @brief Выполняет сброс кольцевого буфера с удалением всех записанных в
     * него данных.
     *
     * @param[out] vObj: Указатель на объект обработчика сообщений.
     *
     * @return Количество записанных байт в кольцевой буфер перед его сбросом.
     */
    size_t (*Reset)(void *vObj);
} rmp_api_t;

typedef rmp_api_t *rmp_api_handle_t;
/*----------------------------------------------------------------------------*/

typedef struct
{
    rmp_return_code (
        *aFn[rmpSTATE_MAX_NUMB])(void *vObj, void *pDst, size_t uDstMemSize);
} rmp_state_api_t;

typedef rmp_state_api_t *rmp_state_api_handle_t;
/*----------------------------------------------------------------------------*/

typedef struct
{
    /**
     * @brief Область памяти хранения API функций.
     *
     * @warning Данное поле должно быть расположено в начале структуры данных, в
     * этом случае адрес переменной типа <rmp_obj_t> совпадает с адресом
     * переменной типа <rmp_api_t>.
     */
    rmp_api_t xAPI;

    rmp_state_api_t xStateAPI;

    /**
     * @brief Текущее состояние конечного автомата.
     */
    rmp_state_e eState;

    /**
     * @brief Управляющая структура кольцевого буфера.
     */
    lwrb_t xLWRB;
    /*------------------------------------------------------------------------*/

    /**
     * @brief Количество считанных из буфера байт перед принудительным
     * прерыванием цикла поиска начала сообщения.
     */
    size_t uReadBytesThreshold;
} rmp_obj_t;

typedef rmp_obj_t *rmp_data_handle_t;
/*----------------------------------------------------------------------------*/

/**
 * @brief
 */
typedef struct
{
    /**
     * @brief Указатель на область памяти, выделенную под кольцевой буфер.
     *
     * @note Данная область памяти выделяется пользователем.
     *
     * @warning Пользователь гарантирует, что выделенная им область памяти
     * используется только одним экземпляром <radio_message_parser> в
     * монопольном режиме и время жизни выделенной области памяти больше или
     * равно времени жизни экземпляра <radio_message_parser>.
     */
    void *pMemAlloc;

    /**
     * @brief Размер в байтах выделенной пользователем области памяти под
     * кольцевой буфер.
     */
    size_t uMemAllocSizeInBytes;
    /*------------------------------------------------------------------------*/

    /**
     * @brief Указатель на область памяти управляющей структуры.
     *
     * @note Данная область памяти выделяется пользователем.
     *
     * @warning Пользователь гарантирует, что выделенная им область памяти
     * используется только одним экземпляром <radio_message_parser> в
     * монопольном режиме и время жизни выделенной области памяти больше или
     * равно времени жизни экземпляра <radio_message_parser>.
     */
    rmp_data_handle_t hData;

    /**
     * @brief Количество считанных из буфера байт перед принудительным
     * прерыванием цикла поиска начала сообщения.
     */
    size_t uReadBytesThreshold;
} rmp_init_t;

extern void
RMP_StructInit(rmp_init_t *pxInit);

extern rmp_api_handle_t
RMP_Ctor(rmp_init_t *pxInit);

extern bool
RMP_Dtor(rmp_api_handle_t hObj);

extern bool
RMP_SetState(void *vObj, rmp_state_e eNewState);

extern rmp_state_e
RMP_GetState(void *vObj);

#if (rmpTEST_ENABLE == 1)
extern rmpPRIVATE size_t
RMP_Get(void *vObj, void *pDst, size_t uDstMemSize);

extern rmpPRIVATE rmp_return_code
RMP_FindFirstByte(void *vObj, void *pDst, size_t uDstMemSize);

extern rmpPRIVATE rmp_return_code
RMP_FindSecondByte(void *vObj, void *pDst, size_t uDstMemSize);

extern rmpPRIVATE rmp_return_code
RMP_WaitAndCopyMessage(void *vObj, void *pDst, size_t uDstMemSize);

extern rmpPRIVATE uint8_t
CORE_GetCrc8_XOR(const void *pSrc, size_t len);
#endif

#endif /* RADIO_MESSAGE_PARSER_H */
