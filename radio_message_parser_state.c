/**
 * @file radio_message_parser_state.c
 * @author Mickle Isaev (mrraptor26@gmail.com)
 *
 * @brief RMP расшифровывается как <Radio Message Parser>. Библиотека содержит
 * программную реализацию парсера сообщений фиксированной длины и предназначена
 * для выполнения в стиле <Bare Metal>.
 *
 * Более подробное описание вы можете найти в <radio_message_parser.h>.
 *
 * @version 1.0.2
 *
 * @copyright Copyright (c) 2024 StilSoft
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

#include "radio_message_parser.h"
#include "lwrb/lwrb.h"

#if (rmpTEST_ENABLE != 1)
rmpPRIVATE rmp_return_code
RMP_FindStartFrame(void *vObj, void *pDst, size_t uDstMemSize);

rmpPRIVATE rmp_return_code
RMP_WaitAndCopyMessage(void *vObj, void *pDst, size_t uDstMemSize);

rmpPRIVATE size_t
RMP_Get(void *vObj, void *pDst, size_t uDstMemSize);

bool
RMP_SetState(void *vObj, rmp_state_e eNewState);

rmpPRIVATE uint16_t
CORE_GetCrc16_CCITT_Poly0x1021(const void *pSrc, size_t uLen);

rmpPRIVATE uint16_t
RMP_GetPackCrc(void *vObj, void *pvMessage);
#endif

rmp_state_api_handle_t
RMP_InitStateAPI(void *vObj)
{
    rmp_data_handle_t hObj = (rmp_data_handle_t) vObj;

    hObj->xStateAPI.aFn[rmpSTATE_FIND_START_FRAME] = RMP_FindStartFrame;

    hObj->xStateAPI.aFn[rmpSTATE_WAIT_AND_COPY_MESSAGE] =
        RMP_WaitAndCopyMessage;

    return (&hObj->xStateAPI);
}

rmpPRIVATE rmp_return_code
RMP_FindStartFrame(void *vObj, void *pDst, size_t uDstMemSize)
{
    (void) pDst;
    (void) uDstMemSize;

    rmp_data_handle_t hObj          = (rmp_data_handle_t) vObj;
    rmp_return_code   eReturnCode   = rmpBREAK;

    size_t            uBytesSkipped = 0u;

    /* Поиск стартового кадра через peek — без удаления байт из буфера
     * до подтверждения валидности заголовка */
    while (lwrb_get_full(&hObj->xLWRB) >= sizeof(rmp_package_head_t)) {
        rmp_package_head_t xHead;
        lwrb_peek(&hObj->xLWRB, 0, &xHead, sizeof(xHead));

        /* Если обнаружен валидный стартовый кадр */
        if ((xHead.uFirstByte == rmpSTART_FRAME_FIRST_BYTE)
            && (xHead.uSecondByte == rmpSTART_FRAME_SECOND_BYTE)) {
            /* Пропускаем заголовок из буфера, переходим к чтению сообщения */
            lwrb_skip(&hObj->xLWRB, sizeof(xHead));
            RMP_SetState(vObj, rmpSTATE_WAIT_AND_COPY_MESSAGE);
            eReturnCode = rmpIN_PROGRESS;
            break;
        }

        /* Невалидный заголовок — пропускаем один байт и продолжаем поиск */
        lwrb_skip(&hObj->xLWRB, 1);
        ++uBytesSkipped;

        /* Считано больше байт чем разрешено за один вызов Processing() */
        if (uBytesSkipped >= hObj->uReadBytesThreshold) {
            break;
        }
    }
    /* while (lwrb_get_full >= sizeof(head)) */

    /* Если в буфере остался ровно 1 байт и это не первый байт стартового
     * кадра — пропускаем его, иначе оставляем на случай прихода 0x55 */
    if (lwrb_get_full(&hObj->xLWRB) == 1u) {
        uint8_t uOneByte;
        lwrb_peek(&hObj->xLWRB, 0, &uOneByte, sizeof(uOneByte));
        if (uOneByte != rmpSTART_FRAME_FIRST_BYTE) {
            lwrb_skip(&hObj->xLWRB, 1);
        }
    }

    return (eReturnCode);
}

