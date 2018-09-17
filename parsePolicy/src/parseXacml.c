/*=============================================================================

University of British Columbia (UBC) 2017
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
Parse XACML Policy File.

==============================================================================*/
/*============================================================================*/
/*!

@file  parseXacmlPolicy.c

@brief
    Parse XACML policy file

@details

    This module provides a mechanism to dynamically create policy on
    IoT data storage

    A Sample XACML Policy file shall look like this:

<?xml version="1.0" encoding="UTF-8"?>
<Policy PolicyId="0" RuleCombiningAlgId="urn:oasis:names:tc:xacml:1.0:rule-combining-algorithm:deny-overrides">
  <Target>
      <Subjects>
        <Subject>
          <SubjectMatch  MatchId="urn:oasis:names:tc:xacml:1.0:function:string-equal">
            <AttributeValue DataType="http://www.w3.org/2001/XMLSchema#string">subject0</AttributeValue>
            <SubjectAttributeDesignator  AttributeId="urn:oasis:names:tc:xacml:1.0:subject:subject-id" DataType="http://www.w3.org/2001/XMLSchema#string"/>
          </SubjectMatch>
        </Subject>
      </Subjects>
      <Resources>
        <Resource>
          <ResourceMatch MatchId="urn:oasis:names:tc:xacml:1.0:function:string-equal">
            <AttributeValue DataType="http://www.w3.org/2001/XMLSchema#string">recource0</AttributeValue>
            <ResourceAttributeDesignator AttributeId="urn:oasis:names:tc:xacml:1.0:resource:resource-id" DataType="http://www.w3.org/2001/XMLSchema#string"/>
          </ResourceMatch>
        </Resource>
      </Resources>
      <Actions>
        <Action>
          <ActionMatch
              MatchId="urn:oasis:names:tc:xacml:1.0:function:string-equal">
            <AttributeValue DataType="http://www.w3.org/2001/XMLSchema#string">action0</AttributeValue>
            <ActionAttributeDesignator AttributeId="urn:oasis:names:tc:xacml:1.0:action:action-id" DataType="http://www.w3.org/2001/XMLSchema#string"/>
          </ActionMatch>
        </Action>
      </Actions>
    </Target>
  <Rule RuleId="urn:oasis:names:tc:xacml:2.0:conformance-test:IIA1:rule" Effect="Permit">
    <Target>
      <Subjects>
        <Subject>
          <SubjectMatch MatchId="urn:oasis:names:tc:xacml:1.0:function:string-equal">
            <AttributeValue
                DataType="http://www.w3.org/2001/XMLSchema#string">subject0</AttributeValue>
            <SubjectAttributeDesignator AttributeId="urn:oasis:names:tc:xacml:1.0:subject:subject-id" DataType="http://www.w3.org/2001/XMLSchema#string"/>
          </SubjectMatch>
        </Subject>
      </Subjects>
      <Resources>
        <Resource>
          <ResourceMatch MatchId="urn:oasis:names:tc:xacml:1.0:function:string-equal">
            <AttributeValue DataType="http://www.w3.org/2001/XMLSchema#string">recource0</AttributeValue>
            <ResourceAttributeDesignator AttributeId="urn:oasis:names:tc:xacml:1.0:resource:resource-id" DataType="http://www.w3.org/2001/XMLSchema#string"/>
          </ResourceMatch>
        </Resource>
      </Resources>
      <Actions>
        <Action>
          <ActionMatch
              MatchId="urn:oasis:names:tc:xacml:1.0:function:string-equal">
            <AttributeValue DataType="http://www.w3.org/2001/XMLSchema#string">action0</AttributeValue>
            <ActionAttributeDesignator AttributeId="urn:oasis:names:tc:xacml:1.0:action:action-id" DataType="http://www.w3.org/2001/XMLSchema#string"/>
          </ActionMatch>
        </Action>
      </Actions>
    </Target>
  </Rule>
</Policy>

    Date: Apr 10, 2017
    Created By: Mehdi Karimi

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
static void char_data (void *policyData, const XML_Char *s, int len);
static void XMLCALL policyXacml_fnPolicyStartElementCallback(void *policyData,
                          const char *element,
                          const char **attribute);
static void XMLCALL policyXacml_fnPolicyEndElementCallback( void *policyData,
                                              const char *element);
static int policyXacml_fnDateString2Tm( char* dateStr, struct tm *date );
static int policyXacml_fnTimeTokenizer( char* timeStr, struct tm *date );
/*==============================================================================
                           Local/Private Variables
==============================================================================*/
tzPolicyData policyData;

