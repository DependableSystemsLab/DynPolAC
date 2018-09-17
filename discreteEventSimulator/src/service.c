/*=============================================================================

University of British Columbia (UBC) 2017
Electrical and Computer Engineering (ECE)
Internet of Things Group (IoT)
MiniCloud Project. List all data points in target system.

==============================================================================*/

/*============================================================================*/

/*!
 * @file service.h
 * @brief Service factory for policy and query processing
 *
 * @addtogroup discreteEventSimulator
 *
 */

 /*! @{ */

/*==============================================================================
                                  Includes
 =============================================================================*/
#include "service.h"
#include "objqueue.h"

/*==============================================================================
                                  Structs
 =============================================================================*/

/*==============================================================================
                         Local Module Variables
 =============================================================================*/
/*! virtual function pointer to populate custom-made output stream variables
 * and values */
extern outputFn outputFCN;

/*==============================================================================
                 Local/Private Function Prototypes
 =============================================================================*/
static void service_fnQueryDatabase( DPRM_HANDLE hDPRM,
                                     char *key,
                                     teMatchType matchType,
                                     char *tag,
                                     teTagMatchType tagMatchType,
                                     char *value,
                                     teValMatchType valMatchType,
                                     uint16_t flags,
                                     uint32_t contextID1,
                                     uint32_t contextID2,
                                     tzDataPointValueData tzDataPointValueData,
                                     outputFn outputFCN,
                                     void* arg );
static void service_fnQueryStatic( DPRM_HANDLE hDPRM,
                                     char *key,
                                     teMatchType matchType,
                                     char *tag,
                                     teTagMatchType tagMatchType,
                                     char *value,
                                     teValMatchType valMatchType,
                                     uint16_t flags,
                                     uint32_t contextID1,
                                     uint32_t contextID2,
                                     tzDataPointValueData tzDataPointValueData,
                                     outputFn outputFCN, //virtual fcn typedef
                                     void* arg );
static int uniform_distribution(int rangeLow, int rangeHigh);
static int service_fnTimestampMatch( int checkTimestamp,
                                     struct timespec *pMatchTime,
                                     struct timespec *pVarTime );
/*============================================================================*/

/*==============================================================================
                            Function Definitions
 =============================================================================*/

/*============================================================================*/
/*!

    Entry point for service factory: policy registration and the query

@param[in]
    hDPRM
        data point resource manager

@param[in]
    key
        the searching keyword passed to the program

@param[in]
    matchType
        telling what kind of the match type is will be
            is it regular expression match type
            is it any expression match type, etc.

@param[in]
    arg
        application parameters like showing the outputstream switch

 */
/*============================================================================*/
void SERVICE_fnProcess( DPRM_HANDLE hDPRM,
                        char* key,
                        teMatchType matchType,
                        void* arg )
{
    tzParams* params = (tzParams*)arg;
    char *tag = NULL;
    uint16_t flags = 0;
    uint32_t contextID1 = 0;
    uint32_t contextID2 = 0;
    teTagMatchType tagMatchType = 0;
    char *value = NULL;
    teValMatchType valMatchType = eValMatchInvalid;
    tzDataPointValueData tzDataPointValueData;
    int choice = 0;
    char policy[512];

    memset(policy, 0, sizeof(policy) );


    /* bring back to default first */
    system("defdp -p /etc/voidPolicy.xml");

    /* choose for sensitivity simulation or free run */
    if( 0 == params->policyRuleNum )
    {
        /* policy registration
         * NOTE: based on varying the data base we get different query size */
        choice = uniform_distribution(1, 6);
        switch(choice)
        {
        case 1:
            system("defdp -p /sim/policyTesla.xml");
            break;
        case 2:
            system("defdp -p /sim/2policy.xml");
            break;
        case 3:
            system("defdp -p /sim/3policy.xml");
            break;
        case 4:
            system("defdp -p /sim/4policy.xml");
            break;
        case 5:
            system("defdp -p /sim/5policy.xml");
            break;
        case 6:
            system("defdp -p /sim/6policy.xml");
            break;
        }
    }
    else
    {
        if( 1 == params->policyRuleNum )
        {
            system("defdp -p /sim/policyTesla.xml");
        }
        else
        {
            /* scale is in 8 rules per file so 2policy means 2 x 8*/
            sprintf( policy,
                     "defdp -p /sim/%upolicy.xml",
                     params->policyRuleNum );
            system(policy);
        }
    }


    if( 0 == params->queryCode )
    {
        /* query section */
        /* defdp -f /etc/10B.xml -p /etc/policyTesla.xml */
        /* date 201708142108.00 */
        service_fnQueryDatabase( hDPRM,
                                 key,
                                 matchType,
                                 tag,
                                 tagMatchType,
                                 value,
                                 valMatchType,
                                 flags,
                                 contextID1,
                                 contextID2,
                                 tzDataPointValueData,
                                 outputFCN,
                                 arg );

    }
    else
    {
        service_fnQueryStatic( hDPRM,
                               key,
                               matchType,
                               tag,
                               tagMatchType,
                               value,
                               valMatchType,
                               flags,
                               contextID1,
                               contextID2,
                               tzDataPointValueData,
                               outputFCN,
                               arg );
    }
}

