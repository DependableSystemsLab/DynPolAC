/*=============================================================================

University of British Columbia (UBC) 2016
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
Parse Data Points.

==============================================================================*/
/*============================================================================*/
/*!

\file  parse.c

@brief
    Parse data points

\details

    This module provides a mechanism to create data points dynamically
    at run time based on data point descriptors read from a file

*/

/*==============================================================================
                              Includes
==============================================================================*/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <syslog.h>
#include "minicloud.h"
#include "defdp.h"
#include "expat.h"
#include "brushstring.h"

/*==============================================================================
                              Defines
==============================================================================*/

#ifdef XML_LARGE_SIZE
#if defined(XML_USE_MSC_EXTENSIONS) && _MSC_VER < 1400
#define XML_FMT_INT_MOD "I64"
#else
#define XML_FMT_INT_MOD "ll"
#endif
#else
#define XML_FMT_INT_MOD "l"
#endif

/*==============================================================================
                               Macros
==============================================================================*/

/*==============================================================================
                                Enums
==============================================================================*/

/*=============================================================================
                              Structures
==============================================================================*/

/*==============================================================================
                        Local/Private Function Protoypes
==============================================================================*/
static void char_data (void *userData, const XML_Char *s, int len);
static void start_element(void *userData,
                          const char *element,
                          const char **attribute);
static void end_element(void *userData, const char *el);
void parse_fnProcessExtDataStart( tzUserData *ptzUserData, const char **attr );
char *parse_fnAssignInstanceID( char *name,
                        uint32_t *id,
                        char *destbuf,
                        size_t destlen );
static int parse_fnAddTags( tzUserData *ptzUserData, char *pTags );
static int parse_fnSetExtData( tzUserData *ptzUserData );

/*==============================================================================
                           Local/Private Variables
==============================================================================*/
tzUserData userData;

/*==============================================================================
                           Local/Private Constants
==============================================================================*/

/*==============================================================================
                           Function Definitions
==============================================================================*/


/*============================================================================*/
//fn  PARSE_fnCreate
/*!

@brief
    Parse Data Points

    This function opens the specified dynamic variable descriptor file,
    parses the contents, and creates the data points.

@param[in]
    hDPRM
        System Variable Resource Manager Handle ( returned by DP_fnOpen() )

@param[in]
    instanceID
        the instance identifier to apply to all of the dynamically
        created data points.

@param[in]
    filename
        pointer to the dynamic variable descriptor filename

//@param[in]
//    flags
//        flags value to bitwise OR with the data point flags

@param[in]
    pCallback
        pointer to a call-back function which is called for every
        created data point.

@param[in]
    pcbData
        pointer to an externally defined data structure which is passed
        to the call-back function.

@param[in]
    options
        options to modify the behaviour of the dynamic variable creation
        process.

        Supported options are:

            PARSE_OPT_SUPPRESS_GUID

@return
    EOK - parse were created OK
    EINVAL - invalid argument specified
    ENOENT - input file could not be opened
    EIO - XML parsing error

*/
/*============================================================================*/
int PARSE_fnCreate( DP_HANDLE hDPRM,
                      uint32_t instanceID,
                      char *filename,
                      uint16_t flags,
                      void (*pCallback)(DP_tzINFO *ptzInfo,
                                        uint32_t instanceID,
                                        void *pcbData),
                      void *pcbData,
                      uint32_t options )
{
    char buf[BUFSIZ];
    int done = 0;
    FILE *fp;
    XML_Parser parse;

    /* open the data point manager */
    if( hDPRM == NULL )
    {
        fprintf(stderr, "Unable to open System Variable Resource Manager\n");
        return EINVAL;
    }

    /* populate the userData structure */
    memset(&userData, 0, sizeof(userData) );
    userData.hDPRM = hDPRM;
    userData.flags = flags;
    userData.requestedInstanceID = instanceID;
    userData.instanceID = userData.requestedInstanceID;
    userData.pCallback = pCallback;
    userData.pcbData = pcbData;
    userData.extdata = false;
    userData.options = options;

    /* open the input file */
    fp = fopen( filename, "r" );
    if( fp == NULL )
    {
        fprintf(stderr,
                "unable to open data point input file %s\n",
                filename);
        return ENOENT;
    }

    /* create the XML parse */
    parse = XML_ParserCreate(NULL);

    /* set the user data structure to be passed to the callback functions */
    XML_SetUserData(parse, &userData);

    /* set up the start and end element callback handlers */
    XML_SetElementHandler(parse, start_element, end_element);

    /* set up the character data callback handler */
    XML_SetCharacterDataHandler(parse, char_data);

    /* process the input file in BUFSIZ chunks */
    do
    {
        /* read a chunk of the input file */
        int len = (int)fread(buf, 1, sizeof(buf), fp);
        done = len < sizeof(buf);

        /* pass the buffer to the XML parse */
        if (XML_Parse(parse, buf, len, done) == XML_STATUS_ERROR)
        {
            fprintf(stderr,
                  "%s at line %" XML_FMT_INT_MOD "u\n",
                  XML_ErrorString(XML_GetErrorCode(parse)),
                  XML_GetCurrentLineNumber(parse));
            return EIO;
        }
    } while (!done);

    /* close the parse */
    XML_ParserFree(parse);

    /* close the input file */
    fclose( fp );

    return EOK;
}

