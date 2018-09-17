/*=============================================================================

University of British Columbia (UBC) 2017
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
MiniCloud Project. List all data points in target system.

==============================================================================*/

/*============================================================================*/

/*!
 * @file discreteEventSimulator.c
 * @brief Statistical Analysis for discrete event simulation of autonomous
 * objects
 *
 *@details

    This application will generate random Objects' demand
    then queue the up
    it will do the query afterwards and the reply to the query is back
    the mean service rate is realized.

 * @addtogroup discreteEventSimulator autonomous objects arriving to service
 * in queue FCFS
 *
 */
 /*! @{ */

/*==============================================================================
                                    Includes
 =============================================================================*/

#include <ctype.h>
#include <grp.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <inttypes.h>
#include <sys/syspage.h>
#include <math.h>
#include <pthread.h>
#include <sys/syspage.h>

#include "service.h"
#include "objqueue.h"


/*==============================================================================
                                Defines
 =============================================================================*/
/*! network latency in seconds, assumed 50ms
 * Reference:
 * ``Sarr, Cheikh, and Isabelle Guérin-Lassous. Estimating average end-to-end
 * delays in IEEE 802.11 multihop wireless networks. Diss. INRIA, 2007.'' */
#define NETWORK_LATENCY ( 0.05f )

/*! file to record normal distribution */
#define NORMAL_DIST_FILE "/ubc/Mehdi/NormalDist.csv"
/*! file to record steady state performance */
#define STEADY_STATE_PERFORMANCE "/ubc/Mehdi/SteadyStatePerformance.csv"
/*! file to record the cycle sums */
#define CYCLE_SUMS_FILE "/ubc/Mehdi/CycleSums.csv"
/*! file to record Queueing times */
#define QUEUE_TIME_FILE "/ubc/Mehdi/QueueTime.csv"
/*! file to record Service times (policy + ) */
#define SERVICE_TIME_FILE "/ubc/Mehdi/ServiceTime.csv"

/*==============================================================================
                                Structs
 =============================================================================*/

/*==============================================================================
                         Local Module Variables
 =============================================================================*/
/*! virtual function declaration */
outputFn outputFCN = NULL;

/*! shared memory buffer */
static char *pSharedMemBuffer = NULL;

/*! queue declaration */
tzQUEUE* pQueue;

/*! switch to test the normal distribution density */
static bool testDistribition = false;

/*! external object counter to keep the simulation running */
extern int objCounter;

/*! an epoch cycle sum */
float y[ MAX_NUM_OF_OBJECTS ];

/*! difference between expected and observed cycle sums */
float w[ MAX_NUM_OF_OBJECTS ];

/*! variance of the difference */
float Var_w;

/*! number of drones that have arrived every time */
int numberArrived[ MAX_NUM_OF_OBJECTS ];

/*! live calculation of the overall mean employing Regeneration Method */
float overalMean;

/*! sum of the cycle sums */
float sumOfCycleSums;

/*! mean cycle length */
float meanCycleLength;

/*! sum of w */
float sumOf_w;

/*! Calculate the confidence interval */
float confidenceInterval;

/*! sum of the every time arrival rate */
int sumOfNumbers;

/*! index of the cycle to calculate the sum */
static int cycleIdx = 0;

/*==============================================================================
                      Local/Private Function Prototypes
 =============================================================================*/

static double discreteEventSimulator_fnNormalDistribution( double mu,
                                                           double sigma );

static void discreteEventSimulator_fnOutputVarValue( DPRM_HANDLE hDPRM,
                                                     DP_HANDLE hDataPoint,
                                                     DP_tzQUERY *ptzQuery,
                                                     int count );

static void* discreteEventSimulator_fnArrival( void* arg );

static void discreteEventSimulator_fnCalcRT( tzParams* arg,
        tzOBJECT* pServiceObject,
        float NL );
