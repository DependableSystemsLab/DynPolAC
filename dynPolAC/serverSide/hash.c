/*=============================================================================

University of British Columbia (UBC)
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
Mini Cloud Server Project.

==============================================================================*/

/*!
 * @addtogroup hash
 * @{
 */

/*============================================================================*/
/*!

 @file  hash.c

 @brief
    Manage a data point hash table

 @details
    This module provides functions to manipulate data point hash
    table(s).  data points can be added to either a GUID hash
    table or a name hash table.  data points can be quickly retrieved
    from the hash tables by name or GUID.

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
#include "cfuhash.h"
#include "hash.h"
#include "dp.h"
#include "name.h"

/*==============================================================================
 	 	 	 	 	 	 	 	 	 Defines
==============================================================================*/

/*! length of the instance string appended to the data point name to
 *  generate a unique key for hashing */
#define KEY_INSTANCE_LENGTH       ( 8 )

/*! an estimate for the number of data points to be created */
#define ESTIMATED_NUM_DPS       ( 30000 )

/*=============================================================================
 	 	 	 	 	 	 	 	 Structures
 =============================================================================*/


/*==============================================================================
 	 	 	 	 	 	 	 Local/Private Variables
 =============================================================================*/

/*! hash table to store the data point Name Strings */
static cfuhash_table_t *hash = NULL;

/*! hash table to store the Globally Unique Identification Strings */
static cfuhash_table_t *guidhash = NULL;

/*! hash table to store the policy hash string */
static cfuhash_table_t *hashPolicy = NULL;

/*! List of policy rule house keeper */
static tzHouseKeep hs[ ESTIMATED_NUM_POLICY ];

/*==============================================================================
 Local/Private Function Prototypes
 =============================================================================*/

static int hash_fnBuildGUIDKey( uint32_t guid,
                                uint32_t instanceID,
                                char *key,
                                size_t key_len );

static int hash_fnBuildNameKey( char *name,
                                uint32_t instanceID,
                                char *key,
                                size_t key_len );

/*==============================================================================
 Function Definitions
 =============================================================================*/

/*============================================================================*/
/*!

    initialise the hash tables

    The HASH_fnSetup function initialises the name, GUID, and policy
    hash tables.

@return
    None

*/
/*============================================================================*/
void HASH_fnSetup(void)
{
    /* create the hash table */
    hash = cfuhash_new_with_initial_size( ESTIMATED_NUM_DPS );
    guidhash = cfuhash_new_with_initial_size( ESTIMATED_NUM_DPS );
    hashPolicy = cfuhash_new_with_initial_size( ESTIMATED_NUM_POLICY );

    /* reset the house keeping array for the policy */
    memset( hs, 0, sizeof(hs) );
}

/*============================================================================*/
//fn  HASH_fnFindByName
/*!

@brief
    Search for a data point by its name

    The HASH_fnFindByName message_callback function is invoked on reception
    of the MSG_DP_FIND_BY_NAME message in response to a request to search
    for a data point by its name.

    The received message is of type datapoint_get_msg_t and it contains
    all of the attributes required to search for the data point.

    - Updated the message handling API
    - Promoted name to upper case
    - Conditionally apply character translations to the name
    - Moved to printsession.c and changed name
    - Updated datapoint structure to separate the data point identification
    from the data point content to support aliasing

@param[in]
    rcvid
        receive identifier used for message replies

@param[in]
    msg
        pointer to the request message

@param[in]
    cred
        pointer to the client's credentials (UID, GID, supplementary GIDs)

@return
    this function returns 0 on success or an error code from errno.h on failure

*/
/*============================================================================*/
int HASH_fnFindByName( int rcvid, datapoint_get_msg_t *msg,
                         struct _cred_info *cred )
{
    struct dp_id_t *pDatapointID;
    char key[DP_MAX_NAME_LENGTH + 10];
    char *name;

    if( msg == NULL )
    {
        return EINVAL;
    }

    /* get a pointer to the name immediately following the message */
    name = (char *)msg + sizeof(*msg);

    /* apply character translations (if any) to the name */
    NAME_fnConvert( name );

