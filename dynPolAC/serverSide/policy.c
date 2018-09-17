/*=============================================================================

University of British Columbia (UBC)
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
Mini Cloud Server Project.

==============================================================================*/

/*!
 * @addtogroup dp
 * @{
 */

/*============================================================================*/
/*!

 @file  policy.c

 @brief
    Data Point Manipulation functions

    This module provides functions to manipulate a data point's data.
    Functions allow setting and getting data point values by name or GUID,
    or handle.

    Functions are provided to render data points to strings.

    Functions are provided to query data point information.

    Functions are provided to manipulate data point timestamps and quality.

 */

/*==============================================================================
 	 	 	 	 	 	 	 	Includes
 =============================================================================*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/neutrino.h>
#include <sys/trace.h>
#include "hash.h"
#include "policy.h"
#include "tags.h"

/*==============================================================================
 	 	 	 	 	 	 	 	 Defines
 =============================================================================*/
/*! maximum static length of the tag string assumed in the dp data structure */
#define MAX_TAG_STRING_LENGTH        ( 128 )
/*! maximum static length of the hashString construction */
#define MAX_HASH_STRING_LENGTH       ( 1024 )

/*=============================================================================
 	 	 	 	 	 	 	 	 Structures
 =============================================================================*/

/*==============================================================================
 	 	 	 	 	 	 External/Public Variables
 =============================================================================*/

/*! data point tags */
char *tagMap[DP_SERVER_MAX_TAGS+1];

/*==============================================================================
 	 	 	 	 	 	 Local/Private Variables
 =============================================================================*/

/*==============================================================================
 	 	 	 	 	 Local/Private Function Prototypes
==============================================================================*/
//static char* policy_fnTypeVal2String( int Type );
static int policy_fnTypeString2Val( char* Type );
static bool policy_fnBoundChecking( struct dp_t *pDp,
		                            struct policy_id_t* pPolicy );
static int policy_fnCheckVal( struct dp_t *pDp,
		                      struct policy_id_t* pPolicy);
static bool policy_fnCheckLoc( char* location,
                               struct policy_id_t* pPolicy );
static bool policy_fnCheckUser( uint32_t user, struct policy_id_t* pPolicy );
static bool policy_fnCheckGroup( uint32_t group, struct policy_id_t* pPolicy );
static int policy_fnCheckAttr( char* location,
		                       uint32_t userID,
		                       uint32_t groupID,
		                       struct policy_id_t* pPolicy );
static int policy_fnTokenizeTags( struct dp_t* pDp,
		                          int*         typeInt,
		                          char*        location,
		                          uint32_t*    userID,
		                          uint32_t*    groupID  );

/*==============================================================================
 	 	 	 	 	 	 Function Definitions
 ==============================================================================*/

