/*=============================================================================

University of British Columbia (UBC) 2017
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
MiniCloud Project. List all data points in target system.

==============================================================================*/

#ifndef OBJQUEUE_H_
#define OBJQUEUE_H_

/*!
 * @file objqueue.h
 * @brief generate a queue for the dynamic objects
 *
 * @defgroup queue autonomous objects arriving to service in queue FCFS
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
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

/*==============================================================================
                              Defines
==============================================================================*/
/*! name of each object that is going in the queue */
#define OBJ_NAME_SIZE       ( 512 )

/*! billion granularity */
#define BILLION  1000000000L;

/*! million granularity */
#define MILLION  1000000L;

/*! maximum number of objects */
#define MAX_NUM_OF_OBJECTS ( 100000 )

/*==============================================================================
                                Enums
==============================================================================*/

/*==============================================================================
                            Type Definitions
==============================================================================*/

/*=============================================================================
                              Structures
==============================================================================*/

/*! Object doubly link list in the queue  */
typedef struct zDOUBLY_LIST
{
	/*! pointer to the next object in the queue
	 * property of the object in the queue */
    struct zOBJECT* next;
    /*! pointer to the previous object in the queue
     * property of the object in the queue */
    struct zOBJECT* prev;
} tzDOUBLY_LIST;

/*! Elements needed for keeping an inventory track in the queue for
 * our autonomous objects */
typedef struct zQUEUE
{
    /*! mutex for protecting access to the queue*/
    pthread_mutex_t mutex;
    /*! mutex attributes */
    pthread_mutexattr_t mutexattr;
    /*! object count */
    uint16_t uiObjCount;
    /*! first item in the queue
     * property of the queue */
    struct zOBJECT* pFirst;
    /*! last item in the queue
     * property of the queue */
    struct zOBJECT* pLast;

} tzQUEUE, *ptzQUEUE;

/*! Register instance information */
typedef struct zOBJECT
{
    /*! Pointer to register class definition */
    char ObjName[OBJ_NAME_SIZE];

    /* time object got in the queue*/
    uint64_t tStartQ;

    /* time object got out of the queue*/
    uint64_t tEndQ;

    /* time spent in the queue */
    uint64_t deltaInQueue;

    /* queue time in seconds */
    double secondsQueue;

    /* time object ready for service */
    uint64_t tStartService;

    /* time object is services */
    uint64_t tEndService;

    /* time spent in the service factory */
    uint64_t deltaService;

    /* service time in seconds */
    double secondsService;

//    /* pointer to the next object in the list or NULL if this is the last
//     * object */
//    struct zOBJECT* pNext;

    /* the object's doubly linked list, the keeper of the object queue */
    tzDOUBLY_LIST queue;

    /* indicating beginning of a batch */
    bool beginBatch;

    /* indicating end of a batch */
    bool endBatch;

} tzOBJECT;

/*! discreate event simulator program parameters to be set before running the
 * algorithm code  */
typedef struct zParams
{
	/* mean of the number of jobs to arrive */
	float mean;

	/* for sensitivity analysis purpose:
	 * fix number of drones coming at some rate */
	uint16_t rate;

	/* for sensitivity analysis purpose:
	 * fix number of policy rules */
	uint16_t policyRuleNum;

	/* for sensitivity analysis purpose:
	 * query a particular size between 200B-500B-1KB-...-5KB
	 * so the code is between 1-11 respectively */
	uint16_t queryCode;

	/* standard deviation for the number of jobs arriving */
	float sigma;

	/* inter-arrival rate aka mean arrival rate lambda */
	float lambda;

	/* maximum time that we want to practice */
    int Epoch;

    /* current epoch holder as counter */
    int currEpoch;

    /* clocks per second */
    uint64_t cps;

    /* file handler to pass to generating the normal distribution graph */
    FILE* fNormalDist;

    /* file handler for recording the mean of the service times until steady
     * state performance has been acheived. */
    FILE* fSteadyState;

    /* file that keeps the record of each epoch cycle sums */
    FILE* fCycleSums;

    /* show output stream datapoints and their values */
    bool showOutput;

    /* verbosity level */
    int verbose;

} tzParams;


/*==============================================================================
                          External/Public Constants
==============================================================================*/

/*==============================================================================
                          External/Public Variables
==============================================================================*/

/*==============================================================================
                      External/Public Function Prototypes
==============================================================================*/

int  QUEUE_fnInitialize( void );
int QUEUE_fnAdd( tzQUEUE *pQueue, int num, void* arg );
tzOBJECT* QUEUE_fnGetFirstObj( tzQUEUE *pQueue );
tzOBJECT* QUEUE_fnGetNextObj( tzOBJECT *pObj );
tzOBJECT* QUEUE_fnGetAndRemoveFirstObj( tzQUEUE *pQueue, void* arg );

#endif /* OBJQUEUE_H_ */

/*! @} */

//EoF