    /* build the name key by concatenating the data point name
     * with an ASCII representation of its instance identifier */
    hash_fnBuildNameKey( name,
                         msg->instanceID,
                         key,
                         sizeof(key) );

    /* search for the data point */
    pDatapointID = cfuhash_get(hash, key );
    if( pDatapointID == NULL )
    {
        return ENOENT;
    }

    MsgReply( rcvid, EOK, &pDatapointID, sizeof(struct dp_id_t *));

    return EOK;
}

/*============================================================================*/
//fn  HASH_fnFindByGUID
/*!

@brief
    Search for a data point by its GUID and instance ID

    The HASH_fnFindByGUID message_callback function is invoked on reception
    of the MSG_DP_FIND_BY_ID message in response to a request to search
    for a data point by its globally unique identifier.

    The received message is of type datapoint_guid_msg_t and it contains
    all of the attributes required to search for the data point.


    - Moved to printsession.c and changed name
    - Updated to use HASH_fnLookupById
    - Updated datapoint structure to separate the data point identification
    from the data point content to support aliasing

@param[in]
    rcvid
        receive identifier used for message replies

@param[in]
    msg
        pointer to the request message

@param[in]
    cred
        pointer to the client's credentials (UID, GID, supplementary GIDs)

@return
    this function returns EOK on success or an error code from errno.h
    on failure

*/
/*============================================================================*/
int HASH_fnFindByGUID( int rcvid, datapoint_guid_msg_t *msg,
                       struct _cred_info *cred )
{
    struct dp_id_t *pDatapointID;

    if( msg == NULL )
    {
        return EINVAL;
    }

    pDatapointID = HASH_fnLookupById( msg->guid, msg->instanceID);
    if( pDatapointID == NULL )
    {
        return ENOENT;
    }

    MsgReply( rcvid, EOK, &pDatapointID, sizeof(struct dp_id_t *));

    return EOK;
}

/*============================================================================*/
//fn  HASH_fnLookupByName
/*!

@brief
    Retrieve a data point from the hash table based on its name

    This function converts the specified name and instance ID into
    a string which it uses as a hash key to retrieve the data point
    structure.

    - Updated datapoint structure to separate the data point identification
    from the data point content to support aliasing

@param[in]
    name
        Null terminated character string containing the data point
        name to search for

@param[in]
    instanceID
        data point instance identifier

@return
    a pointer to the retrieved data point ( struct datapoint_id_t * ),
    or NULL if the data point was not found.

*/
/*============================================================================*/
struct dp_id_t *HASH_fnLookupByName( char *name, uint32_t instanceID )
{
    char key[DP_MAX_NAME_LENGTH + 10];

    /* apply character translations (if any) to the name */
    NAME_fnConvert( name );

    hash_fnBuildNameKey( name,
                         instanceID,
                         key,
                         sizeof(key));

    return cfuhash_get( hash, key );
}

/*============================================================================*/
//fn  HASH_fnLookupById
/*!

@brief
    Retrieve a data point from the hash table based on its GUID

    This function converts the specified 32-bit GUID into an 8-character
    Hex ASCII string and uses this string as a hash key to retreive the
    data point structure.

@param[in]
    guid
        Globally Unique 32-bit identifier specifying the data point
        to retrieve.

@param[in]
    instanceID
        data point instance identifier

@return
    a pointer to the retrieved data point ( struct datapoint_id_t * ),
    or NULL if the data point was not found.




Version: 1.00    Date: 03-Dec-2013    By:
    - Created

Version: 1.01    Date: 03-Jun-2015    By:
    - Moved to printsession.c and changed name

Version: 1.02    Date: 17-Jun-2015    By:
    - Updated datapoint structure to separate the data point identification
    from the data point content to support aliasing

*/
/*============================================================================*/
struct dp_id_t *HASH_fnLookupById( uint32_t guid, uint32_t instanceID )
{
    char guidString[20];

    hash_fnBuildGUIDKey( guid,
                           instanceID,
                           guidString,
                           sizeof(guidString));