/*============================================================================*/
//fn  POLICY_fnCreatePolicy
/*!

	Create the policy data based in MiniCloud server

	location is not the vendor name like AirMap, Google, Intel, etc.

@param[in]
    rcvid
        receive identifier used for message replies

@param[in]
    msg
        pointer to the datapoint_policy_msg_t message type

@return
    EOK on success, any other standard error code on failure

*/
/*============================================================================*/
int POLICY_fnCreatePolicy( int rcvid, datapoint_policy_msg_t *msg )
{
	struct policy_id_t* newPolicy = NULL;
    char* pLocation = NULL;
    char  hashString[ MAX_HASH_STRING_LENGTH ];
    tzHouseKeep* housekeeper = NULL;
    void* found;
    int i = 0;
    int ret = EOK;

    memset( hashString, 0, sizeof(hashString) );

    newPolicy = calloc(1, sizeof( struct policy_id_t ));
    if( newPolicy == NULL )
    {
        ret = ENOMEM;
    }
    else if( msg == NULL )
    {
        ret = EINVAL;
    }
    else
    {
    	pLocation = (char *)msg + sizeof(datapoint_policy_msg_t);

		newPolicy->policy.Name        = msg->Name;
		newPolicy->policy.Type        = msg->Type;
		newPolicy->policy.max         = msg->max;
		newPolicy->policy.min         = msg->min;
		newPolicy->policy.time.tv_sec = msg->time.tv_sec;
		newPolicy->policy.user        = msg->user;
		newPolicy->policy.group       = msg->group;

		/* check if timepec is valid */
		if( 0 != msg->time.tv_sec )
		{
			memcpy( &newPolicy->policy.time,
					&msg->time,
					sizeof(struct timespec) );
		}

		strcpy(newPolicy->policy.Location, pLocation);

		/* the order is name, type, location */
		sprintf( hashString,
				 "%d%d%s",
				 newPolicy->policy.Name,
				 newPolicy->policy.Type,
				 strlwr(newPolicy->policy.Location) );

		/* update the hash  */
		found = POLICYHASH_fnPut( newPolicy, hashString );
		if( (int*)(-1) == found )
		{
			ret = EINVAL;
		}
		else
		{
			housekeeper = POLICYHASH_fnHouseKeepAccessor(  );
			if( NULL == housekeeper )
			{
				ret = EINVAL;
			}
			else
			{
				/* access the housekeeper for policy, policy is new */
				if( (void*)NULL == (void*)found )
				{
					/* find an emery spot in the array and place */
					for( i=0; i< ESTIMATED_NUM_POLICY; i++)
					{
						if( NULL == housekeeper[i].pPolicy )
						{
							housekeeper[i].pPolicy = newPolicy;
							housekeeper[i].Seen = true;
							break;
						}
					}
				}
				else
				{
					/* not new so mark is as seen in this iteration */
					/* find an emery spot in the array and place */
					for( i=0; i< ESTIMATED_NUM_POLICY; i++)
					{
						if( found == housekeeper[i].pPolicy )
						{
							housekeeper[i].Seen = true;
							housekeeper[i].pPolicy = newPolicy;
							break;
						}
					}
				}
			}
		}
    }

    return ret;
}

/*============================================================================*/
/*!
	remove the rules that has not been visited since the current time that
	the new policy enforcement has happened.

@param[in]
    rcvid
        receive identifier used for message replies

@param[in]
    msg
        pointer to the datapoint_policy_msg_t message type

@return
    EOK on success, any other standard error code on failure

*/
/*============================================================================*/
int POLICY_fnHouseKeepPolicy( int rcvid, datapoint_policy_msg_t *msg )
{
	char  hashString[ MAX_HASH_STRING_LENGTH ];
	int ret = EOK;
    tzHouseKeep* housekeeper = NULL;
    int i = 0;

    memset( hashString, 0, sizeof(hashString) );

	housekeeper = POLICYHASH_fnHouseKeepAccessor(  );
	if( NULL == housekeeper )
	{
		ret = EINVAL;
	}
	else
	{
		for( i=0; i< ESTIMATED_NUM_POLICY; i++)
		{
			if( ( 0 != housekeeper[i].pPolicy ) &&
			    ( NULL != housekeeper[i].pPolicy ) )
			{
				/* remove all that last time not visited */
				if( false == housekeeper[i].Seen )
				{
					/* the order is name, type, location */
					sprintf( hashString,
							 "%d%d%s",
							 housekeeper[i].pPolicy->policy.Name,
							 housekeeper[i].pPolicy->policy.Type,
							 strlwr(housekeeper[i].pPolicy->policy.Location) );
					POLICYHASH_fnRemove( hashString );
					housekeeper[i].pPolicy = NULL;
					memset( hashString, 0, sizeof(hashString) );
				}
			}
		}

		/* done with removal housekeeping,
		 * now turn all flags false for next time */
		for( i=0; i < ESTIMATED_NUM_POLICY; i++ )
		{
			if( ( 0    != housekeeper[i].pPolicy ) &&
				( NULL != housekeeper[i].pPolicy ) )
			{
				housekeeper[i].Seen = false; //reset them all to false
			}
		}
	}

	return ret;
}

