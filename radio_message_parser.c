/**
 * @file radio_message_parser.c
 * @author Mickle Isaev (mrraptor26@gmail.com)
 *
 * @brief RMP расшифровывается как <Radio Message Parser>. Библиотека содержит
 * программную реализацию парсера сообщений фиксированной длины и предназначена
 * для выполнения в стиле <Bare Metal>. Реализация парсера соответствует
 * протоколу информационного обмена принятого в автопилота БЛА Альбатрос.
 *
 * Более подробное описание вы можете найти в <radio_message_parser.h>.
 *
 * @version 0.2.0
 * @date 26-07-2023
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

#include <string.h>
#include "radio_message_parser.h"

/**
 * @brief Выполняет сброс структуры инициализации в параметры <по умолчанию>.
 * рекомендуется вызывать данную функцию сразу после объявления структуры
 * инициализации типа <rmp_init_t>.
 *
 * @param[out] pxInit: Указатель на структуру параметров инициализации.
 */
void
RMP_StructInit(rmp_init_t *pxInit)
{
    memset((void *) pxInit, 0, sizeof(rmp_init_t));

    pxInit->uReadBytesThreshold = rmpONE_MESSAGE_SIZE_IN_BYTES * 2;
}

/**
 * @brief Конструктор экземпляра <RMP>.
 *
 * @param[in] pxInit: Указатель на структуру параметров инициализации.
 *
 * @return Возвращает указатель на интерфейс взаимодействия с библиотекой в
 * случае успешной инициализации и NULL в случае ошибки.
 */
rmp_api_handle_t
RMP_Ctor(rmp_init_t *pxInit)
{
    if (pxInit == NULL)
    {
        return (NULL);
    }

    if (pxInit->pMemAlloc == NULL)
    {
        return (NULL);
    }

    if (pxInit->uMemAllocSizeInBytes == 0u)
    {
        return (NULL);
    }

    if (pxInit->hData == NULL)
    {
        return (NULL);
    }

    bool bIsCtorErrorDetect = false;

    rmp_data_handle_t hData = pxInit->hData;
    memset((void *) hData, 0, sizeof(rmp_obj_t));
    /*------------------------------------------------------------------------*/

    hData->uReadBytesThreshold = pxInit->uReadBytesThreshold;
    /*------------------------------------------------------------------------*/

    if (lwrb_init(
            &hData->xLWRB,
            pxInit->pMemAlloc,
            pxInit->uMemAllocSizeInBytes)
        == NULL)
    {
        bIsCtorErrorDetect = true;
    }

    extern rmp_api_handle_t RMP_InitAPI(void *vObj);
    rmp_api_handle_t        hAPI = RMP_InitAPI(hData);
    /*------------------------------------------------------------------------*/

    extern rmp_state_api_handle_t RMP_InitStateAPI(void *vObj);
    (void *) RMP_InitStateAPI(hData);
    /*------------------------------------------------------------------------*/

    if (bIsCtorErrorDetect == true)
    {
        RMP_Dtor(hAPI);

        hAPI = NULL;
    }
    /* if (bIsCtorErrorDetect == true) */

    return (hAPI);
}

bool
RMP_Dtor(rmp_api_handle_t hAPI)
{
    bool bIsObjDestroyed = false;

    if (hAPI != NULL)
    {
        rmp_data_handle_t hObj = (rmp_data_handle_t) hAPI;

        /* Освобождение занимаемых ресурсов */
        lwrb_free(&hObj->xLWRB);
        /*--------------------------------------------------------------------*/

        bIsObjDestroyed = true;
    }
    /* if (hAPI != NULL) */

    return (bIsObjDestroyed);
}