/*============================================================================*/
//fn  char_data
/*!

@brief
    XML Callback handler for XML element character data

    The char_data function is invoked by the XML parse when XML element
    character data is encountered.  This function may be invoked multiple times
    for each XML element so the function needs to store the XML character data
    into a character buffer for later processing (when the end_element callback
    is invoked).

@param[in]
    userData
        pointer to the userData set by the XML_SetUserData() function.

@param[in]
    s
        pointer to the start of the XML character data for the current XML tag

@param[in]
    len
        length of the XML character data to be processed

*/
/*============================================================================*/
static void char_data (void *userData, const XML_Char *s, int len)
{
    tzUserData *ptzUserData = userData;

    if( userData != NULL && s != NULL )
    {
        if (!ptzUserData->overflow)
        {
            if (len + ptzUserData->offset >=
                sizeof(ptzUserData->char_data_buffer) )
            {
                ptzUserData->overflow = true;
            }
            else
            {
                memcpy(ptzUserData->char_data_buffer + ptzUserData->offset,
                       s,
                       len);
                ptzUserData->offset += len;
            }
        }
    }
}

/*============================================================================*/
/*!

/fn  start_element

@brief
    XML Callback handler invoked when a start element tag is encountered
    by the XML parse.

    The start_element function is invoked by the XML parse when an XML element
    start tag is encountered.  It initializes the state of the DynVars
    data point processor and stores the element tag type.

    - Added support for externally defined element handlers
    - Removed extdata objects from the DP_fnRegister function

@param[in]
    userData
        pointer to the userData set by the XML_SetUserData() function.

@param[in]
    element
        pointer to the name of the XML element which is about to be processed

@param[in]
    attribute
        array of XML attribute pointers (not used)

*/
/*============================================================================*/
static void start_element(void *userData,
                          const char *element,
                          const char **attribute)
{
    int i;

    tzUserData *ptzUserData = userData;
    if( ptzUserData == NULL )
    {
        return;
    }

    ptzUserData->start_offset = ptzUserData->offset;

    if( ptzUserData->extdata == true )
    {
        ptzUserData->start_element_handler( userData, element, attribute );
    }
    else if( strcmp(element, "point") == 0 )
    {
        /* setup to begin parsing a data point creation record */
        ptzUserData->offset = 0;
        ptzUserData->start_offset = 0;
        ptzUserData->instanceID = ptzUserData->requestedInstanceID;
        ptzUserData->processingMeta = false;
        for(i=0;i<PARSE_MAX_ALIAS;i++)
        {
            ptzUserData->pAlias[i] = NULL;
        }

        ptzUserData->aliasIndex = 0;

        /* clear the character data buffer */
        memset( ptzUserData->char_data_buffer,
                0,
                sizeof( ptzUserData->char_data_buffer ) );

        /* clear the systemv variable information structure */
        memset( ptzUserData->dpInfo,
                0,
                sizeof(ptzUserData->dpInfo) );

        /* set up default values for the data point creation */
        ptzUserData->dpInfo[0].flags = ptzUserData->flags;
        ptzUserData->dpInfo[0].fmt = "%s";
        ptzUserData->dpInfo[0].length = 64;
        ptzUserData->dpInfo[0].pDefaultValue = "0";
    }
    else if( strcmp( element, "meta") == 0 )
    {
        /* start processing meta data */
        ptzUserData->pMetaData = DP_fnMetaDataInit( ptzUserData->hDPRM,
                                                        DP_OPTIONS_NONE );

        /* indicate that meta data processing is in progress */
        ptzUserData->processingMeta = true;
    }
    else if( strcmp( element, "extdata")  == 0 )
    {
        parse_fnProcessExtDataStart( ptzUserData, attribute );
        return;
    }
}