/*============================================================================*/
/*!

@brief
    Entry point for the discreteEventSimulator command

    This application will generate random Objects' demand
    then queue the up
    it will do the query afterwards and the reply to the query is back
    the mean service rate (miu) is realized.


     steps:
     * 1- create a random guassian numbner
     * 2- create a random timer interval
     * 3- put the number of arriving in a queue
     * 4- calculate the service time
     * 5- report the query service time based on the policy of each
     *


    usage:
        discreteEventSimulator
              [-m <mean value for random number generator>]
              [-s <sigma is the standard deviation>]
              [-l <lambda is the mean arrival rate>]
              [-E <Number of Epochs>]
              [-o <show output data streams>]
              Sensitivity options:
              [-p <path to save the SteadyStatePerformance file>]
              [-f <sensitivity analysis: fix lambda factor (arrival rate)
                  ie. number of drones at some fix number>]
              [-n <sensitivity analysis: policy file number code>]
                  - THE POLICY FILES ARE --HANDMADE-- AND ARE IN
                    MULTIPLES OF 8 RULES.
              [-q <sensitivity analysis: query size number code>]
                  1 means query size 200B   only
                  2 means query size 500B   only
                  3 means query size 1KB    only
                  4 means query size 1.5KB  only
                  5 means query size 2KB    only
                  6 means query size 2.5KB  only
                  7 means query size 3KB    only
                  8 means query size 3.5KB  only
                  9 means query size 4KB    only
                  10 means query size 4.5KB only
                  11 means query size 5KB   only

@param[in]
     argc
     number of arguments passed to the process

@param[in]
     argv
         array of null terminated argument strings passed to the process
         argv[0] is the name of the process

@return
 EXIT_FAILURE - indicates the discreteEventSimulator function did not complete
                successfully
 EXIT_SUCCESS - indicates the discreteEventSimulator function completed
                successfully

 */
