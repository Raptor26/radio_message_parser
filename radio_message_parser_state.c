/**
 * @file radio_message_parser_state.c
 * @author Mickle Isaev (mrraptor26@gmail.com)
 *
 * @brief RMP расшифровывается как <Radio Message Parser>. Библиотека содержит
 * программную реализацию парсера сообщений фиксированной длины и предназначена
 * для выполнения в стиле <Bare Metal>. Реализация парсера соответствует
 * протоколу информационного обмена принятого в автопилота БЛА Альбатрос.
 *
 * Более подробное описание вы можете найти в <radio_message_parser.h>.
 *
 * @version 1.0.0
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
#include "lwrb.h"

#if (rmpTEST_ENABLE != 1)
rmpPRIVATE rmp_return_code
RMP_FindFirstByte(void *vObj, void *pDst, size_t uDstMemSize);

rmpPRIVATE rmp_return_code
RMP_FindSecondByte(void *vObj, void *pDst, size_t uDstMemSize);

rmpPRIVATE rmp_return_code
RMP_WaitAndCopyMessage(void *vObj, void *pDst, size_t uDstMemSize);

rmpPRIVATE size_t
RMP_Get(void *vObj, void *pDst, size_t uDstMemSize);

bool
RMP_SetState(void *vObj, rmp_state_e eNewState);

rmpPRIVATE uint16_t
CORE_GetCrc16_CCITT_Poly0x1021(const void *pSrc, size_t uLen);

rmpPRIVATE uint16_t
RMP_GetPackCrc(void *pvMessage);
#endif

rmp_state_api_handle_t
RMP_InitStateAPI(void *vObj)
{
    rmp_data_handle_t hObj                         = (rmp_data_handle_t) vObj;

    hObj->xStateAPI.aFn[rmpSTATE_FIND_FIRST_BYTE]  = RMP_FindFirstByte;
    hObj->xStateAPI.aFn[rmpSTATE_FIND_SECOND_BYTE] = RMP_FindSecondByte;

    hObj->xStateAPI.aFn[rmpSTATE_WAIT_AND_COPY_MESSAGE] =
        RMP_WaitAndCopyMessage;

    return (&hObj->xStateAPI);
}

rmpPRIVATE rmp_return_code
RMP_FindFirstByte(void *vObj, void *pDst, size_t uDstMemSize)
{
    (void) pDst;
    (void) uDstMemSize;

    rmp_data_handle_t hObj          = (rmp_data_handle_t) vObj;
    size_t            uReadBytesCnt = 0u;
    uint8_t           uOneByte;

    rmp_return_code eReturnCode = rmpBREAK;

    /* Циклический поиск байта начала сообщения */
    while (1)
    {
        /* Поиск первого байта начала пакета данных */
        uOneByte = 0u;

        size_t uReadBytesNumb =
            lwrb_read(&hObj->xLWRB, &uOneByte, sizeof(uOneByte));

        uReadBytesCnt += uReadBytesNumb;

        /* В буфере нет байт, необходимо принудительно выйти из цикла */
        if (uReadBytesNumb == 0u)
        {
            break;
        }
        /*--------------------------------------------------------------------*/

        /* Если обнаружен первый байт */
        if (uOneByte == rmpSTART_FRAME_FIRST_BYTE)
        {
            /* Переход в состояние поиска 2-го байта */
            RMP_SetState(vObj, rmpSTATE_FIND_SECOND_BYTE);

            eReturnCode = rmpIN_PROGRESS;

            break;
        }
        /* if (uOneByte == rmpSTART_FRAME_FIRST_BYTE) */

        /* Считано больше байт чем разрешено за один вызов Processing() */
        if (uReadBytesNumb > hObj->uReadBytesThreshold)
        {
            break;
        }
    }
    /* while (bIsFindFirstByte) */

    return (eReturnCode);
}

rmpPRIVATE rmp_return_code
RMP_FindSecondByte(void *vObj, void *pDst, size_t uDstMemSize)
{
    (void) pDst;
    (void) uDstMemSize;

    uint8_t         uOneByte    = 0u;
    rmp_return_code eReturnCode = rmpIN_PROGRESS;

    size_t uReadBytesNumb       = RMP_Get(vObj, &uOneByte, sizeof(uOneByte));

    /* В буфер еще не записаны данные */
    if (uReadBytesNumb == 0)
    {
        /* Необходимо вернуть код статуса, при получении которого Processing()
         * выйдет из обработки и вернет управление вызывающей функции. Данное
         * условие не означает что за первым байтом не следует второго, это
         * означает что пока в буфере нет данных и, при их получении, необходимо
         * повторить попытку чтения 2-го байта */
        eReturnCode = rmpBREAK;
    }
    else if ((uReadBytesNumb == 1u) && (uOneByte == rmpSTART_FRAME_SECOND_BYTE))
    {
        RMP_SetState(vObj, rmpSTATE_WAIT_AND_COPY_MESSAGE);
    }
    else
    {
        RMP_SetState(vObj, rmpSTATE_FIND_FIRST_BYTE);
    }

    return (eReturnCode);
}