/*============================================================================*/
/*!

/fn  end_element

@brief
    XML Callback handler invoked when an end element tag is encountered
    by the XML parse.

    The end_element function is invoked by the XML parse when an XML element
    end tag is encountered.  It populates the data point creation
    structure based on the content of the XML element character data.

    - Added support for <callback> tag
    - Added support for externally defined element handlers
    - Added support for embedding instance identifiers into datapoint names
    - Added support to globally suppress data point GUID assignment
    - Added support for prefixes on aliases
    - Added support for json flag
    - Update retval check because sysvars API has been updated to return errno

@param[in]
    userData
        pointer to the userData set by the XML_SetUserData() function.

@param[in]
    element
        pointer to the name of the XML element which has been processed

*/
/*============================================================================*/
static void end_element(void *userData, const char *element)
{
    tzUserData *ptzUserData = userData;
    char *pElementData;
    static char namebuf[256];
    char aliasbuf[256];
    DP_HANDLE hDataPoint;
    int rc;
    int i;

    if( ptzUserData == NULL )
    {
        return;
    }

    pElementData = &ptzUserData->char_data_buffer[ptzUserData->start_offset];

    /* null terminate the character data */
    char_data(ptzUserData, "\0", 1);
    if( ptzUserData->overflow == true )
    {
        fprintf(stderr, "data overflow processing element: %s\n", element );
        /* we have a data overflow on our input buffer - we cannot
         * do any further processing */
        return;
    }

    if( strcmp( element, "meta") == 0 )
    {
        /* turn off meta data processing */
        ptzUserData->processingMeta = false;
    }

    /* process meta data */
    if( ptzUserData->processingMeta == true )
    {
        if( ptzUserData->pMetaData != NULL )
        {
            /* try to add the element and its data as a key/value meta data pair */
            if( DP_fnMetaDataAdd( ptzUserData->hDPRM,
                                  ptzUserData->pMetaData,
                                  (char *)element,
                                  pElementData,
                                  DP_OPTIONS_NONE ) != EOK )
            {
                syslog(LOG_ERR, "Error adding meta data for %s : %s", element, pElementData);
            }
        }
    }

    if( strcmp(element, "extdata") == 0 )
    {
        ptzUserData->extdata = false;
    }
    else if( ptzUserData->extdata == true )
    {
        ptzUserData->end_element_handler( userData, element );
    }
    else if( strcmp(element, "id") == 0 )
    {
        if( ptzUserData->options == PARSE_OPT_SUPPRESS_GUID )
        {
            ptzUserData->dpInfo[0].ulName = 0L;
        }
        else
        {
            ptzUserData->dpInfo[0].ulName = BRUSHSTRING_fnXtoL( pElementData );
        }
    }
    else if( strcmp(element, "name") == 0 )
    {
    	/* insert the instance identifier into the variable name (if applicable) */
        ptzUserData->dpInfo[0].pName = parse_fnAssignInstanceID(pElementData,
        											    &(ptzUserData->instanceID),
        											    namebuf,
        											    sizeof(namebuf));
    }
    else if( strcmp(element, "alias") == 0 )
    {
        if( ptzUserData->aliasIndex < PARSE_MAX_ALIAS )
        {
            ptzUserData->pAlias[ptzUserData->aliasIndex++] = pElementData;
        }
    }
    else if( strcmp(element, "format") == 0 )
    {
        ptzUserData->dpInfo[0].fmt = pElementData;
    }
    else if( strcmp(element, "type") == 0 )
    {
        if( strcmp( pElementData, "str" ) == 0 )
        {
            ptzUserData->dpInfo[0].type = DP_TYPE_STR;
            if( ptzUserData->dpInfo[0].length == 0 &&
                ptzUserData->dpInfo[0].pDefaultValue != NULL )
            {
                ptzUserData->dpInfo[0].length =
                        strlen( ptzUserData->dpInfo[0].pDefaultValue ) + 1;
            }
        }
        else if( strcmp( pElementData, "uint16") == 0 )
        {
            ptzUserData->dpInfo[0].type = DP_TYPE_UINT16;
            ptzUserData->dpInfo[0].length = sizeof(uint16_t);
        }
        else if( strcmp( pElementData, "uint32") == 0 )
        {
            ptzUserData->dpInfo[0].type = DP_TYPE_UINT32;
            ptzUserData->dpInfo[0].length = sizeof(uint32_t);
        }
        else if( strcmp( pElementData, "sint16") == 0 )
        {
            ptzUserData->dpInfo[0].type = DP_TYPE_SINT16;
            ptzUserData->dpInfo[0].length = sizeof(int16_t);
        }
        else if( strcmp( pElementData, "sint32") == 0 )
        {
            ptzUserData->dpInfo[0].type = DP_TYPE_SINT32;
            ptzUserData->dpInfo[0].length = sizeof(int32_t);
        }
        else if( strcmp( pElementData, "float") == 0 )
        {
            ptzUserData->dpInfo[0].type = DP_TYPE_FLOAT32;
            ptzUserData->dpInfo[0].length = sizeof(float);
        }
        else
        {
            ptzUserData->dpInfo[0].type = 0;
        }
    }
    else if( strcmp(element, "length") == 0 )
    {
        ptzUserData->dpInfo[0].length = atoi(pElementData);
    }
    /*else if( strcmp(element, "flags") == 0 )
    {
        if( strstr( pElementData, "hidden" ) != NULL )
        {
            ptzUserData->dpInfo[0].flags |= DP_FLAG_HIDDEN;
        }

        if( strstr( pElementData, "password" ) != NULL )
        {
            ptzUserData->dpInfo[0].flags |= DP_FLAG_PASSWORD;
        }

        if( strstr( pElementData, "read only" ) != NULL )
        {
            ptzUserData->dpInfo[0].flags |= DP_FLAG_READ_ONLY;
        }

        if( strstr( pElementData, "protected" ) != NULL )
        {
            ptzUserData->dpInfo[0].flags |= DP_FLAG_PROTECTED;
        }

        if( strstr( pElementData, "volatile" ) != NULL )
        {
            ptzUserData->dpInfo[0].flags |= DP_FLAG_VOLATILE;
        }

        if( strstr( pElementData, "critical" ) != NULL )
        {
            ptzUserData->dpInfo[0].flags |= DP_FLAG_CRITICAL;
        }

        if( strstr( pElementData, "json" ) != NULL )
        {
            ptzUserData->dpInfo[0].flags |= DP_FLAG_JSON;
        }
    }*/
    else if( strcmp(element, "tag") == 0 )
    {
        parse_fnAddTags( ptzUserData, pElementData );
    }
//    else if( strcmp(element, "callback") == 0 )
//    {
//        ptzUserData->dpInfo[0].pCallbackFn = pElementData;
//    }
    else if( strcmp(element, "value") == 0 )
    {
        ptzUserData->dpInfo[0].pDefaultValue = pElementData;
    }
//    else if( strcmp(element, "perm.read") == 0 )
//    {
//        ptzUserData->dpInfo[0].permissions.read |=
//            PermissionBitmapGet( pElementData );
//    }
//    else if( strcmp(element, "perm.write") == 0 )
//    {
//        ptzUserData->dpInfo[0].permissions.write |=
//            PermissionBitmapGet( pElementData );
//    }
//    else if( strcmp(element, "perm.execute") == 0 )
//    {
//        ptzUserData->dpInfo[0].permissions.execute |=
//            PermissionBitmapGet( pElementData );
//    }
    else if( strcmp(element, "point") == 0 )
    {
        if( ptzUserData->dpInfo[0].pName != NULL )
        {
            if( ptzUserData->dpInfo[0].fmt == NULL )
            {
                fprintf(stderr,
                        "Missing format for data point: %s\n",
                        ptzUserData->dpInfo[0].pName );
            }
            else if( ptzUserData->dpInfo[0].pDefaultValue == NULL )
            {
                fprintf(stderr,
                        "Missing value for data point: %s\n",
                        ptzUserData->dpInfo[0].pName );
            }
            else
            {
                /* student is permitted to do anything, this is a back door */
                /*ptzUserData->dpInfo[0].permissions.read |=
                    PermissionBitmapGet( "student" );
                ptzUserData->dpInfo[0].permissions.write |=
                    PermissionBitmapGet( "student" );
                ptzUserData->dpInfo[0].permissions.execute |=
                    PermissionBitmapGet( "student" );*/

                /* create the data point */
                int res = DP_fnRegister( ptzUserData->hDPRM,
                                   	   	     ptzUserData->instanceID,
                                   	   	     &ptzUserData->dpInfo[0] );

                if( res != EOK )
                {
//                	fprintf(stderr,"Failed to DP_fnRegister( %s )\n",
//                			ptzUserData->dpInfo[0].pName );
                	fprintf(stderr,".");
                }
                else
                {
                    /* get a handle to the variable we just added */
                    hDataPoint = DP_fnFindByName( ptzUserData->hDPRM,
                                                ptzUserData->dpInfo[0].pName);
                    if( hDataPoint == NULL )
                    {
                        syslog( LOG_ERR,
                                "Cannot get handle for %s",
                                ptzUserData->dpInfo[0].pName );
                    }

                    /* create the alias */
                    for(i=0;i<ptzUserData->aliasIndex;i++)
                    {
                        if( ptzUserData->pAlias[i] != NULL )
                        {
                            /* insert the instance identifier into the
                             * alias (if applicable) */
                            ptzUserData->instanceID =
                                    ptzUserData->requestedInstanceID;
                            parse_fnAssignInstanceID(ptzUserData->pAlias[i],
                                             &(ptzUserData->instanceID),
                                             aliasbuf,
                                             sizeof(aliasbuf));

                            rc = DP_fnAlias( ptzUserData->hDPRM,
                                                 hDataPoint,
                                                 aliasbuf,
                                                 DP_OPTIONS_NONE );
                            if( rc != EOK )
                            {
                                syslog( LOG_ERR,
                                        "unable to create alias %s",
                                        aliasbuf );
                            }
                        }
                    }

                    /* insert any extended data objects (if applicable) */
                    if( ptzUserData->pExtData != NULL )
                    {
                        if( parse_fnSetExtData( ptzUserData ) != EOK )
                        {
                            syslog( LOG_ERR,
                                    "cannot set extended data objects for %s",
                                    ptzUserData->dpInfo[0].pName );
                        }
                    }

                    /* apply tags (if any) */
                    if( ptzUserData->pTags != NULL )
                    {
                        /* set data point tags/attributes */
                        if( DP_fnSetTagsByName( ptzUserData->hDPRM,
                                                ptzUserData->dpInfo[0].pName,
                                                ptzUserData->pTags,
                                                0 ) != EOK )
                        {
                            syslog( LOG_ERR,
                                    "cannot set tags for variable: %s",
                                    ptzUserData->dpInfo[0].pName );
                        }

                        /* free the tags */
                        free( ptzUserData->pTags );
                        ptzUserData->pTags = NULL;
                    }

                    /* invoke (trigger) the datapoint creation
                     * callback function */
                    if( ptzUserData->pCallback != NULL )
                    {
                        ptzUserData->pCallback( &ptzUserData->dpInfo[0],
                                                ptzUserData->instanceID,
                                                ptzUserData->pcbData );
                    }

                    /* apply the meta data if any */
                    if( ( ptzUserData->pMetaData != NULL ) &&
                        ( hDataPoint != NULL ) )
                    {
                        /* assign the meta data to the data point */
                        rc = DP_fnMetaDataAssign( ptzUserData->hDPRM,
                                                     hDataPoint,
                                                     ptzUserData->pMetaData,
                                                     DP_OPTIONS_NONE );
                        if( rc != EOK )
                        {
                            syslog(LOG_ERR,
                                   "Cannot assign meta data to %s",
                                   ptzUserData->dpInfo[0].pName);
                        }

                        /* clear meta data */
                        ptzUserData->pMetaData = NULL;
                    }
                }
            }
        }
    }
    else
    {
        /* We do not recognize this tag.
         * Reset the offset to where it was before we started
         * processing this tag */
        ptzUserData->offset = ptzUserData->start_offset;
    }
}