/*============================================================================*/
int main(int argc, char *argv[])
{
    int opt;
    int errflag = 0;
    char* key = NULL;
    teMatchType matchType = eMatchContains;

    tzParams params;
    DPRM_HANDLE hDPRM;
    int shmem_fd = -1;
    tzOBJECT* pServiceObject = (tzOBJECT*)NULL;

    /* thread scheduler attributes */
    pthread_attr_t schedAttr;

    memset( &params, 0, sizeof(struct zParams) );
    memset( y, 0, sizeof(y) );
    memset( w, 0, sizeof(w) );
    memset( numberArrived, 0, sizeof(numberArrived) );
    overalMean = 0.0;
    sumOfCycleSums = 0.0;
    sumOfNumbers = 0;
    Var_w = 0; /* variance of the difference */
    sumOf_w = 0;
    meanCycleLength = 0;
    confidenceInterval = 0.0;

    /* Network Latency */
    float NL = NETWORK_LATENCY;

    /* create file handlers */
    char* normalDistFile  = NORMAL_DIST_FILE;
    char* steadyStateFile = STEADY_STATE_PERFORMANCE;
    char* cycleSumsFile   = CYCLE_SUMS_FILE;
//    const char* queueTimeFile   = QUEUE_TIME_FILE;
//    const char* serviceTimeFile = SERVICE_TIME_FILE;

    /* process the command line options */
    while ((opt = getopt(argc, argv, "vs:m:l:E:of:p:n:q:")) != -1)
    {
        switch (opt)
        {
        case 'q':
            /* force to use a fix query size */
            params.queryCode = atoi(optarg);
            break;

        case 'n':
            /* force to use a policy file */
            params.policyRuleNum = atoi(optarg);
            break;

        case 'p':
            steadyStateFile = optarg;
            break;

        case 'f':
            /* force the number of rates to incoming factor */
            params.rate = atoi(optarg);
            break;

        case 's':
            /* objects standard deviation*/
            params.sigma = atof(optarg);
            break;

        case 'm':
            /* mean of objects */
            params.mean = atof(optarg);
            break;

        case 'l':
            /* inter-arrival frequency */
            params.lambda = atof(optarg);
            break;

        case 'E':
            params.Epoch = atof(optarg);
            break;

        case 'v':
            /* verbosity */
            params.verbose = 1;
            break;

        case 'o':
            /* show output stream */
            params.showOutput = true;
            break;

        case '?':
            ++errflag;
            break;

        default:
            break;
        }
    }

    if( errflag )
    {
        return EXIT_FAILURE;
    }

    /* if a regular expression search is added at the end */
    if( optind < argc )
    {
        key = argv[optind];
        matchType = eMatchRegex;
    }

    params.fNormalDist  = fopen( normalDistFile, "w" );
    if( !params.fNormalDist )
    {
        return EXIT_FAILURE;
    }
    params.fSteadyState = fopen( steadyStateFile, "w" );
    if( !params.fSteadyState )
    {
        return EXIT_FAILURE;
    }
    params.fCycleSums   = fopen( cycleSumsFile, "w" );
    if( !params.fCycleSums )
    {
        return EXIT_FAILURE;
    }
//    FILE* fQueueTime   = fopen( queueTimeFile, "w" );
//    if( !fQueueTime )
//    {
//      return EXIT_FAILURE;
//    }
//    FILE* fServiceTime = fopen( serviceTimeFile, "w" );
//    if( !fServiceTime )
//    {
//      return EXIT_FAILURE;
//    }
    fprintf( params.fNormalDist,  "Epoch,Object\n");
    fprintf( params.fSteadyState, "Epoch,Overall mean,confidence interval\n");
    fprintf( params.fCycleSums, "cycle sums(ms)\n");
    fflush( params.fNormalDist );
    fflush( params.fSteadyState );
    fflush( params.fCycleSums );

    /* find out how many cycles per second */
    params.cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;

    /* virtual function pointer can be used later to add more functions
     * based on the input keys */
    outputFCN = discreteEventSimulator_fnOutputVarValue;

    /* open the data point manager */
    hDPRM = DP_fnOpen();
    if (hDPRM == NULL)
    {
        fprintf(stderr, "Unable to open Data Point Resource Manager\n");
        return EXIT_FAILURE;
    }

    /* create a shared memory object so we can print the query result */
    pSharedMemBuffer = DP_fnCreateMem( hDPRM, 16384L, &shmem_fd );

    /*
     * https://stackoverflow.com/questions/1683461/generating-a-gaussian-distribution-with-only-positive-numbers
     */

    /* Step 0 - Initialise and get a queue handler */
    if(params.verbose)
    {
        printf("Initialising the Queue...");
    }
    QUEUE_fnInitialize( );
    if(params.verbose)
    {
        printf("Queue Initialised.\n");
    }


    /* Create a thread for generating arrival rate */
    pthread_attr_init( &schedAttr );
    pthread_attr_setdetachstate( &schedAttr, PTHREAD_CREATE_DETACHED );
    pthread_create( NULL,
                    &schedAttr,
                    &discreteEventSimulator_fnArrival,
                    &params );


    while( objCounter <= MAX_NUM_OF_OBJECTS )
    {
        if( !testDistribition )
        {
            /* 4- get the item from the queue and service the request */
            while( (struct zOBJECT*)NULL != pQueue->pFirst )
            {
                pServiceObject =
                        QUEUE_fnGetAndRemoveFirstObj( pQueue, &params );

                if( NULL == pServiceObject)
                {
                    break;
                }

                /* ----- snap time in the server ----- */
                pServiceObject->tStartService = ClockCycles( );


                /* == service request one by one: Policy + Query  == */

                SERVICE_fnProcess( hDPRM, key, matchType, &params );

                /* ================================================= */

                /* --- snap the time again --- */
                pServiceObject->tEndService = ClockCycles( );
                pServiceObject->deltaService =
                        pServiceObject->tEndService -
                        pServiceObject->tStartService;

                pServiceObject->secondsService =(double)
                        pServiceObject->deltaService/params.cps;


                /* log the response time for the corresponding object */
                discreteEventSimulator_fnCalcRT( &params,
                                                 pServiceObject,
                                                 NL );
            }
        }

        /* exit simulation if exceeded requested number of simulation */
        if( params.currEpoch > params.Epoch )
        {
            break;
        }
    }

    /* close the data point manager */
    DP_fnClose(hDPRM);

    /* closing all files */
    fclose( params.fNormalDist );
    fclose( params.fSteadyState );
//    fclose( fQueueTime );
//    fclose( fServiceTime );

    /* indicate success */
    return EXIT_SUCCESS;
}