/*============================================================================*/
/*!

    Return the enumerated value of the string type
    string types are temperature, voltage, current, etc.

@return
    value of the string type

*/
/*============================================================================*/
static int policy_fnTypeString2Val( char* Type )
{
	int type = -1;

	if( stricmp(Type, "temperature" ) == 0 )
	{
		type = POLICY_TYPE_TEMP;
	}
	else if(stricmp(Type, "voltage" ) == 0)
	{
		type = POLICY_TYPE_VOLT;
	}
	else if(stricmp(Type, "current" ) == 0)
	{
		type = POLICY_TYPE_CURR;
	}
	else if(stricmp(Type, "frequency" ) == 0)
	{
		type = POLICY_TYPE_FREQ;
	}
	else if(stricmp(Type, "power" ) == 0)
	{
		type = POLICY_TYPE_POWER;
	}
	else if(stricmp(Type, "password" ) == 0)
	{
		type = POLICY_TYPE_PASS;
	}
	else if(stricmp(Type, "heading" ) == 0)
	{
		type = POLICY_TYPE_HEAD;
	}
	else if(stricmp(Type, "fuelLevel" ) == 0)
	{
		type = POLICY_TYPE_FUEL;
	}
	else if(stricmp(Type, "positionX" ) == 0)
	{
		type = POLICY_TYPE_POSX;
	}
	else if(stricmp(Type, "positionY" ) == 0)
	{
		type = POLICY_TYPE_POSY;
	}
	else if(stricmp(Type, "altitude" ) == 0)
	{
		type = POLICY_TYPE_ALT;
	}
	else if(stricmp(Type, "speed" ) == 0)
	{
		type = POLICY_TYPE_SPEED;
	}
	else
	{
		type = POLICY_TYPE_INVALID;
	}

	return type;
}

/*============================================================================*/
/*!
    Check if the data point can be delivered or not

    This function based on the existing policies checks if the query can pass
    or not.

@param[in]
    pDp
        data point structure

@retval EOK - if the policy check passed
@retval EACCES - if the policy did not pass

*/
/*============================================================================*/
bool POLICY_fnCheck( struct dp_t *pDp )
{

	int ret = EACCES;
	int typeInt = -1;
    char hashString[ MAX_HASH_STRING_LENGTH ] = "\0";
	char location[ MAX_LOCATION_STRING_LENGTH ]= "\0";
    struct policy_id_t* pPolicy = NULL;
    uint32_t userID = 0u;
    uint32_t groupID = 0u;
    struct timespec dpTime;

	/* get the last updated time of the dp */
	dpTime.tv_sec = pDp->dpdata.timestamp.tv_sec;

	if( EOK != policy_fnTokenizeTags( pDp,
			                          &typeInt,
			                          location,
			                          &userID,
			                          &groupID  ) )
	{
		printf( "POLICY_fnCheck:"
				"cannot tokenize, something is wrong in the dp database\n");
		printf( "POLICY_fnCheck:"
				"Blocking the data access, DoS?!\n");
	}
	else
	{
		/* based on the category type we decided if the data point must be
		 * checked against the comparator rule or the accessor rule */
		switch(typeInt)
		{
		case POLICY_TYPE_INVALID:
			/* if the policy type is unknown from the dp bank it means that
			 * there is no policy for it yet therefore assume wild card ie.
			 * pass it*/
			ret = EOK;
			break;
		case POLICY_TYPE_PASS:
		case POLICY_TYPE_HEAD:
		case POLICY_TYPE_FUEL:
			/* the order is name, type, location */
			sprintf( hashString,
					 "%d%d%s",
					 POLICY_NAME_ACCESS,
					 typeInt,
					 strlwr(location) );
			if( POLICYHASH_fnCheck( hashString ) )
			{
				pPolicy = POLICYHASH_fnFind( hashString );

				/* check if the time is specified o/w assume wildcard for time*/
				if( 0 != pPolicy->policy.time.tv_sec )
				{
					/* check if the time is bigger than policy time (since) */
					if( pPolicy->policy.time.tv_sec <= dpTime.tv_sec )
					{
						ret = policy_fnCheckAttr( location,
												  userID,
												  groupID,
												  pPolicy );
					}
				}
				else
				{
					ret = policy_fnCheckAttr( location,
											  userID,
											  groupID,
											  pPolicy );
				}
			}
			break;
		default:
			/* we assume all other than password are default type which is the
			 * comparator rule check */
			/* the order is name, type, location */
			sprintf( hashString,
					 "%d%d%s",
					 POLICY_NAME_COMP,
					 typeInt,
					 strlwr(location) );
			if( POLICYHASH_fnCheck( hashString ) )
			{
				pPolicy = POLICYHASH_fnFind( hashString );

				/* check if the time is specified o/w assume wildcard for time*/
				if( 0 != pPolicy->policy.time.tv_sec )
				{
					if( pPolicy->policy.time.tv_sec <= dpTime.tv_sec )
					{
						if( EOK == policy_fnCheckVal( pDp, pPolicy ) )
						{
							ret = policy_fnCheckAttr( location,
													  userID,
													  groupID,
													  pPolicy );
						}
					}
				}
				else
				{
					if( EOK == policy_fnCheckVal( pDp, pPolicy ) )
					{
						ret = policy_fnCheckAttr( location,
												  userID,
												  groupID,
												  pPolicy );
					}
				}
			}
			break;
		}
	}

    return ret;
}