rmpPRIVATE rmp_return_code
RMP_WaitAndCopyMessage(void *vObj, void *pDst, size_t uDstMemSize)
{
    rmp_data_handle_t      hObj        = (rmp_data_handle_t) vObj;
    rmp_return_code        eReturnCode = rmpBREAK;
    rmp_package_generic_t *pDstPack    = (rmp_package_generic_t *) pDst;

    /* Если размер целевой области памяти больше или равен минимально
     * допустимому размеру и в буфере находится необходимое количество байт
     */
    if ((uDstMemSize >= hObj->uOneMessageSize)
        && (lwrb_get_full(&hObj->xLWRB)
            >= hObj->uOneMessageSize - sizeof(pDstPack->xHead))) {
        /* В буфере есть необходимое количество байт, требуется выполнить
         * копирование сообщения в целевую область памяти через peek —
         * чтобы при невалидной CRC можно было пропустить только заголовок */

        pDstPack->xHead.uFirstByte  = rmpSTART_FRAME_FIRST_BYTE;
        pDstPack->xHead.uSecondByte = rmpSTART_FRAME_SECOND_BYTE;
        lwrb_peek(
            &hObj->xLWRB,
            0,
            &pDstPack->xPLoad,
            hObj->uOneMessageSize - sizeof(pDstPack->xHead));

        if (RMP_IsCrcValid(vObj, (void *) pDst)) {
            /* CRC валидна — пропускаем payload из буфера, сообщение скопировано */
            lwrb_skip(
                &hObj->xLWRB,
                hObj->uOneMessageSize - sizeof(pDstPack->xHead));
            eReturnCode = rmpMESSAGE_COPIED;
        } else {
            /* CRC невалидна — заголовок уже удалён из буфера в FindStartFrame,
             * payload остаётся в буфере, продолжаем поиск кадра */
            eReturnCode = rmpIN_PROGRESS;
        }

        /* Независимо от результата CRC переходим в поиск стартового кадра */
        RMP_SetState(vObj, rmpSTATE_FIND_START_FRAME);
    }
    /** if ((uDstMemSize >= rmpONE_MESSAGE_SIZE_IN_BYTES)
        && (lwrb_get_full(&hObj->xLWRB)
            >= (rmpONE_MESSAGE_SIZE_IN_BYTES - sizeof(rmpSTART_FRAME)))) */

    return (eReturnCode);
}

rmpPRIVATE uint16_t
RMP_GetPackCrc(void *vObj, void *pvMessage)
{
    rmp_data_handle_t hObj   = (rmp_data_handle_t) vObj;
    uint8_t          *pPack  = (uint8_t *) pvMessage;
    uint16_t         *pPload = (uint16_t *) &pPack[sizeof(uint16_t)];
    const size_t      uPloadSize =
        hObj->uOneMessageSize - sizeof(uint16_t) - sizeof(uint16_t);

    return (CORE_GetCrc16_CCITT_Poly0x1021(pPload, uPloadSize));
}

/**
 * @brief Функция записывает контрольную сумму сообщения в его конец.
 *
 * @note Поддерживаемые библиотекой сообщения имеют фиксированную длину, равную
 * rmpONE_MESSAGE_SIZE_IN_BYTES
 *
 * @param[in,out] pvMessage: Указатель на начало сообщения.
 */
void
RMP_WriteCrcInMessageTail(void *vObj, void *pvMessage)
{
    rmp_data_handle_t hObj  = (rmp_data_handle_t) vObj;
    uint8_t          *pPack = (uint8_t *) pvMessage;
    uint16_t         *uCrc =
        (uint16_t *) &pPack[hObj->uOneMessageSize - sizeof(uint16_t)];
    *uCrc = RMP_GetPackCrc(vObj, pvMessage);
}

/**
 * @brief Проверяет достоверность контрольной суммы пакета данных.
 *
 * @param[in] vObj: Указатель на объект обработчика сообщений.
 * @param[in] pvMessage: Указатель на начало сообщения.
 *
 * @return true в случае если контрольная сумма пакета достоверна.
 * @return false в противном случае.
 */