/*============================================================================*/
/*!
    Calculate and log the response Time

@param[in]
    params
        program parameters

@param[in]
    pServiceObject
        Object coming out of thr FCFS queue to be inspected for timing

@param[in]
    Network Latency
        Network Latency -- at the moment is a constant factor

@retval - NULL

*/
/*============================================================================*/
static void discreteEventSimulator_fnCalcRT( tzParams* params,
        tzOBJECT* pServiceObject,
        float NL )
{
    /* Response Time -- Mnemonic */
    float ResponseTime = (float)pServiceObject->secondsQueue +
            (float)pServiceObject->secondsService + NL;

    float milliSecondsQ = (float)pServiceObject->secondsQueue * 1000;
    float milliSecondsS = (float)pServiceObject->secondsService * 1000;

    /* compute cycle sums in an epoch */
    if( !pServiceObject->endBatch )
    {
        y[ cycleIdx ] += ( milliSecondsQ + milliSecondsS );
    }
    else if( pServiceObject->endBatch )
    {
        /* Finalise computation of cycle sums in an epoch */
        y[ cycleIdx ] += ( milliSecondsQ + milliSecondsS );
        fprintf( params->fCycleSums, "%d,%0.4f\n",
                cycleIdx,
                y[ cycleIdx ] );
        fflush( params->fCycleSums );

        /* calculating the sum of cycle sums lively */
        sumOfCycleSums += y[ cycleIdx ];
        sumOfNumbers += numberArrived[ cycleIdx];

        /* calculating the overall Mean lively */
        overalMean = (float)sumOfCycleSums / (float)sumOfNumbers;

        /* calculate the difference between expected and observed cycle sums */
        w[ cycleIdx ] = y[ cycleIdx ] - (numberArrived[ cycleIdx] * overalMean);

        sumOf_w += ( w[ cycleIdx ] * w[ cycleIdx ] );


        /* Regeneration method, step 4. p435 - calculate the variance of diff.
         * only calculate if more than 1, ie. 2 epochs are practices,
         * otherwise will get infinity  */
        if( params->currEpoch > 1 )
        {
            Var_w = sumOf_w / ( params->currEpoch - 1 ) ;

            /* compute the mean cycle length */
            meanCycleLength = (float)sumOfNumbers/ (float)params->currEpoch;

            /* +/- Z_1-alpha/2 * s_w / n_bar * sqrt(m)
             * 90 percent confidence interval 1.645 */
            confidenceInterval = 1.645 *
                    ( sqrt(Var_w) /
                            (meanCycleLength * sqrt(params->currEpoch) ) );

            /* record samples every x number */
            if( ( 0 == ( params->currEpoch % 100 ) ) ||
                    ( params->currEpoch < 1000 ) )
            {
                fprintf( params->fSteadyState, "%d,%0.2f,%0.2f\n",
                        params->currEpoch,
                        overalMean,
                        confidenceInterval );
                fflush( params->fSteadyState );
            }
        }

        cycleIdx++;
    }

    if( params->verbose )
    {
        fprintf(stdout,
                "%12.2lf ms %12.2lf ms %18.2lf\n",
                (float)milliSecondsS,
                (float)ResponseTime * 1000,
                (float)(1/(float)ResponseTime) );
        fflush(stdout);
    }

}