/*============================================================================*/
/*!

	tokenize data point tags and populate in their proper containers getting
	ready to compare against the policy checks

@param[in]
    pDp
        data point structure

@param[in]
    typeInt
        category type of the data point, taken from one of the data point tags
        type examples are temperature, voltage, password, current, poewr,
        frequency, etc. the type is translated to a macro definition integer
        value for quick comparison operations.

@param[in]
    location
        location of the data point to check against the policy file

@param[in]
    user
        user tag of the data point to check against the policy file

@param[in]
    group
        group id annotated to the data point tag


@retval - EOK if success in tokenizing or errno.h errors if failed.

*/
/*============================================================================*/
static int policy_fnTokenizeTags( struct dp_t* pDp,
		                          int*         typeInt,
		                          char*        location,
		                          uint32_t*    userID,
		                          uint32_t*    groupID  )
{
	int ret = EINVAL;
	int i = 0;
    char tagString[ MAX_TAG_STRING_LENGTH ] = "\0";
	char* user = NULL;
	char* group = NULL;
	char* token = NULL;
	char* type = NULL;

	/* check if the tag ID is within the valid range */
	if( ( pDp->dpdata.tags[i] < 1 ) ||
		( pDp->dpdata.tags[i] > DP_SERVER_MAX_TAGS ) )
	{
		ret = ENOENT;
	}
	else
	{
		/* tokenize data point tags */
		while( pDp->dpdata.tags[i] != NULL)
		{
			/* copy so that the strtok does not manipulate the pointer */
			strncpy( tagString,
					 tagMap[pDp->dpdata.tags[i]],
					 strlen(tagMap[pDp->dpdata.tags[i]]) + 1 );
			if( strstr( strlwr(tagMap[pDp->dpdata.tags[i]]), "type") )
			{
				strtok(tagString, COLON);
				type = strtok(NULL, COLON);
				*typeInt = policy_fnTypeString2Val( type );
			}
			else if( strstr( strlwr(tagMap[pDp->dpdata.tags[i]]), "location") )
			{
				strtok(tagString, COLON);
				token = strtok(NULL, COLON);
				strncpy( location, token, strlen(token)+1 );
			}
			else if( strstr( strlwr(tagMap[pDp->dpdata.tags[i]]), "user") )
			{
				strtok(tagString, COLON);
				user = strtok(NULL, COLON);

				/* TODO --
				 * July 17 2017: these are static and must change in future. */
				if( 0 == stricmp(user, "gus") )
				{
					*userID = USER_CODE_GUS;
				}
				else if( 0 == stricmp(user, "doug") )
				{
					*userID = USER_CODE_DOUG;
				}
				else if( 0 == stricmp(user, "mike") )
				{
					*userID = USER_CODE_MIKE;
				}
				else if( 0 == stricmp(user, "tom") )
				{
					*userID = USER_CODE_TOM;
				}
				else if( 0 == stricmp(user, "jackie") )
				{
					*userID = USER_CODE_JACKIE;
				}
				else if( 0 == stricmp(user, "lilli") )
				{
					*userID = USER_CODE_LILLI;
				}
				else if( 0 == stricmp(user, "bob") )
				{
					*userID = USER_CODE_BOB;
				}
				else if( 0 == stricmp(user, "madi") )
				{
					*userID = USER_CODE_MADI;
				}
				else
				{
					*userID = USER_CODE_INVALID;
				}

			}
			else if( strstr( strlwr(tagMap[pDp->dpdata.tags[i]]), "group") )
			{
				strtok(tagString, COLON);
				group = strtok(NULL, COLON);

				/* TODO --
				 * July 17 2017: these are static and must change in future. */
				if( 0 == stricmp(group, "manager") )
				{
					*groupID = GROUP_CODE_MANAGER;
				}
				else if( 0 == stricmp(group, "engineering") )
				{
					*groupID = GROUP_CODE_ENG;
				}
				else if( 0 == stricmp(group, "technician") )
				{
					*groupID = GROUP_CODE_TECH;
				}
				else if( 0 == stricmp(group, "customer") )
				{
					*groupID = GROUP_CODE_CUSTOMER;
				}
				else
				{
					*groupID = GROUP_CODE_INVALID;
				}
			}
			i++;
		}
	}

	ret = EOK;

	return ret;
}