bool
RMP_IsCrcValid(void *vObj, void *pvMessage)
{
    rmp_data_handle_t hObj  = (rmp_data_handle_t) vObj;
    uint8_t          *pPack = (uint8_t *) pvMessage;
    uint16_t         *uCrc =
        (uint16_t *) &pPack[hObj->uOneMessageSize - sizeof(uint16_t)];

    bool bIsCrcValid = false;

    if (*uCrc == RMP_GetPackCrc(vObj, pvMessage)) {
        bIsCrcValid = true;
    }

    return (bIsCrcValid);
}

/**
 * @brief Возвращает размер одного сообщения в байтах.
 *
 * @param[in] vObj: Указатель на объект обработчика сообщений.
 *
 * @return Размер сообщения.
 */
size_t
RMP_GetMessageSize(void *vObj)
{
    rmp_data_handle_t hObj = (rmp_data_handle_t) vObj;
    return hObj->uOneMessageSize;
}

rmpPRIVATE size_t
RMP_Get(void *vObj, void *pDst, size_t uDstMemSize)
{
    rmp_data_handle_t hObj = (rmp_data_handle_t) vObj;

    return (lwrb_read(&hObj->xLWRB, pDst, uDstMemSize));
}

/**
 * @brief Устанавливает состояние конечного автомата парсера.
 *
 * @param[out] vObj: Указатель на объект обработчика сообщений.
 * @param[in] eNewState: Новое состояние конечного автомата.
 *
 * @return true если состояние изменено; false если передано недопустимое
 * состояние.
 */
bool
RMP_SetState(void *vObj, rmp_state_e eNewState)
{
    rmp_data_handle_t hObj  = (rmp_data_handle_t) vObj;

    bool bIsStateWasUpdated = false;

    if (eNewState < rmpSTATE_MAX_NUMB) {
        hObj->eState       = eNewState;

        bIsStateWasUpdated = true;
    }
    /* if (eNewState < rmpSTATE_MAX_NUMB) */

    return (bIsStateWasUpdated);
}

/**
 * @brief Возвращает текущее состояние конечного автомата парсера.
 *
 * @param[in] vObj: Указатель на объект обработчика сообщений.
 *
 * @return Текущее состояние конечного автомата.
 */
rmp_state_e
RMP_GetState(void *vObj)
{
    rmp_data_handle_t hObj = (rmp_data_handle_t) vObj;

    return (hObj->eState);
}

/**
 * @brief Функция выполняет расчет 16-ти битной контрольной суммы.
 *
 * @see
 * https://ru.wikibooks.org/wiki/%D0%A0%D0%B5%D0%B0%D0%BB%D0%B8%D0%B7%D0%B0%D1%86%D0%B8%D0%B8_%D0%B0%D0%BB%D0%B3%D0%BE%D1%80%D0%B8%D1%82%D0%BC%D0%BE%D0%B2/%D0%A6%D0%B8%D0%BA%D0%BB%D0%B8%D1%87%D0%B5%D1%81%D0%BA%D0%B8%D0%B9_%D0%B8%D0%B7%D0%B1%D1%8B%D1%82%D0%BE%D1%87%D0%BD%D1%8B%D0%B9_%D0%BA%D0%BE%D0%B4#CRC-16
 *
 * @param[in] pSrc: Адрес области памяти, с которой начинается вычисление CRC.
 * @param[in] uLen: Количество байт, участвующих в расчете CRC.
 *
 * @return Значение 16-ти битной контрольной суммы.
 */
rmpPRIVATE uint16_t
CORE_GetCrc16_CCITT_Poly0x1021(const void *pSrc, size_t uLen)
{
    uint8_t *pMem = (uint8_t *) pSrc;

    uint16_t uCrc = 0xFFFF;

    while (uLen--) {
        uCrc ^= (uint16_t) (*pMem++ << 8u);
        size_t i;
        for (i = 0; i < 8; i++) {
            uCrc =
                (uint16_t) (uCrc & 0x8000 ? (uCrc << 1) ^ 0x1021 : uCrc << 1);
        }
    }
    /* while (uLen--) */

    return (uCrc);
}
