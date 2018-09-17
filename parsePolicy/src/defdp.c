/*=============================================================================

University of British Columbia (UBC) 2016-2017
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
The Application for defining Data Points.

==============================================================================*/

/*============================================================================*/
/*!

@file  defdp.c

    Create data points from an XML file


    This command renders a data point template file which contains
    data point tags in the form: ${DP:<data point name>}
    to an output stream.  Any data point tag encountered is
    replaced in-line with its value.

    The rendering function also supports expansion of argument identifiers:
    $1 to $9 inclusive.

    $1 corresponds to the render command name as it was entered on the
       command line
    $2 corresponds to the template file name as it was entered on the
       command line
    $3 corresponds to the first argument following the template file
       name on the command line
    $4 corresponds to the second argument following the template file
       name on the command line.

    e.g

        For the following template file, template.txt:

        Date: $3
        Model: ${DP:FGA}
        Serial Number: ${DP:SERIALNUM}
        Hardware Rev: ${DP:BBB_PLATFORM}

        and assuming FGA="xyz-1234", SERIALNUM="1004-0002",
        and HWPLATFORM="Rev-A"

        Executing the following command:

            render template.txt "`date`"

        Yields the following:

            Date: Tue Feb 08 05:05:52 UTC 2019
            Model: xyz-1234
            Serial Number: 1004-0002
            Hardware Rev: Rev-A
*/

/*==============================================================================
                              Includes
==============================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>

#include "defdp.h"

/*==============================================================================
                              Local variables
==============================================================================*/
/*! virtual function pointer to populate custom-made policy parser */
typedef int (*policyFn)( DP_HANDLE hDPRM, char* filename );

/*! virtual function pointer to populate custom-made policy parser */
policyFn pPolicyFCN = NULL;



/*==============================================================================
                           Data Structures
==============================================================================*/
/*! the UserData structure is passed to the defdp_fnCallback function
 *  once for each data point that is created. */
typedef struct zdefdpUserData
{
    /*! handle to the data point manager */
    DP_HANDLE hDPRM;

    /*! pointer to variable attributes (comma separated list of data point
     * tags) */
    char *attributes;

} tzdefdpUserData;

/*==============================================================================
                           Function Declarations
==============================================================================*/
int main(int argc, char *argv[]);
void defdp_fnCallback( DP_tzINFO *ptzInfo,
                          uint32_t instanceID,
                          void *userData );

/*==============================================================================
                           Function Definitions
==============================================================================*/