    return cfuhash_get(guidhash, guidString );
}

/*============================================================================*/
//fn  hash_fnBuildGUIDKey
/*!

@brief
    Build a globally unique ID hash key

    Build a globally unique identifier hash key based on a GUID
    and an instance identifier.  The instance identifier and guid
    are processed 1 nibble at a time starting with the least significant
    nibble.  It's nibble value (0-F) or (0-15) is added to ASCII 'A' to
    generate a character to be added to the hash key.

    For example, passing the guid 0x800007BC and instance identifier 1 to this
    function will generate a key of "MLHAAAAIBAAAAAAA" as the hash key.

@param[in]
    guid
        Globally Unique 32-bit identifier specifying the data point
        to retrieve.

@param[in]
    instanceID
        data point instance ID

@param[out]
    key
        pointer to the location to store the generated key

@param[in]
    key_len
        length of the buffer to store the generated key

@return
    this function returns EOK on success or any other value from errno.h
    on failure.




Version: 1.00    Date: 05-Dec-2013    By:
    - Created

Version: 1.10    Date: 31-Jan-2014    By:
    - Updated for efficiency to remove snprintf call

Version: 1.11    Date: 03-Jun-2015    By:
    - Moved to printsession.c and changed name

*/
/*============================================================================*/
static int hash_fnBuildGUIDKey( uint32_t guid,
                                uint32_t instanceID,
                                char *key,
                                size_t key_len )
{
    size_t idx;
    int i;

    if( key == NULL || key_len <= 16 )
    {
        return ENOMEM;
    }

    idx = 0;
    for( i=0 ; i<8; i++ )
    {
        key[idx++] = 'A' + ( guid & 0xF );
        guid = guid >> 4;
    }

    for( i=0; i<8; i++ )
    {
        key[idx++] = 'A' + ( instanceID & 0xF );
        instanceID = instanceID >> 4;
    }

    key[idx] = '\0';

    return EOK;
}

/*============================================================================*/
//fn  hash_fnBuildNameKey
/*!

@brief
    Build a globally unique ID hash key

    Build a data point name hash key based on a data point name
    and an instance identifier.  The instance identifier is processed
    1 nibble at a time starting with the least significant nibble.  It's
    nibble value (0-F) or (0-15) is added to ASCII 'A' to generate a
    character to be added to the hash key.

    For example, passing the name "POWER" and instance identifier 1 to this
    function will generate a key of "POWERBAAAAAAA" as the hash key.

@param[in]
    name
        Pointer to a data point name

@param[in]
    instanceID
        data point instance ID

@param[out]
    key
        pointer to the location to store the generated key

@param[in]
    key_len
        length of the buffer to store the generated key

@return
    this function returns EOK on success or any other value from errno.h
    on failure.




Version: 1.00    Date: 05-Dec-2013    By:
    - Created

Version: 1.10    Date: 31-Jan-2014    By:
    - Much faster algorithm no longer using sprintf

Version: 1.11    Date: 03-Jun-2015    By:
    - Moved to printsession.c and changed name

*/
/*============================================================================*/
static int hash_fnBuildNameKey( char *name,
                                uint32_t instanceID,
                                char *key,
                                size_t key_len )
{
    size_t name_len = 0;
    size_t idx = 0;

    if( name == NULL || key == NULL )
    {
        return ENOMEM;
    }

    while( idx < key_len )
    {
        key[idx] = name[idx];
        if( key[idx] == '\0' )
        {
            break;
        }

        idx++;
    }

    name_len = idx;

    if( ( name_len + KEY_INSTANCE_LENGTH + 1 ) > key_len )
    {
        return ENOMEM;
    }

    while( idx < ( name_len + KEY_INSTANCE_LENGTH ) )
    {
        key[idx++] = 'A' + ( instanceID & 0xF );
        instanceID = instanceID >> 4;
    }

    /* null terminate the key */
    key[idx] = '\0';

    return EOK;
}

