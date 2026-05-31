//
//  Project     Chameleon
//
//  Copyright 	Dynniq UK Ltd 2018
//
//  All rights reserved
//
//  The information contained herein is the property of Peek Traffic Limited
//  and is supplied without any liability for errors or omissions. No part
//  may be reproduced or copied except as authorised by contract or other
//  written permission. The copyright and foregoing restriction on
//  reproduction and use extend to all media in which the information may be
//  embodied.
//
//  \file   108NewL.c
//
//	\brief  MOVA licence creation facility for MOVA M8.0 kernel
//
//	\author Andrew Leigh
//
//	\date   27-May-2018
//

static char NewL_c_rcsid[] = "$Id: ";

/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdarg.h>
#include "000Common.h"
#include "504CmnLib.h"
#include "108MovaLib.h"

/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/

/* --------------------------------------------------------------------------- *
 *                     MACRO DEFINITIONS                                       *
 * --------------------------------------------------------------------------- *
*/

/*
 * --------------------------------------------------------------------------- *
 *                     TYPE DEFINITIONS                                        *
 * --------------------------------------------------------------------------- *
 */


/* --------------------------------------------------------------------------- *
 *                     GLOBAL DATA REFERENCES                                  *
 * --------------------------------------------------------------------------- *
*/
equip_id_text_t gMovaEqId;
int GMySocket = 0;
/* --------------------------------------------------------------------------- *
 *                           FUNCTION REFERENCES                               *
 * --------------------------------------------------------------------------- *
*/

//  \fn     main()
//  \brief  Main for module
//	\param	argc int argument count 
//	\param	argv char** argument pointer
//  \retval int success = 0 failure = 1
//
int main(int argc, char **argv)
{
    lice_type_t type;
    int streams = 0;
    char lice_type[LICENCE_STRLEN];
    char lice_provider[LICENCE_STRLEN];
    char lice_owner[LICENCE_STRLEN];
    // char *LiceTypPtr = lice_type;
    char *LiceProPtr = lice_provider;
    char *LiceOwnPtr = lice_owner;

    // parse args
    if (argc < 3 || argc > 5)
    {
        printf("incorrect number of parameters.\n");
        return EX_OK;
    }

    streams = atoi(argv[1]);
    bzero(lice_type, sizeof(lice_type));
    bzero(lice_provider, sizeof(lice_provider));
    bzero(lice_owner, sizeof(lice_owner));

    //
    // streams are required
    //
    if (streams <0 || streams > MAX_MOVA_IO_ENTRIES)
    {
        printf("incorrect number of streams.\n");
        return EX_OK;
    }

    //
    // type is required
    //
    if (argv[2] != NULL)
    {
        strncpy(lice_type, argv[2], LICENCE_STRLEN);
        lice_type[LICENCE_STRLEN-1] = '\0';

        if (strcasecmp(lice_type, "PERMANENT") == 0)
        {
            type = PERMANENT;
        }
        else if (strcasecmp(lice_type, "TEMPORARY") == 0)
        {
            type = TEMPORARY;
        }
        else if (strcasecmp(lice_type, "UPGRADE") == 0)
        {
            type = UPGRADE;
        }
        else
        {
            printf("incorrect type.\n");
            return EX_OK;
        }
    }

    //
    // optional parameters
    //
    if (argc >= 3 && argv[3] != NULL)
    {
        strncpy(lice_provider, argv[3], LICENCE_STRLEN);
        lice_provider[LICENCE_STRLEN-1] = '\0';
    }
    else
    {
        LiceProPtr = NULL;
    }

    if (argc >= 4 && argv[4] != NULL)
    {
        strncpy(lice_owner, argv[4], LICENCE_STRLEN);
        lice_owner[LICENCE_STRLEN-1] = '\0';
    }
    else
    {
        LiceOwnPtr = NULL;
    }

    MovaLibCreateLicence(streams, type, LiceProPtr, LiceOwnPtr, 0);
    return EX_OK;
}   // main()
