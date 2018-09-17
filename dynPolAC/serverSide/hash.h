/*=============================================================================

University of British Columbia (UBC)
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
Mini Cloud Server Project.

==============================================================================*/

#ifndef HASH_H_
#define HASH_H_

/*!
 * @file hash.h
 * @brief Public APIs for hashing data point names and ids
 *
 * The hash.h file contains the public APIs, types, and
 * data structures for managing data point hashes.
 *
 * @defgroup hash Data Point Hashing
 * @brief Data Point hash management
 *
 * The data point server maintains a hash table to quickly locate
 * data points by their name and globally unique identifier.
 * The functions in the hash file are used to add a data point
 * to the hash table, and find a data point using its name or GUID.
 *
 */

 /*! @{ */

 /*=============================================================================
 	 	 	 	 	 	 	 	 	 Includes
 =============================================================================*/

#include "minicloud.h"
#include "minicloudmsg.h"
#include "dp.h"                       /* internal data point definitions */
#include "policy.h"

/*==============================================================================
 	 	 	 	 	 	 	 	 	 Defines
==============================================================================*/
/*! an estimate for the number of distinct policy rule buckets */
#define ESTIMATED_NUM_POLICY    ( 200 )

/*==============================================================================
 	 	 	 	 	 	 	 	 Structures
 =============================================================================*/
/*! this structure keeps the record for the visited policy rules, and the ones
 * that have not been visited need to be removed, every time that the policy
 * is edited. */
typedef struct zHouseKeep
{
	/*! policy handler */
	struct policy_id_t* pPolicy;
	/*! a flag, each time the policy rules are seen this
        flag is turned to true */
	bool Seen;
} tzHouseKeep;

/*==============================================================================
                           Function Declarations
==============================================================================*/

void HASH_fnSetup(void);

int HASH_fnAdd( struct dp_id_t *pDatapointID,
                char *optional_name );

int HASH_fnFindByName( int rcvid, datapoint_get_msg_t *msg,
                       struct _cred_info *cred );

int HASH_fnFindByGUID( int rcvid, datapoint_guid_msg_t *msg,
                       struct _cred_info *cred );

struct dp_id_t *HASH_fnLookupByName( char *name,
                                         uint32_t instanceID );

struct dp_id_t *HASH_fnLookupById( uint32_t guid,
                                       uint32_t instanceID );

/* following are policy hash public function */
bool POLICYHASH_fnCheck( char *name );
void* POLICYHASH_fnPut( struct policy_id_t* pPolicy, char* pHashString );
int POLICYHASH_fnRemove( char* pHashString );
struct policy_id_t* POLICYHASH_fnFind( char* hashString );
tzHouseKeep* POLICYHASH_fnHouseKeepAccessor( void );

/*! @} */

#endif /* HASH_H_ */
