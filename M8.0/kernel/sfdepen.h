/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sfdepend.c
*
*  TITLE:        MOVA Satutation flow dependent varaibles calculator
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			Public interface to MOVA satflow depedent paramater calculation functionality.
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	22-May-2013		7.0.6		AB		AK		M7.0.6			Issue M7.0.6
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (MOVASATFLOWDEPENDENCY_H)
#define MOVASATFLOWDEPENDENCY_H

#include "mova_types.h"

void MovaSatFlowDependantVar_Initialise_(void);
void MovaSatFlowDependantVar_ReInitialise_(void);
void MovaSatFlowDependantVar_UpdateDependentLaneVariables_(uint8 laneNo);
void MovaSatFlowDependantVar_UpdateDependentLinkVariables_(uint8 linkNo);
int MovaSatFlowDependantVar_GetLinkIdFromLaneId_(int laneId);
int MovaSatFlowDependantVar_GetLinkNoFromLaneNo_(int laneNo);

/* for Repository_Type */
#include "repository.h"
/* Repository access functions from datahand.c */
extern int GetActiveRepositoryNo();
//extern const Repository_Type *GetActiveRepositoryPtr();

#endif /*MOVASATFLOWDEPENDENCY_H*/