/*============================================================================*/
//fn  parse_fnProcessExtDataStart
/*!

 @brief
     Function to manage processing of extension data (externally defined)
     in the XML file

     This function manages the processing of the extdata opening tag.

 @param[in]
     ptzUserData
         pointer to the tzUserData structure

 @param[in]
     attr
         array of pointers to section attributes

 @return
     Nothing

 */
/*============================================================================*/
void parse_fnProcessExtDataStart( tzUserData *ptzUserData, const char **attr )
{
    int idx;
    char *pLibName = NULL;
    char *pFuncName = NULL;
    char callbackFunctionName[256];
    void *sharedObj = NULL;

    if( ptzUserData == NULL )
    {
        return;
    }

    if( attr == NULL )
    {
        return;
    }

    for( idx = 0; attr[idx] != NULL ; idx += 2 )
    {
        if( strcmp(attr[idx], "defn") == 0 )
        {
            ptzUserData->pExtLibName = (char *)attr[idx+1];
        }
    }

    if( ptzUserData->pExtLibName != NULL )
    {
        pFuncName = ptzUserData->pExtLibName;
        pLibName = strchr( ptzUserData->pExtLibName, '@' );
        if( pLibName != NULL )
        {
            *pLibName++ = '\0';
        }
        else
        {
            pLibName = ptzUserData->pExtLibName;
            pFuncName = NULL;
        }
    }

    /* try to open the library */
    /* load the shared object */
    if( pLibName != NULL )
    {
        sharedObj = dlopen( (const char *)pLibName, RTLD_NOW );
        if( sharedObj == NULL )
        {
            fprintf(stderr, "unable to find library: %s\n", pLibName);
            return;
        }

        /* build the start_element function name */
        if( pFuncName != NULL )
        {
            sprintf(callbackFunctionName, "%s_start_element_handler", pFuncName);
        }
        else
        {
            strcpy(callbackFunctionName, "start_element_handler" );
        }

        /* resolve the name of the function to its address */
        ptzUserData->start_element_handler =
            (PARSE_fnStartElementHandler)dlsym( sharedObj, callbackFunctionName );

        if( pFuncName != NULL )
        {
            sprintf(callbackFunctionName, "%s_end_element_handler", pFuncName);
        }
        else
        {
            strcpy(callbackFunctionName, "end_element_handler");
        }

        ptzUserData->end_element_handler =
            (PARSE_fnEndElementHandler)dlsym( sharedObj, callbackFunctionName );

        if( ptzUserData->start_element_handler != NULL &&
            ptzUserData->end_element_handler != NULL )
        {
            ptzUserData->extdata = true;
        }
    }
}