/*============================================================================*/
/*!
    Entry point for the defdp command

    The defdp module allows dynamic creation of data points from
    an XML file.  The XML file takes the following form:

    - Updated to support data point tags/attributes.
    - Added support to globally suppress the data point GUID assignment
      using the -G flag.

    <?xml version="1.0" encoding="utf-8" ?>
    <defdp>
        <point>
            <id>8010001A</id>
            <name>ROOM</name>
            <type>str</type>
            <format>%s</format>
            <length>50</length>
            <value>Workout room in Boston</value>
            <tag>data:info</tag>
        </point>

        ...

    </defdp>

    The data point value refers to an absolute path on the file system.
    The XML parser supports embedded variable tags.
    eg
    <value>${HOME}/room.txt</value>


@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        argv[0] is the name of the process
        argv[argc-1] is the name of the data point definition file

@return
    EXIT_FAILURE - indicates the data points could not be created
    EXIT_SUCCESS - indicates the specified variables were created

*/
/*============================================================================*/
int main(int argc, char *argv[])
{
    int flags=0;
    uint32_t instanceID = 0;
    int c;
    int errflag = 0;
    char* dpFile = (char*) NULL;
    char* policyFile = (char*) NULL;
    tzdefdpUserData userData;
    uint32_t options = PARSE_OPT_NONE;
    bool verbose = false;

    /* timing characterisation for policy time measurement before and after */
	uint64_t cps = 0;
	uint64_t cycle1 = 0;
	uint64_t cycle2 = 0;
	uint64_t ncycles = 0;
	double sec = 0;

    if( argc < 2)
    {
        fprintf(stderr,
                "usage: %s "
                "[-a <attributes>] "
                "[-f <dpfilename> for example /etc/bigfile.xml] "
                "[-i <instance ID>] "
                "[-p <policy_filepath> for example /etc/policy_file.xml] "
                "[-G] "
                "<datapointfile>\n"
                "where flags may be one of:\n"
                "    h=hidden\n"
                "    r=read only\n"
                "    p=protected\n"
                "    v=volatile\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    /* Initialise the UserData structure */
    memset( &userData, 0, sizeof( userData ));

    /* parse the command line options */
    while( ( c = getopt( argc, argv, "p:P:a:i:f:Gv" ) ) != -1 )
    {
        switch( c )
        {
            case 'a':
                userData.attributes = optarg;
                break;

            case 'i':
                instanceID = atoi(optarg);
                break;

            case 'f':
            	dpFile = strdup(optarg);
                break;

//            case 'G':
//                options = PARSE_OPT_SUPPRESS_GUID;
//                break;

            /* XML parsing */
            case 'p':
            	policyFile = strdup(optarg);
            	pPolicyFCN = PARSE_fnPolicyCreate;
            	break;

            /* XACML parsing */
            case 'P':
            	policyFile = strdup(optarg);
            	pPolicyFCN = PARSEXACML_fnPolicyCreate;
            	break;

            case 'v':
            	verbose = true;
            	break;


            case '?':
                ++errflag;
                break;

            default:
                break;
        }
    }

    /* get a handle to the data point manager */
    userData.hDPRM = DP_fnOpen();
    if( userData.hDPRM == NULL )
    {
        return EXIT_FAILURE;
    }

    /* register data points with the minicloud server */
    if( (char*)NULL != dpFile )
    {
    	if( verbose )
    	{
    		printf("Registering data points in the MiniCloud Server->\n");
    	}
        PARSE_fnCreate( userData.hDPRM,
                          instanceID,
                          dpFile,
                          flags,
                          defdp_fnCallback,
                          &userData,
                          options );
        if( verbose )
        {
        	printf("Done DP registrations.\n");
        }
    }

    /* register policy rules with the minicloud server */
    if( (char*)NULL != policyFile )
	{
    	if( verbose )
    	{
    		printf("Enforce Policies->\n");
    	}

    	/* snap the time */
    	cycle1 = ClockCycles( );

    	/* virtual function pointer for when the XACML or XML is used */
    	pPolicyFCN( userData.hDPRM, policyFile );


    	/* snap the time again */
    	cycle2 = ClockCycles( );
    	ncycles = cycle2-cycle1;;
    	/* find out how many cycles per second */
    	cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;;
    	sec=(double)ncycles/cps;
    	if( verbose )
    	{
        	printf("Time to parse and register policy = %f ms.\n", sec*1000);
        	printf("Done policy enforcement.\n");
        	printf("policy housekeeping->\n");
    	}

		if( EOK != DP_fnPolicyHousekeeping( userData.hDPRM ) )
		{
			syslog( LOG_ERR, "Failed to housekeep policy." );
			fprintf(stderr,"Failed to housekeep policy\n" );
		}
    	if( verbose )
    	{
    		printf("Done policy housekeeping.\n");
    	}
	}

    /* close the data point manager */
    DP_fnClose( userData.hDPRM );

    return EXIT_SUCCESS;
}

/*============================================================================*/
/*!
    Callback function invoked for each variable created

    The defdp_fnCallback function is invoked once for each variable that is
    created when the defdp module is run. It handles applying data point
    attributes (or tags) to each data point.
    - Tags are comma separated list of 'Namespace:instance' format.
    - If a tag does not currently exist, it will be created.
    - The tag list is optional.  If the tag list is NULL, then no tags will be
      applied.

@param[in]
    ptzInfo
        pointer to the DP_tzINFO structure for the data point
        that has just been created.

@param[in]
    instanceID
        the instance identifier for the data point that has
        just been created.

@param[in]
    userData
        userData pointer which was supplied by the function
        which invoked the PARSE_fnCreate() function.
        It contains:
            - the data point manager handle,
            - a pointer to a list of data point tags to apply (optional)

*/
/*============================================================================*/
void defdp_fnCallback( DP_tzINFO *ptzInfo,
                          uint32_t instanceID,
                          void *userData )
{
    tzdefdpUserData *ptzUserData = (tzdefdpUserData *)userData;

    if( ptzUserData != NULL )
    {
        if( ptzUserData->attributes != NULL )
        {
            /* set data point tags/attributes */
            if( DP_fnSetTagsByName( ptzUserData->hDPRM,
                                    ptzInfo->pName,
                                    ptzUserData->attributes,
                                    0 ) != EOK )
            {
                syslog( LOG_ERR,
                        "cannot set tags for variable: %s",
                        ptzInfo->pName );
            }
        }
    }
}

//EoF
