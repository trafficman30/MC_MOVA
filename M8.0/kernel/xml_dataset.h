
/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         xml_dataset.h
*
*  TITLE:        Mova Kernel: PCMOVA
*
*
*  (c) Copyright TRL Ltd 2016. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/



#if !defined (__XML_DATASET_H)
#define __XML_DATASET_H


#include "MVTypes.h"
#include "gbltypes.h" /* For MOVA_CONST_DATA_STRUCT */


void        XML_DataSet_Initialise(        void );
MVBool      XML_DataSet_IsInitialised(     void );
void        XML_DataSet_DeInitialise(      void );

MVStatus    XML_DataSet_ReadAllDataPlansFromFile(        const char *, const char * );
MVStatus	XML_DataSet_ReadSpecificDataPlansFromFile(const char * datasetFileName, MVInt8 dp1Index, MVInt8 dp2Index, MVInt8 dp3Index, MVInt8 dp4Index);

MVStatus	XML_DataSet_UpdateDatasetFile();
MVStatus	XML_DataSet_GetDatasetFileInfo(char * file_name, MVInt * file_size, MVInt8 * controller_id);
void		XML_DataSet_GetLatestDatasetFileInfo(char * file_path_and_name, MVInt * file_size);
MVInt8		XML_GetRunningControllerID();

/* Called from within MOVA kernel - see Datahand.c */
MVBool      XML_DataSet_GetDefault(        MOVA_CONST_DATA_STRUCT * );

MVUInt32 XML_DataSet_GetCRC32();
void	 XML_DataSet_SetCRC32(MVUInt32 crc32);

#endif /* __XML_DATASET_H */