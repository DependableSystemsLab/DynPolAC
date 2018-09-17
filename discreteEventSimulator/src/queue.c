/*=============================================================================

University of British Columbia (UBC) 2017
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
MiniCloud Project. List all data points in target system.

==============================================================================*/

/*!
 * @file queue.c
 * @brief generate a queue for the dynamic objects
 *
 * @addtogroup queue autonomous objects arriving to service in queue FCFS
 * @brief queue generator
 *
 *  The queue functions for creating and removing or adding queues of objects
 *  to be serviced by the minicloud server
 *
 */

 /*! @{ */
/*==============================================================================
                              Includes
==============================================================================*/
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/neutrino.h>
#include <inttypes.h>
#include <sys/syspage.h>

#include "objqueue.h"

/*==============================================================================
                                Defines
==============================================================================*/

/*==============================================================================
                                Enums
==============================================================================*/

/*==============================================================================
                              Structures
==============================================================================*/

/*==============================================================================
                           Local/Private Variables
==============================================================================*/
extern tzQUEUE* pQueue;
//tzOBJECT** pObj;
tzOBJECT* pObj[MAX_NUM_OF_OBJECTS];
int objCounter = 0;
/*==============================================================================
                        Local/Private Function Prototypes
==============================================================================*/
static int queue_fnAddObj( tzQUEUE *pQueue, tzOBJECT *pObj, void* arg );

/*==============================================================================
                           Function Definitions
==============================================================================*/

/*============================================================================*/
/*!
    Add objects to the queue

    Add the number of objects that have arrived to the queue

@param[in]
    pQueue
        the queue handle

@param[in]
    num
        number of objects being added to the queue

@param[in]
    arg
        system parameters accompanying like the verbosity and number of epochs

@retval EOK or errno.h upon any errors

*/
/*============================================================================*/
int QUEUE_fnAdd( tzQUEUE *pQueue, int num, void* arg )
{
    int ret = EINVAL;
    int i = 0;


    for(i=0; i<num; i++)
    {
        pObj[objCounter] = calloc(1, sizeof(struct zOBJECT) );
        if( NULL == pObj[objCounter])
        {
            printf("cannot calloc pObj[%d]\n", objCounter );
        }
        else
        {
            /* Unmanned Aircraft System (UAS) Name Registration */
            strncpy(pObj[objCounter]->ObjName, "Drone", strlen( "Drone" ) + 1 );
            if( 0 == i )
            {
                pObj[objCounter]->beginBatch = true;
            }
            if( ( num - 1 ) == i )
            {
                pObj[objCounter]->endBatch = true;
            }

            /* add objects one by one to the queue */
            queue_fnAddObj( pQueue,  pObj[objCounter], arg );

            objCounter++;

            ret = ( EOK );

        }
    }

    return ( ret );

}

/*============================================================================*/
/*!
    Get first object from the queue

    This function gets the first object from the specified queue

@param[in]
    pQueue
        Pointer to the queue

@return
    Pointer to the first block of the queue,
    NULL if empty queue or invalid argument

*/
/*============================================================================*/
tzOBJECT* QUEUE_fnGetFirstObj( tzQUEUE *pQueue )
{
    tzOBJECT* pObject = (tzOBJECT*)NULL;

    if( (tzQUEUE*)NULL != pQueue )
    {
        if( (tzOBJECT*)NULL != pQueue->pFirst )
        {
            pObject = pQueue->pFirst;
        }
    }
    return( pObject );
}

/*============================================================================*/
/*!
    Get the next object from the queue

    This function gets the next object from the queue given current object

@param[in]
    pObject - pointer to the current block in the queue

@return
    Pointer to the next object in the queue or NULL upon an error

*/
/*============================================================================*/
tzOBJECT* QUEUE_fnGetNextObj( tzOBJECT *pObject )
{
    tzOBJECT* retObj = (tzOBJECT*)NULL;

    if( (tzOBJECT*)NULL != pObject )
    {
        retObj = (tzOBJECT*)pObject->queue.next;
    }
    return( retObj );
}

