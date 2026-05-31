/*
    Name:           PCDatSet.c
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        PCDatSet provides functionality for setting
                    and retrieving the MOVA kernel's default dataset.  
                    PCDatSet supports the dataset version defined
                    in Gbltypes.h (if compiling with the MOVA 5 kernel
                    this will be "M5.0", MOVA 6 kernel - "M6.0", etc.).
                    
                    In embedded MOVA, the default dataset is stored 
                    in the MOVA Kernel executable by including "hugemova.sa".
                    The Hugemova dataset is just a placeholder, which is
                    overwritten when a junction-specific MOVA dataset is
                    downloaded to MOVA during the MOVA commissioning process.
                    In PCMOVA, it is possible to set the default dataset
                    to a real dataset using the functionality in this module
                    (the dataset is passed to PCMOVA as a command line param).
                    This allows the PCMOVA Kernel to be started automatically
                    from the PCMOVA Linker with an appropriate dataset without
                    the user having to go through the relatively involved 
                    process of downloading a dataset to PCMOVA using MOVA Comm.
                    
                    Synchronisation Note:
                    PCDatSet not thread-safe - all calls to PCDatSet
                    should be made from the same thread.
                    Default dataset should be set before the MOVA 
                    kernel is started. Even if it wasn't, there aren't 
                    any potential synchronisation issues now as monitr() 
                    is called from the main thread.
                    
    Last revision:  $Revision:: 7                           $
    Last changed:   $Date:: 24/05/06 11:02                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pcdatset.c $
 * 
 * *****************  Version 7  *****************
 * User: Ihenderson   Date: 24/05/06   Time: 11:02
 * Updated in $/PCMOVA
 * 
 * *****************  Version 6  *****************
 * User: Ihenderson   Date: 19/05/06   Time: 13:28
 * Updated in $/PCMOVA
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 9/03/06    Time: 14:04
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:15
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:02
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
    TODO:
    
    
*/


#include "stdafx.h"
#include "pcdatset.h"

#include <string.h> /* for strncpy, strncmp etc. */

#include "hugemova.sa" /* The default dataset to use if none is supplied */

#define g_MODULE_NAME       ("PCDatSet")


/* MOVA_CONST_DATA_STRUCT is defined in Gbltypes.h.
Note that in PCMOVA it isn't a const struct as it can be changed. */
static MOVA_CONST_DATA_STRUCT   g_defaultDataset;

/* Whether the default dataset is real (passed to the PCMOVA Kernel
as a command line parameter) or just the Hugemova placeholder. */
static MVBool                   g_isDefaultDatasetReal = MV_FALSE;

static MVBool                   g_isInitialised = MV_FALSE;


static MVStatus PCDatSet_ReadHeader(    FILE * p_datasetFile, char headerData[] );
static MVStatus PCDatSet_ReadSplit(     FILE * p_datasetFile, 
                                        MVInt splitFields[], MVInt splitLen );
static MVStatus PCDatSet_ReadChecksum(  const MOVA_CONST_DATA_STRUCT * );


/*
    Name:           PCDatSet_Initialise
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        Initialises the PCMOVA dataset module by
                    setting the default dataset to the Hugemova 
                    placeholder dataset included via "hugemova.sa".
    Inputs:         
    Returns:        
    Remarks:
*/
void PCDatSet_Initialise( void )
{
    TRACE_ASSERT( !g_isInitialised, "Dataset handler already initialised." );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Initialising dataset handler..." );   

    /* Initialise the default dataset to the Hugemova dataset */
    g_defaultDataset = mova_site_data;    
    g_isDefaultDatasetReal = MV_FALSE;

    g_isInitialised = MV_TRUE;

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Dataset handler initialised." );
        
} /* PCDatSet_Initialise */


