/*=============================================================================

University of British Columbia (UBC)
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
Mini Cloud Server Project.

==============================================================================*/

#ifndef POLICY_H_
#define POLICY_H_

/*!
 * @file policy.h
 * @brief Public APIs for managing data points
 *
 * The policy.h file contains the public APIs, types, and
 * data structures for managing policy
 *
 * @defgroup policy policy Management
 * @brief Manage policies
 *
 * The Policy management functions provide the following features
 *     - Policy Creation
 *     - Policy Setting
 *     - Policy Retrieval
 *     - Policy Rendering
 *     - Policy Queries
 */

 /*! @{ */

/*==============================================================================
                                 Includes
 =============================================================================*/

#include "minicloudmsg.h"

/*=============================================================================
                              Structures
==============================================================================*/

/*! The policy_id_t structure contains policy linked list info.
 */
struct policy_id_t
{
	/*! policy data structure */
	struct zPOLICY policy;

    /*! points to the next policy in the iterator list */
    struct policy_id_t *pNext;
};

/*==============================================================================
                           Function Declarations
==============================================================================*/


int POLICY_fnCreatePolicy( int rcvid, datapoint_policy_msg_t *msg );
int POLICY_fnHouseKeepPolicy( int rcvid, datapoint_policy_msg_t *msg );
struct policy_id_t* POLICY_fnGetHead( void );
bool POLICY_fnCheck( struct dp_t *pDp );


/*! @} */

#endif /* POLICY_H_ */