static bool subject;
static bool resource;
static bool action;

/*==============================================================================
                           Local/Private Constants
==============================================================================*/

/*==============================================================================
                           Function Definitions
==============================================================================*/


/*============================================================================*/
//fn  PARSEXACML_fnPolicyCreate
/*!

@brief
    Parse Policy file

    This function opens the specified policy descriptor file,
    parses the contents, and creates the data points.

@param[in]
    hDPRM
        MiniCloud Resource Manager Handle (handle created by DP_fnOpen() and
        is being passed to this function)

@param[in]
    filename
        pointer to the dynamic policy filename

@return
    EOK - parse were created OK
    EINVAL - invalid argument specified
    ENOENT - input file could not be opened
    EIO - XML parsing error

*/
/*============================================================================*/
int PARSEXACML_fnPolicyCreate( DP_HANDLE hDPRM, char *filename)
{
    char buf[BUFSIZ];
    int done = 0;
    FILE *fp;
    XML_Parser parse;

    subject = false;
    resource = false;
    action = false;

    /* open the minicloud resource manager */
    if( hDPRM == NULL )
    {
        fprintf(stderr, "Unable to open minicloud Resource Manager\n");
        return EINVAL;
    }

    /* populate the policyData structure */
    memset(&policyData, 0, sizeof(policyData) );
    policyData.hDPRM = hDPRM;

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
    XML_SetUserData(parse, &policyData);

    /* set up the start and end element callback handlers */
    XML_SetElementHandler( parse,
                           policyXacml_fnPolicyStartElementCallback,
                           policyXacml_fnPolicyEndElementCallback);

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
/*!

/fn  policyXacml_fnPolicyStartElementCallback

@brief
    XML Callback handler invoked when by reading and XML file
    a start element tag is encountered
    by the XML parse.

    The start_element function is invoked by the XML parse when an XML element
    start tag is encountered.  It initialises the state of the policy
    processor and stores the element tag type.

@param[in]
    policyData
        pointer to the policyData set by the XML_SetUserData() function.

@param[in]
    element
        pointer to the name of the XML element which is about to be processed

@param[in]
    attribute
        array of XML attribute pointers: in here, used for min and max
        assignment of the 'comparator rule':
        <rule min = "25" max = "60">comparator</rule>

*/
/*============================================================================*/
static void XMLCALL policyXacml_fnPolicyStartElementCallback(void *policyData,
                          const char *name,
                          const char **atts)
{
    int i = 0;

    tzPolicyData *ptzPolicyData = policyData;
    if( ptzPolicyData == NULL )
    {
        return;
    }

    ptzPolicyData->start_offset = ptzPolicyData->offset;

    if( stricmp( name, "policy" ) == 0 )
    {
        /* setup to begin parsing a policy block */
        ptzPolicyData->offset = 0;
        ptzPolicyData->start_offset = 0;

        /* clear the character data buffer */
        memset( ptzPolicyData->char_data_policy_buffer,
                0,
                sizeof( ptzPolicyData->char_data_policy_buffer ) );

        /* clear the entire policy data structure */
        memset( &ptzPolicyData->policy,
                0,
                sizeof( struct zPOLICY ) );
    }
    else if( strcasecmp(name, "rule") == 0 )
    {
        while( atts[i] != NULL )
        {
            if((strcasecmp( atts[ i ], "Effect" ) == 0))
            {
                if( stricmp(atts[i+1], "permit" ) == 0 )
                {
                    ptzPolicyData->policy.Name = POLICY_NAME_ACCESS;
                }
                else
                {
                    ptzPolicyData->policy.Name = POLICY_NAME_INVALID;
                }
            }
            i+=2;
        }
    }
    else if( strcasecmp(name, "subject") == 0 )
    {
        subject  = true;
        resource = false;
        action   = false;
    }
    else if( strcasecmp(name, "resource") == 0 )
    {
        subject  = false;
        resource = true;
        action   = false;
    }
    else if( strcasecmp(name, "action") == 0 )
    {
        subject  = false;
        resource = false;
        action   = true;
    }
}

/*============================================================================*/
/*!

/fn  policyXacml_fnPolicyEndElementCallback

@brief
    XML Callback handler invoked when an end element tag is encountered
    by the XML parse.

    The end_element function is invoked by the XML parse when an XML element
    end tag is encountered.  It populates the data point creation
    structure based on the content of the XML element character data.

@param[in]
    policyData
        pointer to the policyData set by the XML_SetUserData() function.

@param[in]
    element
        pointer to the name of the XML element which has been processed

*/
/*============================================================================*/
static void XMLCALL policyXacml_fnPolicyEndElementCallback( void *policyData,
                                              const char *element)
{
    tzPolicyData *ptzPolicyData = policyData;
    char *pElementData;

    if( ptzPolicyData == NULL )
    {
        return;
    }

    pElementData =
        &ptzPolicyData->char_data_policy_buffer[ptzPolicyData->start_offset];

    /* null terminate the character data */
    char_data(ptzPolicyData, "\0", 1);
    if( ptzPolicyData->overflow == true )
    {
        fprintf(stderr, "data overflow processing element: %s\n", element );
        /* we have a data overflow on our input buffer - we cannot
         * do any further processing */
        return;
    }

    if( stricmp(element, "rule") == 0 )
    {
        if( stricmp(pElementData, "comparator" ) == 0 )
        {
            ptzPolicyData->policy.Name = POLICY_NAME_COMP;
        }
        else if(stricmp(pElementData, "access" ) == 0)
        {
            ptzPolicyData->policy.Name = POLICY_NAME_ACCESS;
        }
        else
        {
            ptzPolicyData->policy.Name = POLICY_NAME_INVALID;
        }
    }
    else if( stricmp(element, "AttributeValue") == 0 )
    {
        if( subject )
        {
            if( stricmp(pElementData, "temperature" ) == 0 )
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_TEMP;
            }
            else if(stricmp(pElementData, "voltage" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_VOLT;
            }
            else if(stricmp(pElementData, "current" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_CURR;
            }
            else if(stricmp(pElementData, "frequency" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_FREQ;
            }
            else if(stricmp(pElementData, "power" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_POWER;
            }
            else if(stricmp(pElementData, "password" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_PASS;
            }
            else if(stricmp(pElementData, "heading" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_HEAD;
            }
            else if(stricmp(pElementData, "positionX" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_POSX;
            }
            else if(stricmp(pElementData, "positionY" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_POSY;
            }
            else if(stricmp(pElementData, "fuelLevel" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_FUEL;
            }
            else if(stricmp(pElementData, "altitude" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_ALT;
            }
            else if(stricmp(pElementData, "speed" ) == 0)
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_SPEED;
            }
            else
            {
                ptzPolicyData->policy.Type = POLICY_TYPE_INVALID;
            }

            subject = false;

        }
        else if( resource )
        {
            strcpy(ptzPolicyData->policy.Location, strlwr( pElementData) );

            resource = false;
        }
        else if( action )
        {
            action   = false;
        }
    }
    else if( stricmp(element, "time") == 0 )
    {
        /* convert time string to timespec only if not null and not empty("") */
        if( (NULL != pElementData) && ( 0 != strcmp(EMPTY, pElementData) ) )
        {
            strncpy( ptzPolicyData->timeString,
                     pElementData,
                     strlen(pElementData)+1 );

            if( EOK != policyXacml_fnDateString2Tm( ptzPolicyData->timeString,
                                              &ptzPolicyData->tm_time ) )
            {
                printf("cannot tokenize the date");
            }
            time_t t_t = mktime( &ptzPolicyData->tm_time );
            ptzPolicyData->policy.time.tv_sec = t_t;
        }

        memset(ptzPolicyData->timeString, 0, TIME_STR_LENGTH);
        memset(&ptzPolicyData->tm_time, 0, sizeof(struct tm));

    }
    else if( stricmp(element, "user") == 0 )
    {
        if( 0 == stricmp(pElementData, "gus") )
        {
            ptzPolicyData->policy.user = USER_CODE_GUS;
        }
        else if( 0 == stricmp(pElementData, "doug") )
        {
            ptzPolicyData->policy.user = USER_CODE_DOUG;
        }
        else if( 0 == stricmp(pElementData, "mike") )
        {
            ptzPolicyData->policy.user = USER_CODE_MIKE;
        }
        else if( 0 == stricmp(pElementData, "tom") )
        {
            ptzPolicyData->policy.user = USER_CODE_TOM;
        }
        else if( 0 == stricmp(pElementData, "jackie") )
        {
            ptzPolicyData->policy.user = USER_CODE_JACKIE;
        }
        else if( 0 == stricmp(pElementData, "lilli") )
        {
            ptzPolicyData->policy.user = USER_CODE_LILLI;
        }
        else if( 0 == stricmp(pElementData, "bob") )
        {
            ptzPolicyData->policy.user = USER_CODE_BOB;
        }
        else if( 0 == stricmp(pElementData, "madi") )
        {
            ptzPolicyData->policy.user = USER_CODE_MADI;
        }
        else
        {
            ptzPolicyData->policy.user = USER_CODE_INVALID;
        }
    }
    else if( stricmp(element, "group") == 0 )
    {
        if( 0 == stricmp(pElementData, "manager") )
        {
            ptzPolicyData->policy.group = GROUP_CODE_MANAGER;
        }
        else if( 0 == stricmp(pElementData, "engineering") )
        {
            ptzPolicyData->policy.group = GROUP_CODE_ENG;
        }
        else if( 0 == stricmp(pElementData, "technician") )
        {
            ptzPolicyData->policy.group = GROUP_CODE_TECH;
        }
        else if( 0 == stricmp(pElementData, "customer") )
        {
            ptzPolicyData->policy.group = GROUP_CODE_CUSTOMER;
        }
        else
        {
            ptzPolicyData->policy.group = GROUP_CODE_INVALID;
        }
    }
    /* we do or do not want to register for now todo */
    else if( stricmp(element, "policy") == 0 )
    {
        /* create the policy */
        int res = DP_fnRegisterPolicy( ptzPolicyData->hDPRM,
                                       &ptzPolicyData->policy );
        if( res != EOK )
        {
            syslog( LOG_ERR, "Failed to create DP_fnRegisterPolicy" );
            fprintf(stderr,"Failed to create DP_fnRegisterPolicy"
                    "#%d\n",
                    ptzPolicyData->policy.Name );
        }

        /* done with a ruleset, reset the policy content for the next item
         * the reseting below has been done at the start of each policy rule
         * in the start element and here is redundant, remove it in future */
        memset( &ptzPolicyData->policy, 0, sizeof(struct zPOLICY ) );

    }
    else
    {
        /* We do not recognise this tag.
         * Reset the offset to where it was before we started
         * processing this tag */
        ptzPolicyData->offset = ptzPolicyData->start_offset;
    }
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
    policyData
        pointer to the policyData set by the XML_SetUserData() function.

@param[in]
    s
        pointer to the start of the XML character data for the current XML tag

@param[in]
    len
        length of the XML character data to be processed

*/
/*============================================================================*/
static void char_data (void *policyData, const XML_Char *s, int len)
{
    tzPolicyData *ptzPolicyData = policyData;

    if( policyData != NULL && s != NULL )
    {
        if (!ptzPolicyData->overflow)
        {
            if (len + ptzPolicyData->offset >=
                sizeof(ptzPolicyData->char_data_policy_buffer) )
            {
                ptzPolicyData->overflow = true;
            }
            else
            {
                memcpy( ptzPolicyData->char_data_policy_buffer +
                        ptzPolicyData->offset,
                        s,
                        len );
                ptzPolicyData->offset += len;
            }
        }
    }
}

/*============================================================================*/
/*!
    get date as string and return each date item as in tm structure

    get date in "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DDTHH:MM:SS" format
    tokenize and return each item as in tm structure.

@param[in]
    dateStr
     "<YYYY-MM-DD>T<HH:MM:SS>" format
     or
     "<YYYY-MM-DD> <HH:MM:SS>" format

@param[out]
    date
        The extracted values from dateStr in "struct tm" format.

@retval EOK    - if conversion successful
@retval EINVAL - If null pointer detected

*/
/*============================================================================*/
static int policyXacml_fnDateString2Tm( char* dateStr, struct tm *date )
{
    time_t      timenow     = 0;
    struct tm * tm_timenow  = NULL;
    /* should be year since 1900 (from 0); will subtract (2016 - 1900 )*/
    char YYYY[ DATE_STRING_LEN ]        = "1900";
    char MM[ DATE_STRING_LEN ]          = "00"; //  month of the year (from 0)
    char DD[ DATE_STRING_LEN ]          = "01"; // day of the month (from 1)
    char pYYYYMMDD[ DATE_STRING_LEN ]   = "0";
    char pHHMMSSmmm[ DATE_STRING_LEN ]  = "0";
    char *token         = NULL;
    char dateStrCopy[TIME_STR_LENGTH];
    int notime          = 0;
    int retval          = EOK;

    timenow = time(NULL);
    tm_timenow = localtime(&timenow);

    memset(dateStrCopy, 0, sizeof(dateStrCopy));

    if( ( NULL == dateStr ) || ( NULL == date ) )
    {
        retval = EINVAL;
    }
    else
    {
        /* strtok function modifies its input string,
         * make a working copy to play with*/
        strncpy(dateStrCopy, dateStr, strlen(dateStr)+1 );

        /* get first token, it is yyyy-mm-dd */
        token = strtok( dateStrCopy, TIMETAGSPACE );
        if( !token )
        {
            /* todo March 22 2017 - Mehdi - change this to 1970 from beginning
             *  of time */
            memcpy(date, tm_timenow, sizeof(struct tm) );
        }
        else
        {
            strcpy( pYYYYMMDD, token );

            /* get second token, it is Time. */
            token = strtok( NULL, TIMETAGSPACE );
            if( token )
            {
                strcpy( pHHMMSSmmm, token );
            }
            else
            {
                notime = 1;
            }

            /* get YYYY */
            token = strtok( pYYYYMMDD, DASHSLASH );
            if( !token )
            {
                retval = EINVAL;
            }
            else
            {
                strcpy( YYYY, token );

                /* get second token, it is Month. */
                token = strtok( NULL, DASHSLASH );
                if( !token )
                {
                    retval = EINVAL;
                }
                else
                {
                    strcpy( MM, token );

                    /* get Day token. */
                    token = strtok( NULL, DASHSLASH );
                    /* check Month token is not null */
                    if( !token )
                    {
                        retval = EINVAL;
                    }
                    else
                    {
                        strcpy( DD, token );

                        /* pre-populate the date first for the sake
                         * of offset and timezone */
                        memcpy( date, tm_timenow, sizeof( struct tm ) );

                        /* change the year month and day based on
                         * the incoming requested time */
                        date->tm_year = atoi( YYYY ) - 1900;
                        // years since 1900 (from 0)
                        date->tm_mon  = atoi( MM ) - 1;
                        // months since January -- [0,11]
                        date->tm_mday = atoi( DD );

                        if( notime )
                        {
                            date->tm_hour = 00;
                            date->tm_min = 00;
                            date->tm_sec = 00;
                        }
                        else
                        {
                            retval = policyXacml_fnTimeTokenizer (pHHMMSSmmm, date);
                        }
                    }
                }
            }
        }
    }
    return retval;
}

/*============================================================================*/
/*!

    get time as string and return each time item as in tm structure

    get time in "HH:MM:SS" format and
    tokenize and return each item as in tm structure.

@param[in]
    timeStr
     "HH:MM:SS" format. This string is modified by the function

@param[in]
    startFlag
        if considerations to be made for tokenizing a start date search

@param[in]
    endFlag
        if considerations to be made for tokenizing an end date search

@param[out]
    date
        The extracted values from dateStr in "struct tm" format.

@retval EOK    - if conversion successful
@retval EINVAL - If null pointer detected

*/
/*============================================================================*/
static int policyXacml_fnTimeTokenizer( char* timeStr, struct tm *date )
{
    char HH[ DATE_STRING_LEN ]    = "0"; // hour of the day (from 0)
    char mm[ DATE_STRING_LEN ]    = "0"; // minutes after the hour (from 0)
    char SSmmm[ DATE_STRING_LEN ] = "0";
    char SS[ DATE_STRING_LEN ]    = "0"; // seconds after the minute (from 0)
    char nullSSmmm[ DATE_STRING_LEN ] = "00.000";
    char nullmm[ DATE_STRING_LEN ]    = "00";
    char nullHH[ DATE_STRING_LEN ]    = "00";
    char nullSS[ DATE_STRING_LEN ]    = "00";
    char *token           = NULL;
    int  retval           = EOK;

    if( ( NULL == timeStr ) || ( NULL == date ) )
    {
        retval = EINVAL;
    }
    else
    {
        /* get HH */
        token = strtok( timeStr, COLON );
        if( token )
        {
            strcpy( HH, token );
        }
        else
        {
            strcpy( HH, nullHH );
        }

        /* minutes token */
        token = strtok( NULL, COLON );
        if( token )
        {
            strcpy( mm, token );

        }
        else
        {
            strcpy( mm, nullmm );
        }

        /* seconds token */
        token = strtok( NULL, COLON );
        if( token )
        {
            strcpy( SSmmm, token );
        }
        else
        {
            strcpy( SSmmm, nullSSmmm );
        }

        /* get SS */
        token = strtok( SSmmm, DOT );
        if( token )
        {
            strcpy( SS, token );
        }
        else
        {
            strcpy( SS, nullSS );
        }

        date->tm_hour  = atoi( HH );
        date->tm_min   = atoi( mm );
        date->tm_sec   = atoi( SS );
        date->tm_wday  = -1;
        date->tm_yday  = -1;
    }

    return retval;
}

//EoF