/*============================================================================*/
//fn  HASH_fnAdd
/*!

@brief
    Add a data point into the hash table(s).

    - Updated to allow creation of a hash key using an optional name
    - Updated datapoint structure to separate the data point identification
    from the data point content to support aliasing

@param[in]
    pDatapointID
        Pointer to a data point

@param[in]
    optional_name
        optional name to use to create the hash key.  If name is NULL the
        data point name will be used instead.

@return
    this function returns EOK on success or any other value from errno.h
    on failure.

*/
/*============================================================================*/
int HASH_fnAdd( struct dp_id_t *pDatapointID, char *optional_name )
{
    char key[DP_MAX_NAME_LENGTH + 10];
    char *name = optional_name;

    if( name == NULL )
    {
        name = pDatapointID->pName;
    }

    /* build the name key */
    hash_fnBuildNameKey( name,
                         pDatapointID->instanceID,
                         key,
                         sizeof(key) );

    /* perform the insert */
    cfuhash_put( hash, key, pDatapointID );

    if( pDatapointID->ulName != 0L )
    {
        /* build the GUID key */
        hash_fnBuildGUIDKey( pDatapointID->ulName,
                             pDatapointID->instanceID,
                             key,
                             sizeof(key) );

        /* perform the insert */
        cfuhash_put( guidhash, key, pDatapointID );
    }

    return EOK;
}

/*============================================================================*/
/*========================== POLICY HASH SECTION =============================*/
/*============================================================================*/
/*!

    Check if the policy data structure already exists

@param[in]
    name
        Null terminated character string containing the policy hash string
        policy hash string is constructed using sprintf in the following order:
        name, type, location, user, group

@retval 1 if an entry exists in the table for the given key, 0 otherwise

*/
/*============================================================================*/
bool POLICYHASH_fnCheck( char *hashString )
{
    return cfuhash_exists(hashPolicy, hashString);
}

/*============================================================================*/
/*!

    Add the new policy to the policy hash table

@param[in]
    pPolicy
        policy data structure to insert in the policy hash table

@param[in]
    pHashString
        name of the policy hash string to be added as the key in to the policy
        hash table.

@return
    old value is returned if it existed, o/w NULL is returned
    if pPlicy or pHashString are null, then -1 is returned

*/
/*============================================================================*/
void* POLICYHASH_fnPut( struct policy_id_t* pPolicy, char* pHashString )
{
	void* ret = (int*)(-1);

	if( (NULL != pPolicy) && ( NULL != pHashString) )
	{
	    /* perform the insert */
	    ret = cfuhash_put( hashPolicy, pHashString, pPolicy );
	}

    return ( ret );
}

/*============================================================================*/
/*!

    remove the unvisited policy from the hash table
    the hash table for policy rules must be kept clean
    and so remove any unvisited policy every time a new push button for policy
    is triggered

@param[in]
    pHashString
        name of the policy hash string to be added as the key in to the policy
        hash table.

@retval EOK or errno.h error codes.

*/
/*============================================================================*/
int POLICYHASH_fnRemove( char* pHashString )
{
	int ret = EINVAL;

	if( NULL != pHashString )
	{
	    /*  delete from policy hash  */
	    cfuhash_delete( hashPolicy, pHashString );
	    ret = EOK;
	}

    return ( ret );
}

/*============================================================================*/
/*!

    Retrieve a policy from the hash table based on its hashString name

    policy hash string is constructed using sprintf in the following order:
        name, type, location, user, group

@param[in]
    name
        Null terminated character string containing the data point
        name to search for

@param[in]
    instanceID
        data point instance identifier

@return
    a pointer to the retrieved data point ( struct datapoint_id_t * ),
    or NULL if the data point was not found.

*/
/*============================================================================*/
struct policy_id_t* POLICYHASH_fnFind( char* hashString )
{
    return cfuhash_get( hashPolicy, hashString );
}

/*============================================================================*/
/*!
	this is an accessor function, it returns the address of the array that
	keeps the house keeping policies

@return
    array handle keeping the house keeping policy structure array

*/
/*============================================================================*/
tzHouseKeep* POLICYHASH_fnHouseKeepAccessor( void )
{
	return ( hs );
}

/*!
 * @} // hash
 */