rmpPRIVATE rmp_return_code
RMP_WaitAndCopyMessage(void *vObj, void *pDst, size_t uDstMemSize)
{
    rmp_data_handle_t hObj        = (rmp_data_handle_t) vObj;
    rmp_return_code   eReturnCode = rmpBREAK;

    /* Если размер целевой области памяти больше или равен минимально
     * допустимому размеру и в буфере находится необходимое количество байт */
    if ((uDstMemSize >= rmpONE_MESSAGE_SIZE_IN_BYTES)
        && (lwrb_get_full(&hObj->xLWRB)
            >= (rmpONE_MESSAGE_SIZE_IN_BYTES - sizeof(rmpSTART_FRAME))))
    {
        /* В буфере есть необходимое количество байт, требуется выполнить
         * копирование сообщения в целевую область памяти */

        uint8_t *pDstMemByte = (uint8_t *) pDst;
        pDstMemByte[0]       = rmpSTART_FRAME_FIRST_BYTE;
        pDstMemByte[1]       = rmpSTART_FRAME_SECOND_BYTE;
        RMP_Get(
            vObj,
            &pDstMemByte[sizeof(rmpSTART_FRAME)],
            rmpONE_MESSAGE_SIZE_IN_BYTES - sizeof(rmpSTART_FRAME));

        /* Сообщение найдено и скопировано, необходимо перейти в режим
         * поиска первого байта независимо от того достоверна контрольная сумма
         * или нет */
        RMP_SetState(vObj, rmpSTATE_FIND_FIRST_BYTE);
        /*--------------------------------------------------------------------*/

        /* Проверка контрольной суммы */
        uint8_t uCrc     = RMP_GetPackCrc(pDst);

        uint8_t *pDstIdx = (uint8_t *) pDst;

        if (uCrc
            == pDstIdx[rmpONE_MESSAGE_SIZE_IN_BYTES - rmpCRC_SIZE_IN_BYTES])
        {
            eReturnCode = rmpMESSAGE_COPIED;
        }
        /* if (uCrc
            == pDstIdx[rmpONE_MESSAGE_SIZE_IN_BYTES - rmpCRC_SIZE_IN_BYTES]) */
    }
    /** if ((uDstMemSize >= rmpONE_MESSAGE_SIZE_IN_BYTES)
        && (lwrb_get_full(&hObj->xLWRB)
            >= (rmpONE_MESSAGE_SIZE_IN_BYTES - sizeof(rmpSTART_FRAME)))) */

    return (eReturnCode);
}

rmpPRIVATE uint16_t
RMP_GetPackCrc(void *pvMessage)
{
    rmp_package_generic_t *pPack = (rmp_package_generic_t *) pvMessage;

    return (
        CORE_GetCrc16_CCITT_Poly0x1021(&pPack->xPLoad, sizeof(pPack->xPLoad)));
}

rmpPRIVATE size_t
RMP_Get(void *vObj, void *pDst, size_t uDstMemSize)
{
    rmp_data_handle_t hObj = (rmp_data_handle_t) vObj;

    return (lwrb_read(&hObj->xLWRB, pDst, uDstMemSize));
}

bool
RMP_SetState(void *vObj, rmp_state_e eNewState)
{
    rmp_data_handle_t hObj  = (rmp_data_handle_t) vObj;

    bool bIsStateWasUpdates = false;

    if (eNewState < rmpSTATE_MAX_NUMB)
    {
        hObj->eState       = eNewState;

        bIsStateWasUpdates = true;
    }
    /* if (eNewState < rmpSTATE_MAX_NUMB) */

    return (bIsStateWasUpdates);
}

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

    while (uLen--)
    {
        uCrc ^= (uint16_t) (*pMem++ << 8u);
        size_t i;
        for (i = 0; i < 8; i++)
        {
            uCrc =
                (uint16_t) (uCrc & 0x8000 ? (uCrc << 1) ^ 0x1021 : uCrc << 1);
        }
    }
    /* while (uLen--) */

    return (uCrc);
}