/*============================================================================*/
/*!
    Get and remove the first object from the queue

    This function returns and removes the first object from the queue
    Characteristics of the system, from book the art of computer systems
    performance analysis page 509:
    item number 6:
        Service Discipline: the order in which objects are served
        here the service discipline is first-come-first-serve (FCFS)

    Essentially this function is for taking out items from the queue one
    by one.

@param[in]
    pQ Pointer to the block

@param[in]
    arg
        Accompanying application params

@return
    Pointer to the first block of the queue or NULL otherwise

*/
/*============================================================================*/
tzOBJECT* QUEUE_fnGetAndRemoveFirstObj( tzQUEUE *pQ, void* arg )
{
    tzOBJECT *pSecondObj;
    tzOBJECT *pToBeRemoved = (tzOBJECT *)NULL;
    tzParams* params = (tzParams*)arg;

    if(  pQ != (tzQUEUE*)NULL )
    {
        pthread_mutex_lock( &pQ->mutex );

        if( (tzOBJECT*)NULL != pQ->pFirst )
        {
            pToBeRemoved = pQ->pFirst;

            /* Check if the queue has only one element*/
            if( pQ->pFirst == pQ->pLast )
            {
                pQ->pFirst = (tzOBJECT*)NULL;
                pQ->pLast  = (tzOBJECT*)NULL;
            }
            else
            {
                pSecondObj             = pQ->pFirst->queue.next;
                pSecondObj->queue.prev = (tzOBJECT*)NULL;
                pQ->pFirst             = pSecondObj;
            }
            pToBeRemoved->queue.next   = (tzOBJECT*)NULL;
            pToBeRemoved->queue.prev   = (tzOBJECT*)NULL;

            pQ->uiObjCount--;

        }

        /* --- end time going out of the queue --- */
        pToBeRemoved->tEndQ = ClockCycles( );
        /* ------------------------------ */

        pthread_mutex_unlock( &pQ->mutex );
    }

    /* update time spent in queue for each object */

    pToBeRemoved->deltaInQueue = pToBeRemoved->tEndQ - pToBeRemoved->tStartQ;

    pToBeRemoved->secondsQueue =(double)pToBeRemoved->deltaInQueue/params->cps;

    if( params->verbose)
    {
        fprintf( stdout,
                 "%45.2lf ms",
                 pToBeRemoved->secondsQueue*1000 );
    }


    return( pToBeRemoved );
}

/*============================================================================*/
/*!
    Add a new object to the queue

    This function adds a new object to the queue
    the clock starts ticking from the time the object has been added to the q

@param[in]
    pQueue
        the queue handle

@param[in]
    pObj
        incoming object handle

@param[in]
    arg
        system arguments accompanying

@retval EOK or errno.h upon any errors

*/
/*============================================================================*/
static int queue_fnAddObj( tzQUEUE *pQueue, tzOBJECT *pObj, void* arg )
{
    int ret = EINVAL;
    tzOBJECT* pTmpObj;
    tzParams* params = (tzParams*)arg;

    if( ( pQueue != (tzQUEUE *)NULL ) && ( pObj != (tzOBJECT*)NULL ) )
    {
        pthread_mutex_lock( &pQueue->mutex );

        /* ----- start time in the queue ----- */
        pObj->tStartQ = ClockCycles( );
        /* ------------------------ */

        /* Add the item to the end of the list */
        if( (tzOBJECT*)NULL == pQueue->pFirst )
        {
            /* this is the first object */
            pQueue->pFirst    = pObj;
            pQueue->pLast     = pObj;
            pObj->queue.prev  = NULL;
            pObj->queue.next  = NULL;
        }
        else
        {
            /* Handling of the doubly linked list */
            pTmpObj               = pQueue->pLast;
            pTmpObj->queue.next   = (void*)pObj;
            pObj->queue.prev      = (void*)pTmpObj;
            pObj->queue.next      = (void*)NULL;
            pQueue->pLast         = pObj;
        }
        pQueue->uiObjCount++;

        /* print to stdout if verbosity is set */
        if( params->verbose )
        {
            if( pQueue->uiObjCount < 2 )
            {
                fprintf(stdout,
                        "%10u\n",
                        pQueue->uiObjCount );
            }
            else
            {
                fprintf(stdout,
                        "%37u\n",
                        pQueue->uiObjCount );
            }
        }

        pthread_mutex_unlock( &pQueue->mutex );

        ret = EOK;
    }

    return ( ret );
}

/*============================================================================*/
/*!
    Initialise a queue for autonomous objects

    Create and Initialise a new queue and initialises mutexes
    Create a new queue for autonomous objects

@param[in]
    Queue
        pointer to the newly created queue

@return EOK or errno.h codes on error
        note for software best practices, there must be one
        function entry and exit only. no middle-exit is good.

*/
/*============================================================================*/
int  QUEUE_fnInitialize( void )
{
    int ret = EINVAL;

    pQueue = (tzQUEUE*)calloc( 1, sizeof( tzQUEUE ) );
    if( (tzQUEUE*)NULL == pQueue )
    {
        ret = ENOMEM;
    }
    else
    {
        pthread_mutexattr_init( &pQueue->mutexattr );
        pthread_mutex_init( &pQueue->mutex, &pQueue->mutexattr );

        ret = EOK;
    }

    return( ret );
}

/*! @} */

//EoF