/*============================================================================*/
/*!

	Check the location, user and group

@param[in]
    location
        location of the data point to check against the policy file

@param[in]
    user
        user tag of the data point to check against the policy file

@param[in]
    group
        group id annotated to the data point tag

@param[in]
    pPolicy
        policy data structure

@return
    EOK if passed the check, otherwise, EACCES indicating blocking access.

*/
/*============================================================================*/
static int policy_fnCheckAttr( char* location,
		                       uint32_t userID,
		                       uint32_t groupID,
		                       struct policy_id_t* pPolicy )
{
	int ret = EACCES;

	/* check the location is matching or wild card */
	if( policy_fnCheckLoc(location, pPolicy) )
	{
		if( policy_fnCheckUser(userID,pPolicy ) )
		{
			if( policy_fnCheckGroup(groupID,pPolicy ) )
			{
				ret = EOK; /* pass ok */
			}
		}
	}

	return ret;
}

/*============================================================================*/
/*!

@brief
    check if the group is matching with the policy file
    wild card for group is -1
    if wild card in the policy this check will pass true

@param[in]
    group
        group id annotated to the data point tag

@param[in]
    pPolicy
        policy data structure

@return
    true if the group is in the policy or wild card otherwise will be false.

*/
/*============================================================================*/
static bool policy_fnCheckGroup( uint32_t group, struct policy_id_t* pPolicy )
{
	bool ret = true;

	/* wild card for group is if the group is invalid meaning not specified in
	 * the policy file ruleset */
	if( GROUP_CODE_INVALID != pPolicy->policy.group )
	{
		if( group != pPolicy->policy.group )
		{
			ret = false;
		}
	}

	return ret;
}

/*============================================================================*/
/*!

@brief
    check user is matching with specified user policy

@param[in]
    user
        user id annotated to the data point tag

@param[in]
    pPolicy
        policy data structure

@return
    true if the user is in the policy or wild card otherwise will be false.

*/
/*============================================================================*/
static bool policy_fnCheckUser( uint32_t user, struct policy_id_t* pPolicy )
{
	bool ret = true;

	/* wild card is if the user is invalid in policy, meaning not specified */
	if( USER_CODE_INVALID != pPolicy->policy.user )
	{
		if( user != pPolicy->policy.user )
		{
			ret = false;
		}
	}

	return ret;
}

