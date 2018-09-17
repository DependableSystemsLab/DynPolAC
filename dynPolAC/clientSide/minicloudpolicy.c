/*=============================================================================

University of British Columbia (UBC) 2017
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
Mini Cloud Server Project.
Policy implementation
==============================================================================*/

/*!
 * @addtogroup datapoints
 * @{
 */

/*============================================================================*/

/*!
@file  policy.c

*/
/*============================================================================*/

/*==============================================================================
                              Includes
==============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/mman.h>
#include "minicloudmsg.h"

/*==============================================================================
                              Defines
==============================================================================*/


/*==============================================================================
                               Macros
==============================================================================*/

/*==============================================================================
                                Enums
==============================================================================*/

/*=============================================================================
                              Structures
==============================================================================*/

/*! internal structure maintained by the library to manage connections with
 * the Data Point Resource Manager (DPRM) */
typedef struct zDPRM
{
    /*! channel ID for the synchronisation signals from the Data Point
     * Resource Manager */
    int chid;

    /*! connection ID on which to receive synchronisation signals from the
     * Data Point Resource Manager */
    int coid;

    /*! API handle for the Data Point Resource Manager */
    int handle;

    /*! pointer to the client's shared memory buffer */
    char *pSharedMem;

    /*! length of the shared memory */
    size_t shmem_size;

    /* handle to the shared memory file descriptor */
    int shmem_fd;
} tzDPRM;

/*==============================================================================
                        Local/Private Function Prototypes
==============================================================================*/

/*==============================================================================
                        Function Definitions
==============================================================================*/

/*============================================================================*/
//fn  DP_fnRegister
/*!

    Add or modify the policy information

@param[in]
    dprm_handle
        Data Point Resource Manager Handle (returned by DP_fnOpen())

@param[in]
    pInfo
        pointer to the policy data structure

@return
    EOK : The data points were registered successfully
    any other value specifies an error code (see errno.h)

*/
/*============================================================================*/
int DP_fnRegisterPolicy( DPRM_HANDLE dprm_handle, tzPOLICY *pPolicy )
{
    tzDPRM *ptzDPRM = (tzDPRM *)dprm_handle;
    int ret = EINVAL;
    datapoint_policy_msg_t msg;

    /* these information are going to be sent to the server for policy
     * registration*/
    char pLocation[256] = "\0"; //  physical location of IoT variables

    int numIOV = 2;
    iov_t iov[numIOV];

    if( (NULL != ptzDPRM) && (NULL != pPolicy) )
    {
		/* clear the iovectors used for the reply */
		memset( iov, 0, sizeof(iov));

		/* Clear the memory for the msg and the reply */
		memset( &msg, 0, sizeof( msg ) );

		memset( pLocation, 0, sizeof(pLocation) );

		/* Set up the message code to send to the server */
		msg.code = MSG_DP_POLICY_REGISTER;

		/* rule name of the policy*/
		msg.Name = pPolicy->Name;
		/* populate the policy max value */
		msg.max = pPolicy->max;
		/* populate the policy min value */
		msg.min = pPolicy->min;
		/* populate the policy attributes */
		msg.Type = pPolicy->Type;
		/* populate the time in seconds, we only have resolution of time
		 * up to in seconds only */
		msg.time.tv_sec = pPolicy->time.tv_sec;
		/* populate the user code id */
		msg.user = pPolicy->user;
		/* populate the group code id */
		msg.group = pPolicy->group;

		/* strings are being sent separately since their length are dynamic */
		strncpy(pLocation, pPolicy->Location, strlen(pPolicy->Location) + 1 );

		/* Send the data to the server and get a reply */
		SETIOV (iov + 0, &msg, sizeof (msg));
		SETIOV (iov + 1, pLocation, strlen(pLocation)+1);

		ret = MsgSendv( ptzDPRM->handle, iov, numIOV, NULL, 0);
		if( ret == -1 )
		{
			if( errno != EEXIST )
			{
				fprintf( stderr,
						 "%s: %s\n",
						 __func__,
						 strerror( errno ) );
			}
			ret = errno;
		}
    }

    return ret;
}

/*============================================================================*/
/*!

    message to the server to ask to delete the policies that have been
    removed from the policy file.

@param[in]
    dprm_handle
        Data Point Resource Manager Handle (returned by DP_fnOpen())

@return
    EOK : The data points were registered successfully
    any other value specifies an error code (see errno.h)

*/
/*============================================================================*/
int DP_fnPolicyHousekeeping( DPRM_HANDLE dprm_handle )
{
    tzDPRM *ptzDPRM = (tzDPRM *)dprm_handle;
    int ret = EINVAL;
    datapoint_policy_msg_t msg;

    int numIOV = 1;
    iov_t iov[numIOV];

    if( NULL != ptzDPRM)
    {
		/* clear the iovectors used for the reply */
		memset( iov, 0, sizeof(iov));

		/* Clear the memory for the msg and the reply */
		memset( &msg, 0, sizeof( msg ) );

		/* Set up the message code to send to the server */
		msg.code = MSG_DP_POLICY_HOUSEKEEPING;

		/* Send the data to the server and get a reply */
		SETIOV (iov + 0, &msg, sizeof (msg));

		ret = MsgSendv( ptzDPRM->handle, iov, numIOV, NULL, 0);
		if( ret == -1 )
		{
			if( errno != EEXIST )
			{
				fprintf( stderr,
						 "%s: %s\n",
						 __func__,
						 strerror( errno ) );
			}
			ret = errno;
		}
    }

    return ret;
}

/*! @}
 * end of libdatapoints group */