/*============================================================================*/
/*!
    Query our database

@param[in]
    hDPRM
        Data base handle resource manager
@param[in]
    key

@param[in]
    matchType

@param[in]
    tag

@param[in]
    tagMatchType

@param[in]
    value

@param[in]
    valMatchType

@param[in]
    flags
        flags if any

@param[in]
    contextID1
        handle1 to keep

@param[in]
    contextID2
        handle2 to keep

@param[in]
    tzDataPointValueData

@param[in]
    outputFn
        virtual function to tell where to print the values

@param[in]
    arg
        program parameter arguments to pass around

@retval - NULL

*/
/*============================================================================*/
static void service_fnQueryDatabase( DPRM_HANDLE hDPRM,
                                     char *key,
                                     teMatchType matchType,
                                     char *tag,
                                     teTagMatchType tagMatchType,
                                     char *value,
                                     teValMatchType valMatchType,
                                     uint16_t flags,
                                     uint32_t contextID1,
                                     uint32_t contextID2,
                                     tzDataPointValueData tzDataPointValueData,
                                     outputFn outputFCN, //virtual fcn typedef
                                     void* arg )
{
    tzParams* params = (tzParams*)arg;

    DP_HANDLE hDataPoint = NULL;
    DP_tzQUERY query;
    uint32_t instanceID = 0;
    uint32_t startID = 0;
    uint32_t endID = 0;
    int timestamp = 0;
    struct timespec ts;
    int count = 0;

    /* process mapping functions */
    hDataPoint = DP_fnGetFirst( hDPRM,
                                0,
                                key,
                                matchType,
                                tag,
                                tagMatchType,
                                value,
                                valMatchType,
                                flags,
                                &contextID1,
                                &contextID2,
                                &tzDataPointValueData );
    while ( hDataPoint != NULL )
    {
        memset(&query, 0, sizeof(query));

        /* get data point information */
        DP_fnQuery(hDPRM, hDataPoint, eBasicQuery, &query, NULL, 0);

        if ((query.flags & DP_FLAG_HIDDEN) == DP_FLAG_HIDDEN)
        {
            /* ignore hidden data points */
        }
        else
        {
            if ((instanceID == 0) || (query.instanceID == instanceID))
            {
                if (((startID == 0) && (endID == 0)) ||
                    ((query.guid >= startID) && (query.guid <= endID)))
                {
                    /* check for timestamp match */
                    if( service_fnTimestampMatch( timestamp,
                                                  &ts,
                                                  &query.timestamp ) )
                    {
                        /* if asked to print to the output stream */
                        if( params->showOutput )
                        {
                            /* output the datapoint data */
                            if (outputFCN != NULL)
                            {
                                outputFCN( hDPRM,
                                          hDataPoint,
                                          &query,
                                          count );
                            }
                        }

                        /* increment the variable counter */
                        count++;
                    }
                }
            }
        }

        /* get the next variable */
        hDataPoint = DP_fnGetNext( hDPRM,
                                    0,
                                    hDataPoint,
                                    key,
                                    matchType,
                                    tagMatchType,
                                    flags,
                                    contextID1,
                                    contextID2,
                                    &tzDataPointValueData );
    }
}