/*============================================================================*/
//fn  parse_fnAssignInstanceID
/*!

 @brief
     Insert an instance identifier into a datapoint name string at the location
     indicated by the '%d'

    This function is used to construct a new data point name
    from a template, using the specified data point instance
    identifier.

    For example:

    Input name:  AB%d.V
    Input prefix: /INV1/
    Input Instance Identifier: 12345

    Output name:  /INV1/AB12345.V


     - Added support for a data point prefix
     - Added fix to clear the instanceID when it has already been applied
     inside the prefix

 @param[in]
     name
         pointer to a null terminated array of characters used to format the
         datapoint name

 @param[in]
     prefix
         pointer to a null terminated array of characters containing a prefix
         string to prepend to the data point name

 @param[in]
     id
         data point instance identifier

 @param[in]
     destbuf
         pointer to a buffer to store the output datapoint name

 @param[in]
     destlen
         specifies the maximum size of the generated name string

 @return
     a pointer to a null terminated string containing the final
     data point name with embedded instance identifier

 */
/*============================================================================*/
char *parse_fnAssignInstanceID( char *name,
                        uint32_t *id,
                        char *destbuf,
                        size_t destlen )
{
//    size_t len;
    char *retbuf = destbuf;

	if(( name == NULL ) || ( destbuf == NULL ) || ( id == NULL ))
	{
		return name;
	}

	/* clear the destination buffer */
	memset( destbuf, 0, destlen );

	if( destlen )
	{
	    /* reduce the buffer size by one to ensure we are always
	     * null terminated */
	    destlen--;
	}

	if( strstr( name, "%d") != NULL )
	{
		/* we need to insert the instance ID */
		snprintf(destbuf, destlen, name, *id );

        /* set the instanceID to zero so we do not use it to instance
         * the variable (since the instance is already embedded in the
         * name )
         */
		*id = 0;

		return retbuf;
	}
	else
	{
	    /* append the name to the destination buffer */
	    snprintf(destbuf, destlen, "%s", name );
	}

	return retbuf;
}

