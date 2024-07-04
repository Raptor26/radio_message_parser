/**
 * @file radio_message_parser_api.c
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

static size_t
prvPut(void *vObj, void *pSrc, size_t uBytesNumb);

static size_t
prvPutISR(void *vObj, void *pSrc, size_t uBytesNumb);

static size_t
prvProcessing(void *vObj, void *pDst, size_t uDstMemSize);

static size_t
prvReset(void *vObj);

rmp_api_handle_t
RMP_InitAPI(void *vObj)
{
    rmp_data_handle_t hObj = (rmp_data_handle_t) vObj;

    hObj->xAPI.Put         = prvPut;
    hObj->xAPI.PutISR      = prvPutISR;
    hObj->xAPI.Processing  = prvProcessing;
    hObj->xAPI.Reset       = prvReset;

    return (&hObj->xAPI);
}

static size_t
prvPut(void *vObj, void *pSrc, size_t uBytesNumb)
{
    rmp_data_handle_t hObj = (rmp_data_handle_t) vObj;

    return (lwrb_write(&hObj->xLWRB, pSrc, uBytesNumb));
}

static size_t
prvPutISR(void *vObj, void *pSrc, size_t uBytesNumb)
{
    return (prvPut(vObj, pSrc, uBytesNumb));
}

static size_t
prvProcessing(void *vObj, void *pDst, size_t uDstMemSize)
{
    rmp_data_handle_t hObj           = (rmp_data_handle_t) vObj;
    size_t            uRxMessageSize = 0u;
    rmp_return_code   eReturnCode    = rmpIN_PROGRESS;

    do
    {
        eReturnCode =
            hObj->xStateAPI.aFn[RMP_GetState(vObj)](vObj, pDst, uDstMemSize);

        if (eReturnCode == rmpMESSAGE_COPIED)
        {
            uRxMessageSize = rmpONE_MESSAGE_SIZE_IN_BYTES;
        }
    } while (eReturnCode == rmpIN_PROGRESS);

    return (uRxMessageSize);
}

static size_t
prvReset(void *vObj)
{
    rmp_data_handle_t hObj            = (rmp_data_handle_t) vObj;

    size_t uBytesNumbInBuffBeforReset = lwrb_get_full(&hObj->xLWRB);
    lwrb_reset(&hObj->xLWRB);

    RMP_SetState(vObj, rmpSTATE_FIND_FIRST_BYTE);

    return (uBytesNumbInBuffBeforReset);
}