/*
    Name:           PCDatSet_DeInitialise
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        Deinitialises the PCMOVA dataset module.
    Inputs:         
    Returns:        
    Remarks:
*/
void PCDatSet_DeInitialise( void )
{
    if ( g_isInitialised )
    {
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Deinitialising dataset handler..." );   

        g_isInitialised = MV_FALSE;

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Dataset handler deinitialised." );
    }
    
} /* PCDatSet_DeInitialise */


/*
    Name:           PCDatSet_DeInitialise
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        Determines whether the PCMOVA dataset module is
                    initialised.    
    Inputs:         
    Returns:        True if module initialised; False otherwise.
    Remarks:
*/
MVBool PCDatSet_IsInitialised( void )
{
    return ( g_isInitialised );
    
} /* PCDatSet_IsInitialised */

#ifndef M8_ENABLED
/*
    Name:           PCDatSet_SetDefault
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Reads in the default dataset to start MOVA with 
                    from the given file
    Inputs:         The name and full path of the file
    Returns:        Success or failure (if a file operation failed)
                    Extended error information can be obtained using
                    PCError_GetLast...
    Remarks:        
*/
MVStatus PCDatSet_SetDefault( const char * datasetFileName )
{
    MVStatus    status = MV_SUCCESS;
    FILE *      p_datasetFile;
    int         fileError;    
    
    TRACE_ASSERT( g_isInitialised, "PC Dataset module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( datasetFileName, "Dataset file name invalid." );
    
    TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Setting default dataset to %s...", datasetFileName );

    /* Open the dataset file */
    p_datasetFile = fopen( datasetFileName, "r" );
        
    if ( p_datasetFile != NULL )
    {
        /* Read in all the data from the dataset file.
        SPLITs are sections of data within the MOVA dataset - see Gbltypes.h */
        status = PCDatSet_ReadHeader( p_datasetFile, 
            &( g_defaultDataset.sitename[0] ) );
        if ( status == MV_SUCCESS )
        {
            status = PCDatSet_ReadSplit( p_datasetFile, 
                &( g_defaultDataset.stages_to_stgdem[0] ), SPLIT1 );
        }
        if ( status == MV_SUCCESS )
        {
            status = PCDatSet_ReadSplit( p_datasetFile, 
                &( g_defaultDataset.lgreen_to_pfacil[0] ), SPLIT2 );
        }
        if ( status == MV_SUCCESS )
        {
            status = PCDatSet_ReadSplit( p_datasetFile, 
                &( g_defaultDataset.emxmax_to_nmainl[0] ), SPLIT3 );
        }
        if ( status == MV_SUCCESS )
        {
            status = PCDatSet_ReadSplit( p_datasetFile, 
                &( g_defaultDataset.llanes_to_candet[0] ), SPLIT4 );
        }
        if ( status == MV_SUCCESS )
        {
            status = PCDatSet_ReadSplit( p_datasetFile, 
                &( g_defaultDataset.epext_to_mixout[0] ), SPLIT5 );
        }
        if ( status == MV_SUCCESS )
        {
            status = PCDatSet_ReadSplit( p_datasetFile, 
                &( g_defaultDataset.det_to_assocd[0] ), SPLIT6 );
        }
        if ( status == MV_SUCCESS )
        {
            status = PCDatSet_ReadSplit( p_datasetFile, 
                &( g_defaultDataset.cruisin_to_checksm[0] ), SPLIT7 );
        }
        if ( status == MV_SUCCESS )
        {
            status = PCDatSet_ReadChecksum( &g_defaultDataset );
        }

        /* Close the dataset file */
        fileError = fclose( p_datasetFile );
        if ( fileError != 0 )
        {
            PCError_SetLast( PCMOVA_ERROR_DATASET_CLOSE, NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }
    }
    else
    {
        PCError_SetLast( PCMOVA_ERROR_DATASET_OPEN, NULL, g_MODULE_NAME );
        status = MV_FAILURE;
    }

    if ( status == MV_SUCCESS )
    {
        g_isDefaultDatasetReal = MV_TRUE;

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Default dataset set successfully." );
    }

    return ( status );

} /* PCDatSet_SetDefault */

/*
    Name:           PCDatSet_ReadHeader
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Reads the "header" data - filename, date/time etc.
                    from the given file
    Inputs:         Dataset file
                    Array into which to read header data
                    Length of header data
    Returns:        Status (success/failure)
                    Extended error information can be obtained using
                    PCError_GetLast...
    Remarks:
*/
static MVStatus PCDatSet_ReadHeader( FILE * p_datasetFile, char headerData[] )
{   
    #define HEADER_FIELD_COUNT (5)

    MVStatus        status = MV_SUCCESS;
    int             readCount;
    char            readBuffer[ 64 ];
    char *          p_header;
    MVInt           headerFieldIndex;    
    
    /* Header information in the MOVA dataset - 
    field sizes defined in Gbltypes.h */
    const MVInt     headerFieldLengths[ HEADER_FIELD_COUNT ] = { 
        xgnFILE_NAME_STR_LEN, 
        xgnVERSION_STR_LEN,
        xgnTIME_STR_LEN,
        xgnDATE_STR_LEN,
        xgnTITLE_STR_LEN };
    
    TRACE_ASSERT( g_isInitialised, "PC Dataset module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_datasetFile, "No dataset file." );
    TRACE_ASSERT_VALID_ADDRESS( headerData, "No header data to set." );

    p_header = &( headerData[0] );
    
    headerFieldIndex = 0; 
    readCount = 1;

    /* Read in the header data */
    while ( headerFieldIndex != HEADER_FIELD_COUNT && readCount == 1 )
    {
        memset( readBuffer, 0, sizeof( readBuffer ) );

        /* Read all characters up to the end of line */
        readCount = fscanf( p_datasetFile, "%[^\n]\n", readBuffer );

        strncpy( p_header, readBuffer, headerFieldLengths[ headerFieldIndex ] );
        p_header += headerFieldLengths[ headerFieldIndex ];

        ++headerFieldIndex;
    }

    if ( readCount != 1 )
    {
        PCError_SetLast( PCMOVA_ERROR_DATASET_READ, NULL, g_MODULE_NAME );
        status = MV_FAILURE;
    }

    /* Check that the dataset version is correct - 
       xgszVERSION_MOVA is defined in Gbltypes.h */
    else
    {
        const Ccomshr * p_headerStruct = (Ccomshr *) &( headerData[0] );

        if ( strncmp( p_headerStruct->version, 
            xgszVERSION_MOVA, strlen( xgszVERSION_MOVA ) ) != 0 )
        {
            PCError_SetLast( PCMOVA_ERROR_DATASET_VERSION, NULL, g_MODULE_NAME );
            status = MV_FAILURE;    
        }
    }

    return ( status );

} /* PCDatSet_ReadHeader */


/*
    Name:           PCDatSet_ReadSplit
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Reads a "split" of data from the given dataset file
                    Splits are sections of the MOVA dataset defined 
                    in Gbltypes.h.
    Inputs:         Dataset file
                    Array into which to read split data
                    Length of split
    Returns:        Status (success/failure)
                    Extended error information can be obtained using
                    PCError_GetLast...
    Remarks:
*/
static MVStatus PCDatSet_ReadSplit( FILE * p_datasetFile, 
                                    MVInt splitFields[], MVInt splitLen )
{
    MVStatus status = MV_SUCCESS;
    MVInt          splitFieldIndex;
    int             readCount;
    char            readBuffer[ 8 ];

    TRACE_ASSERT( g_isInitialised, "PC Dataset module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_datasetFile, "No dataset file." );
    TRACE_ASSERT_VALID_ADDRESS( splitFields, "No split to set." );
    TRACE_ASSERT( splitLen > 0, "Split length is zero." );

    splitFieldIndex = 0; 
    readCount = 1;

    /* Read in the split data */
    while ( splitFieldIndex != splitLen && readCount == 1 )
    {
        readCount = fscanf( p_datasetFile, "%s\n", readBuffer );

        splitFields[ splitFieldIndex ] = atoi( readBuffer );

        ++splitFieldIndex;
    }

    if ( readCount != 1 )
    {
        PCError_SetLast( PCMOVA_ERROR_DATASET_READ, NULL, g_MODULE_NAME );
        status = MV_FAILURE;
    }

    return ( status );

} /* PCDatSet_ReadSplit */


/*
    Name:           PCDatSet_ReadChecksum
    Author (Date):  ihenderson, 30/11/05
    Platform:       PC
    Purpose:        PCDatSet_ReadChecksum compares the stored dataset 
                    checksum with a newly calculated checksum for the dataset.
    Inputs:         Default dataset struct.
    Returns:        Status (success = checksum ok)
                    Extended error information can be obtained using
                    PCError_GetLast...
    Remarks:        
*/
static MVStatus PCDatSet_ReadChecksum( const MOVA_CONST_DATA_STRUCT * p_dataset )
{
    #define CHECKSUM_FIELDS_NUM (4)

    /* ADD_SPLIT sums all the data within the given split (section) 
    of the MOVA dataset*/
    #define ADD_SPLIT( first, count, split, p_checksum )        \
    {                                                           \
        MVInt dataItem;                                         \
        for ( dataItem = first; dataItem < count; ++dataItem )  \
        {                                                       \
            (*p_checksum) += split[ dataItem ];                 \
        }                                                       \
    }

    MVStatus    status = MV_SUCCESS;
    MVUInt32    checksumCalc = 0;
    MVUInt32    checksumStored = 0;
 
    /* Sum the data in each section of the dataset.
    See Gblytypes.h for SPLIT definitions. */
    ADD_SPLIT( 0, SPLIT1, p_dataset->stages_to_stgdem, &checksumCalc );
    ADD_SPLIT( 0, SPLIT2, p_dataset->lgreen_to_pfacil, &checksumCalc );
    ADD_SPLIT( 0, SPLIT3, p_dataset->emxmax_to_nmainl, &checksumCalc );
    ADD_SPLIT( 0, SPLIT4, p_dataset->llanes_to_candet, &checksumCalc );
    ADD_SPLIT( 0, SPLIT5, p_dataset->epext_to_mixout, &checksumCalc );
    ADD_SPLIT( 0, SPLIT6, p_dataset->det_to_assocd, &checksumCalc );
    ADD_SPLIT( 0, SPLIT7 - CHECKSUM_FIELDS_NUM, 
        p_dataset->cruisin_to_checksm, &checksumCalc );

    checksumStored = ( p_dataset->cruisin_to_checksm[ SPLIT7 - 2 ] * 1000 )
        + ( p_dataset->cruisin_to_checksm[ SPLIT7 - 1 ] );

    if ( checksumCalc != checksumStored )
    {
        PCError_SetLast( PCMOVA_ERROR_DATASET_CHECKSUM, NULL, g_MODULE_NAME );
        status = MV_FAILURE;
    }
  
    return ( status );

} /* PCDatSet_ReadChecksum */


/*
    Name:           PCDatSet_GetDefault
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Returns the default dataset in the format expected
                    by Movsub() in Datahand.c
    Inputs:         The struct to be filled with the default dataset
    Returns:        Whether the default dataset is a "real" default.
                    If the default dataset is Hugemova, it isn't real - 
                    i.e. it is just a placeholder that can be overwritten 
                    automatically when the user downloads a dataset.
                    Otherwise it is a real dataset passed from the command line,
                    so shouldn't be automatically overwritten (see Datahand.c).
    Remarks:
*/
MVBool PCDatSet_GetDefault( MOVA_CONST_DATA_STRUCT * p_defaultDataset )
{
    TRACE_ASSERT( g_isInitialised, "PC Dataset module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_defaultDataset, "No default dataset to set." );

   (*p_defaultDataset) = g_defaultDataset;

    return ( g_isDefaultDatasetReal );

} /* PCDatSet_GetDefault */


#endif