/*============================================================================*/
//fn  parse_fnAddTags
/*!

@brief
     Append the specified tags to the tags list in the UserData structure

     This function builds up a list of tags to apply to a data point

@param[in]
     ptzUserData
         pointer to the tzUserData structure

@param[in]
     pTags
         either a single tag name, or a comma separated list of tag names

@return
     EOK on success, or any other negative error code from errno.h on failure

 */
/*============================================================================*/
static int parse_fnAddTags( tzUserData *ptzUserData, char *pTags )
{
    size_t len;
    size_t origlen;
    size_t newlen;

    /* validate the arguments */
    if( ptzUserData == NULL || pTags == NULL )
    {
        return EINVAL;
    }

    /* check the length of the tag(s) */
    len = strlen(pTags);
    if( len < 1 )
    {
        return EINVAL;
    }

    if( ptzUserData->pTags == NULL )
    {
        /* create first tags */
        ptzUserData->pTags = malloc( strlen( pTags ) + 1 );
        if( ptzUserData->pTags == NULL )
        {
            syslog( LOG_ERR,
                    "parse_fnAddTags: memory allocation failed" );
            return ENOMEM;
        }

        /* copy the tag */
        strcpy( ptzUserData->pTags, pTags );
    }
    else
    {
        /* append to existing tags */
        origlen = strlen( ptzUserData->pTags );
        newlen = origlen + len + 2;
        ptzUserData->pTags = realloc( ptzUserData->pTags, newlen );
        if( ptzUserData->pTags == NULL )
        {
            syslog( LOG_ERR,
                    "parse_fnAddTags: memory allocation failed" );
            return ENOMEM;
        }

        ptzUserData->pTags[origlen] = ',';
        strcpy( &ptzUserData->pTags[origlen+1], pTags );
    }

    /* indicate success */
    return EOK;
}

