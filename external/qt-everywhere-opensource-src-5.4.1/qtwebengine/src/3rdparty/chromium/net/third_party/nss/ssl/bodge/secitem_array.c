/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Support routines for SECItemArray data structure.
 */

#include "nssutil.h"
#include "seccomon.h"
#include "secitem.h"
#include "secerr.h"
#include "secport.h"

#define NSSUTIL_VERSION_NUM \
    (NSSUTIL_VMAJOR * 10000 + NSSUTIL_VMINOR * 100 + NSSUTIL_VPATCH)
#if NSSUTIL_VERSION_NUM < 31500
// Added in NSS 3.15.
typedef struct SECItemArrayStr SECItemArray;

struct SECItemArrayStr {
    SECItem *items;
    unsigned int len;
};
#endif

SECItemArray *
SECITEM_AllocArray(PLArenaPool *arena, SECItemArray *array, unsigned int len)
{
    SECItemArray *result = NULL;
    void *mark = NULL;

    if (arena != NULL) {
        mark = PORT_ArenaMark(arena);
    }

    if (array == NULL) {
        if (arena != NULL) {
            result = PORT_ArenaZAlloc(arena, sizeof(SECItemArray));
        } else {
            result = PORT_ZAlloc(sizeof(SECItemArray));
        }
        if (result == NULL) {
            goto loser;
        }
    } else {
        PORT_Assert(array->items == NULL);
        result = array;
    }

    result->len = len;
    if (len) {
        if (arena != NULL) {
            result->items = PORT_ArenaZNewArray(arena, SECItem, len);
        } else {
            result->items = PORT_ZNewArray(SECItem, len);
        }
        if (result->items == NULL) {
            goto loser;
        }
    } else {
        result->items = NULL;
    }

    if (mark) {
        PORT_ArenaUnmark(arena, mark);
    }
    return(result);

loser:
    if ( arena != NULL ) {
        if (mark) {
            PORT_ArenaRelease(arena, mark);
        }
        if (array != NULL) {
            array->items = NULL;
            array->len = 0;
        }
    } else {
        if (result != NULL && array == NULL) {
            PORT_Free(result);
        }
        /*
         * If array is not NULL, the above has set array->data and
         * array->len to 0.
         */
    }
    return(NULL);
}

static void
secitem_FreeArray(SECItemArray *array, PRBool zero_items, PRBool freeit)
{
    unsigned int i;

    if (!array || !array->len || !array->items)
        return;

    for (i=0; i<array->len; ++i) {
        SECItem *item = &array->items[i];

        if (item->data) {
            if (zero_items) {
                SECITEM_ZfreeItem(item, PR_FALSE);
            } else {
                SECITEM_FreeItem(item, PR_FALSE);
            }
        }
    }
    PORT_Free(array->items);
    array->items = NULL;
    array->len = 0;

    if (freeit)
        PORT_Free(array);
}

void SECITEM_FreeArray(SECItemArray *array, PRBool freeit)
{
    secitem_FreeArray(array, PR_FALSE, freeit);
}

void SECITEM_ZfreeArray(SECItemArray *array, PRBool freeit)
{
    secitem_FreeArray(array, PR_TRUE, freeit);
}

SECItemArray *
SECITEM_DupArray(PLArenaPool *arena, const SECItemArray *from)
{
    SECItemArray *result;
    unsigned int i;

    if (!from || !from->items || !from->len)
        return NULL;

    result = SECITEM_AllocArray(arena, NULL, from->len);
    if (!result)
        return NULL;

    for (i=0; i<from->len; ++i) {
        SECStatus rv = SECITEM_CopyItem(arena,
                                        &result->items[i], &from->items[i]);
        if (rv != SECSuccess) {
            SECITEM_ZfreeArray(result, PR_TRUE);
            return NULL;
        }
    }

    return result;
}
