/*
 * Copyright (c) 2000  Microsoft Corporation
 * Copyright (c) 2020 Oracle and/or its affiliates.
 */

#ifndef _NPROTDEBUG__H
#define _NPROTDEBUG__H

//Message verbosity: lower values indicate higher urgency
#define DL_EXTRA_LOUD       20
#define DL_VERY_LOUD        10
#define DL_LOUD             8
#define DL_INFO             6
#define DL_WARN             4
#define DL_ERROR            2
#define DL_FATAL            0

#if DBG
extern INT                ndisprotDebugLevel;

#define DEBUGP(lev, stmt)                                               \
        {                                                               \
            if ((lev) <= ndisprotDebugLevel)                             \
            {                                                           \
                DbgPrint("Ndisprot: "); DbgPrint stmt;                   \
            }                                                           \
        }

#define NPROT_ASSERT(exp)                                                \
        {                                                               \
            if (!(exp))                                                 \
            {                                                           \
                DbgPrint("Ndisprot: assert " #exp " failed in"           \
                    " file %s, line %d\n", __FILE__, __LINE__);         \
                DbgBreakPoint();                                        \
            }                                                           \
        }

#define NPROT_SET_SIGNATURE(s, t)\
        (s)->t##_sig = t##_signature;

#define NPROT_STRUCT_ASSERT(s, t)                                        \
        if ((s)->t##_sig != t##_signature)                              \
        {                                                               \
            DbgPrint("ndisprot: assertion failure"                       \
            " for type " #t " at 0x%p in file %s, line %d\n",           \
             s, __FILE__, __LINE__);                            \
            DbgBreakPoint();                                            \
        }
#else
#define DEBUGP(lev, stmt)
#define DEBUGPDUMP(lev, pBuf, Len)

#define NPROT_ASSERT(exp)
#define NPROT_SET_SIGNATURE(s, t) UNREFERENCED_PARAMETER(s)
#define NPROT_STRUCT_ASSERT(s, t) UNREFERENCED_PARAMETER(s)
#endif // DBG
#endif // _NPROTDEBUG__H