/*============================================================================*/
/*!
    this is the first thread for creation of objects arriving

@param[in]
    arg
        passing parameter of the thread

@retval - NULL

*/
/*============================================================================*/
static void* discreteEventSimulator_fnArrival( void* arg )
{

    tzParams* params = (tzParams*)arg;

    int ret = EINVAL;
    int numberOfArrivals = 0;
    float lambda = params->lambda;

    float lambdaMilli = lambda * 1000;

    while( objCounter <= MAX_NUM_OF_OBJECTS )
    {
        if( 0 == params->rate )
        {
            /* Guassian distribution model */
            numberOfArrivals =
                    abs( discreteEventSimulator_fnNormalDistribution
                            ( params->mean, params->sigma ) ) + 1;
        }
        else
        {
            /* sensitivity for a fix number */
            numberOfArrivals = params->rate;
        }


        /* array list of number of arrivals every time */
        numberArrived[ params->currEpoch ] = numberOfArrivals;

        params->currEpoch++;

        fprintf( params->fNormalDist, "%d,%d\n",
                 params->currEpoch,
                 numberOfArrivals );
        fflush( params->fNormalDist );

        if( !testDistribition )
        {
            if( params->verbose )
            {
                fprintf(stdout, ">>Epoch#%d\n", params->currEpoch );

                /* headers */
                fprintf(stdout,
                        "%25s %10s %10s %12s %12s %19s\n",
                        "Mean Arrival Rate Lambda(/s)",
                        "#inQueue",
                        "QueueTime",
                        "ServerTime",
                        "ResponseT",
                        "RRate(u[Hz])");
                fprintf(stdout,
                        "%27s %10s %10s %12s %12s %18s\n",
                        "-----------------------------",
                        "----------",
                        "-----------",
                        "-----------",
                        "-----------",
                        "-------------------");

                fprintf(stdout,
                        "%27.2f",
                        (float)numberOfArrivals/lambda );
            }

            /* queue the number of arrived drones */
            ret = QUEUE_fnAdd( pQueue, numberOfArrivals, arg );
            if( ret != EOK )
            {
                printf("could not add an object, exiting.\n");
                break;
            }

            delay(lambdaMilli);
        }

        /* exit simulation if exceeded requested number of simulation */
        if( params->currEpoch > params->Epoch )
        {
            break;
        }
    }

    return( NULL );
}

/*============================================================================*/
/*!

    print the value

@param[in]
    hDPRM
        data point resource manager

@param[in]
    hDataPoint
        data point handle

@param[in]
    ptzQuery
        query structure handle

@param[in]
    count
        data point resource manager

@retval - double the value of in the normal distribution

*/
/*============================================================================*/
static void discreteEventSimulator_fnOutputVarValue( DPRM_HANDLE hDPRM,
                             DP_HANDLE hDataPoint,
                             DP_tzQUERY *ptzQuery,
                             int count)
{
    if ( NULL != hDataPoint )
    {
        /* output the data point name */
        DP_fnPrintName(hDPRM, stdout, hDataPoint, DP_PROG_ACCESS );
        fflush(stdout);

        /* output trailing quote for data point name */
        fprintf(stdout, " = ");
        fflush(stdout);

        /* output the data point value */
        DP_fnPrint(hDPRM, stdout, hDataPoint, 0, DP_PROG_ACCESS );

        fprintf(stdout, "\n");
        fflush(stdout);
    }
}

/*============================================================================*/
/*!

    Generating a random number based on the mu mean and standard deviation sigma
    Guassian distribution

@param[in]
    mu
        mean of the normal distribution

@param[in]
    sigma
        standard deviation

@retval - double the value of in the normal distribution

*/
/*============================================================================*/
static double discreteEventSimulator_fnNormalDistribution( double mu,
                                                           double sigma )
{
  double U1, U2, W, mult;
  static double X1, X2;
  static int call = 0;

  if (call == 1)
  {
    call = !call;
    return (mu + sigma * (double) X2);
  }

  do
  {
    U1 = -1 + ((double) rand () / RAND_MAX) * 2;
    U2 = -1 + ((double) rand () / RAND_MAX) * 2;
    W = pow (U1, 2) + pow (U2, 2);
  } while (W >= 1 || W == 0);

  mult = sqrt ((-2 * log (W)) / W);
  X1 = U1 * mult;
  X2 = U2 * mult;

  call = !call;

  return (mu + sigma * (double) X1);
}


/*! @} */

//EoF