/*============================================================================*/
/*!
    Static Query for simulation purpose only

@param[in]
    hDPRM
        Data base handle resource manager
@param[in]
    key

@param[in]
    matchType

@param[in]
    tag

@param[in]
    tagMatchType

@param[in]
    value

@param[in]
    valMatchType

@param[in]
    flags
        flags if any

@param[in]
    contextID1
        handle1 to keep

@param[in]
    contextID2
        handle2 to keep

@param[in]
    tzDataPointValueData

@param[in]
    outputFn
        virtual function to tell where to print the values

@param[in]
    arg
        program parameter arguments to pass around

@retval - NULL

*/
/*============================================================================*/
static void service_fnQueryStatic( DPRM_HANDLE hDPRM,
                                     char *key,
                                     teMatchType matchType,
                                     char *tag,
                                     teTagMatchType tagMatchType,
                                     char *value,
                                     teValMatchType valMatchType,
                                     uint16_t flags,
                                     uint32_t contextID1,
                                     uint32_t contextID2,
                                     tzDataPointValueData tzDataPointValueData,
                                     outputFn outputFCN, //virtual fcn typedef
                                     void* arg )
{
    tzParams* params = (tzParams*)arg;

    DP_HANDLE hDataPoint = NULL;
    DP_tzQUERY query;
    uint32_t instanceID = 0;
    uint32_t startID = 0;
    uint32_t endID = 0;
    int timestamp = 0;
    struct timespec ts;
//    int count = 0;
    char* test;

    /* process mapping functions */
    hDataPoint = DP_fnGetFirst( hDPRM,
                                0,
                                key,
                                matchType,
                                tag,
                                tagMatchType,
                                value,
                                valMatchType,
                                flags,
                                &contextID1,
                                &contextID2,
                                &tzDataPointValueData );
    while ( hDataPoint != NULL )
    {
        memset(&query, 0, sizeof(query));

        /* get data point information */
        DP_fnQuery(hDPRM, hDataPoint, eBasicQuery, &query, NULL, 0);

        if ((query.flags & DP_FLAG_HIDDEN) == DP_FLAG_HIDDEN)
        {
            /* ignore hidden data points */
        }
        else
        {
            if ((instanceID == 0) || (query.instanceID == instanceID))
            {
                if (((startID == 0) && (endID == 0)) ||
                    ((query.guid >= startID) && (query.guid <= endID)))
                {
                    /* check for timestamp match */
                    if( service_fnTimestampMatch( timestamp,
                                                  &ts,
                                                  &query.timestamp ) )
                    {
                        /* BLANK */
                    }
                }
            }
        }

        /* get the next variable */
        hDataPoint = DP_fnGetNext( hDPRM,
                                    0,
                                    hDataPoint,
                                    key,
                                    matchType,
                                    tagMatchType,
                                    flags,
                                    contextID1,
                                    contextID2,
                                    &tzDataPointValueData );
    }


    /* simulate query sizes */
    if( 1 == params->queryCode)
    {
        test = malloc(200);
    }
    else
    {
        test = malloc( (params->queryCode-1) * 500 );
    }

    if( NULL == test )
    {
        printf("cannot give memory to test\n");
    }

    free(test);
    test = NULL;

}


/*============================================================================*/
/*!

    uniform distribution

@param[in]
    rangeLow
        giving the low range value

@param[in]
    rangeHigh
        giving the high range value

@retval - int the random number generated in uniform distribution between
              the high and low values.

*/
/*============================================================================*/
static int uniform_distribution(int rangeLow, int rangeHigh)
{
    double myRand = rand()/(1.0 + RAND_MAX);

    int range = rangeHigh - rangeLow + 1;

    int myRand_scaled = (myRand * range) + rangeLow;

    return myRand_scaled;
}

/*============================================================================*/
/*!

    check if datapoint timestamp is greater than match timestamp

@param[in]
    checkTimestamp

@param[in]
    pMatchTime

@param[in]
    pVarTime


@retval - int

*/
/*============================================================================*/
static int service_fnTimestampMatch( int checkTimestamp,
                                   struct timespec *pMatchTime,
                                   struct timespec *pVarTime )
{
    if( checkTimestamp == 1 )
    {
        if( pVarTime->tv_sec > pMatchTime->tv_sec )
        {
            return 1;
        }
        else if( pVarTime->tv_sec == pMatchTime->tv_sec )
        {
            if( pVarTime->tv_nsec > pMatchTime->tv_nsec )
            {
                return 1;
            }
        }
    }
    else
    {
        // if we do not have to check the timestamp, always return 1
        return 1;
    }

    return 0;
}

/*! @} */

//EoF