/*============================================================================*/
//parse_fnSetExtData
/*!

 @brief

 @param[in]
     ptzUserData
         pointer to the tzUserData structure

 @return
     EOK on success, or any other negative error code from errno.h on failure

 */
/*============================================================================*/
static int parse_fnSetExtData( tzUserData *ptzUserData )
{
    tzDatapointExtData *pExtData;
    tzDatapointExtData *p;
    tzDatapointExtData *pLast;
    DP_HANDLE hDataPoint;
    int rc = EOK;

    if( ptzUserData == NULL )
    {
        return EINVAL;
    }

    pExtData = ptzUserData->pExtData;
    if( pExtData == NULL )
    {
        return EINVAL;
    }

    /* get a handle to the data point to add extended objects to */
    hDataPoint = DP_fnFindByName( ptzUserData->hDPRM,
                                ptzUserData->dpInfo[0].pName);
    if( hDataPoint == NULL )
    {
        return ESRCH;
    }

    p = pExtData;
    while( p != NULL )
    {
        /* try to set the extended data object in the data point */
        if( DP_fnSetExtData( ptzUserData->hDPRM,
                                 hDataPoint,
                                 p,
                                 p->extDataSize,
                                 DP_OPTIONS_NONE ) != EOK )
        {
            syslog(LOG_ERR, "unable to set extended data object %d for %s",
                    p->extDataType,
                    ptzUserData->dpInfo[0].pName );

            rc = ENOMEM;
        }

        /* store a pointer to the current node */
        pLast = p;

        /* move to the next node */
        p = p->pNext;

        /* free the object */
        free( pLast );
        pLast = NULL;
    }

    /* clear the extended data object list head pointer */
    ptzUserData->pExtData = NULL;

    return rc;
}

//EoF