/*============================================================================*/
/*!

@brief
    check the location against the policy file
    if the location in the policy is not specified it means a wild card

@param[in]
    pDp
        data point structure

@param[in]
    pPolicy
        policy data structure

@return
    true if the location matching or if the location is wild card in the policy
    otherwise passes back false.

*/
/*============================================================================*/
static bool policy_fnCheckLoc( char* location,
                               struct policy_id_t* pPolicy )
{
	/* if the location in the policy is not specified it means a wild card*/
	bool ret = true;

	if( ( 0    != strcmp( EMPTY, pPolicy->policy.Location) ) &&
		( NULL != pPolicy->policy.Location ) )
	{
		if( 0 != stricmp( location, pPolicy->policy.Location ) )
		{
			ret = false;
		}
	}

	return ret;
}

/*============================================================================*/
/*!

@brief
    check if the data point value fits in the policy min max range

@param[in]
    pDp
        data point structure

@param[in]
    pPolicy
        policy data structure

@return
    true if the value is in the range otherwise false.

*/
/*============================================================================*/
static int policy_fnCheckVal( struct dp_t *pDp,
		                      struct policy_id_t* pPolicy )
{
	int ret = EACCES;

	if( true == policy_fnBoundChecking( pDp, pPolicy ) )
	{
		ret = EOK;
	}

	return ret;
}

/*============================================================================*/
/*!

@brief
    Check the min max bound

    Assumption: check the policy min/max are not set to wild card wild card for
    the policy min max are both zero
    this happens during the parsing of the policy

@param[in]
    pDp
        data point structure

@param[in]
    pPolicy
        policy data structure

@return
    true if the value is in the range otherwise false.

*/
/*============================================================================*/
static bool policy_fnBoundChecking( struct dp_t *pDp,
		                            struct policy_id_t* pPolicy )
{
	bool ret = true;

	/* check the policy min/max are not set to wild card
	 * wild card for the policy min max are both zero */
	if( ( 0 != pPolicy->policy.min ) && ( 0 != pPolicy->policy.max ) )
	{
		/* get a pointer to the data point data and determine its length */
		switch( pDp->dpdata.type )
		{
		/* this practice is for integer values only
		 * so factoring strings out */
		/*case DP_TYPE_STR:
			pVar = pDp->dpdata.val.pStr;
			len = pDp->dpdata.len;
			break;*/

		case DP_TYPE_UINT16:
			if( pDp->dpdata.val.uiVal >= pPolicy->policy.min &&
				pDp->dpdata.val.uiVal <= pPolicy->policy.max )
			{
				ret = true;
			}
			else
			{
				ret = false;
			}
			break;

		case DP_TYPE_SINT16:
			if( pDp->dpdata.val.siVal >= pPolicy->policy.min &&
				pDp->dpdata.val.siVal <= pPolicy->policy.max )
			{
				ret = true;
			}
			else
			{
				ret = false;
			}
			break;

		case DP_TYPE_UINT32:
			if( pDp->dpdata.val.ulVal >= pPolicy->policy.min &&
				pDp->dpdata.val.ulVal <= pPolicy->policy.max )
			{
				ret = true;
			}
			else
			{
				ret = false;
			}
			break;

		case DP_TYPE_SINT32:
			if( pDp->dpdata.val.slVal >= pPolicy->policy.min &&
				pDp->dpdata.val.slVal <= pPolicy->policy.max )
			{
				ret = true;
			}
			else
			{
				ret = false;
			}
			break;

		case DP_TYPE_FLOAT32:
			if( pDp->dpdata.val.fVal >= pPolicy->policy.min &&
				pDp->dpdata.val.fVal <= pPolicy->policy.max )
			{
				ret = true;
			}
			else
			{
				ret = false;
			}
			break;

		case DP_TYPE_ARRAY32:
		case DP_TYPE_ARRAY16:
		case DP_TYPE_CONJUGATE:
		default:
			ret = false;
			break;
		}
	}

	return ret;
}

/*! @} */
