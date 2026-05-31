#if !defined (__MVERROR_H)
#define __MVERROR_H

/*
    Name:           MVError.h
    Author (Date):  ihenderson, 29/11/05
    Platform:       PC
    Purpose:        Common error handling functionality

    Last revision:  $Revision:: 1                           $
    Last changed:   $Date:: 5/12/05 11:14                   $
    Changed by:     $Author:: Ihenderson                    $

    $History: MVError.h $
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:14
    Created in $/PCMOVA/MVUtils
*/

/*
	TODO:
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"

#define xg_WIN_ERROR_INVALID	((WinError)-1)

typedef MVInt32 WinError;


MVSize MVError_DecodeWin(			WinError, char *, MVSize );
MVSize MVError_BuildDefaultMessage(	char *, MVSize, WinError );


#if defined (__cplusplus)
    }
#endif

#endif /* __MVERROR_H */