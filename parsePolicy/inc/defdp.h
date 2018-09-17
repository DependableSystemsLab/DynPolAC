/*=============================================================================

University of British Columbia (UBC) 2016-2017
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
Define Data Point Application.

==============================================================================*/
#ifndef DEFDP_H_
#define DEFDP_H_

#include "minicloud.h"
#include "minicloudpolicy.h"
#include <stdbool.h>

typedef void (*PARSE_fnEndElementHandler)( void *userData,
                                               const char *element );

typedef void (*PARSE_fnStartElementHandler)( void *userData,
                                               const char *element,
                                               const char **attribute );

/*============================================================================*/

/*! dynamic point creation options */
#define PARSE_OPT_NONE                ( 0 )
#define PARSE_OPT_SUPPRESS_GUID       ( 1 )

#define PARSE_MAX_ALIAS               ( 20 )

/*! string length of the time - ISO 8601 - 35 characters */
#define TIME_STR_LENGTH ( strlen( "YYYY-MM-DDThh:mm:ss.nnnnnnnnn-zzzz#" ) )

/*============================================================================*/

/*! the tzUserData structure is passed to all XML element and character
 *  processing callback functions by the XML parser */
typedef struct zUserData
{
    /*! handle to the Data Point Manager */
    DP_HANDLE hDPRM;

    /*! character data buffer used to store all XML element data */
    char char_data_buffer[1024];

    /*! current offset within the char_data_buffer */
    size_t offset;

    /*! the start offset in the character buffer for the current XML element */
    size_t start_offset;

    /*! flag indicating if a buffer overflow has occurred during processing */
    bool overflow;

    /*! original instance identifier requested for the data point */
    uint32_t requestedInstanceID;

    /*! instance identifier used when creating the data points.  This
     * will be the same as the requestedInstanceID except when the instance
     * identifier has been encoded directly into the name, in which case
     * the instanceID will be set to zero */
    uint32_t instanceID;

    /*! base flags to apply to all data points */
    uint16_t flags;

    /*! data point info structure to be passed to SYSVAR_fnCreate() */
    DP_tzINFO dpInfo[2];

    /*! indicates if we are processing externally defined data */
    bool extdata;

    /*! pointer to a NULL terminated string containing the library name */
    char *pExtLibName;

    /*! pointer to tag data */
    char *pTags;

    /*! pointer to the data point alias */
    char *pAlias[PARSE_MAX_ALIAS];

    /*! number of aliases we have seen so far for this point */
    int aliasIndex;

    /*! pointer to meta data */
    tzDataPointMetaData *pMetaData;

    /*! flag to indicate if we are currently processing meta data */
    bool processingMeta;

    /*! pointer to extended data objects */
    tzDatapointExtData *pExtData;

    /*! pointer to current extended data object */
    tzDatapointExtData *pCurrentExtData;

    /*! options for modifying the behavior of dynamic point creation */
    uint32_t options;

    /*! pointer to an external start element handler */
    PARSE_fnStartElementHandler start_element_handler;

    /*! pointer to an external end element handler */
    PARSE_fnEndElementHandler end_element_handler;

    /*! callback function to invoke for every data point created */
    void (*pCallback)(DP_tzINFO *ptzInfo,
                      uint32_t instanceID,
                      void *pcbData );

    /* data to pass to the callback function */
    void *pcbData;

} tzUserData;


/*! the tzPolicyData structure is passed to all Policy XML element and character
 *  processing callback functions by the XML parser */
typedef struct zPolicyData
{
	/*! handle to the Data Point Manager */
	DP_HANDLE hDPRM;

    /*! character data buffer used to store all XML element data */
    char char_data_policy_buffer[1024];

    /*! current offset within the char_data_buffer */
    size_t offset;

    /*! the start offset in the character buffer for the current XML element */
    size_t start_offset;

    /*! flag indicating if a buffer overflow has occurred during processing */
    bool overflow;

    /*! policy attributes are collected here ready to be sent to the server */
    struct zPOLICY policy;

    /*! container that converts the string ISO8601 time to the tm struct */
    struct tm tm_time;

    /*! container for holding the string time */
    char timeString[ 128 ];

    /*! pointer to an external start element handler */
    PARSE_fnStartElementHandler start_element_handler;

    /*! pointer to an external end element handler */
    PARSE_fnEndElementHandler end_element_handler;

} tzPolicyData;


int PARSE_fnCreate( DP_HANDLE hDPRM,
		              uint32_t instanceID,
		              char *filename,
		              uint16_t flags,
		              void (*pCallback)(DP_tzINFO *ptzInfo,
		                                uint32_t instanceID,
		                                void *pcbData),
		              void *pcbData,
		              uint32_t options );


int PARSE_fnPolicyCreate(DP_HANDLE hDPRM, char *filename);
int PARSEXACML_fnPolicyCreate( DP_HANDLE hDPRM, char *filename);
int DP_fnPolicyHouseKeeping( DP_HANDLE hDPRM );

#endif /* DEFDP_H_ */
