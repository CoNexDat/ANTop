/*  Number of times it will be tried to send
 *  primary address requests packets.
 */


#define N_PAR 1

/*  Time we will wait for receive primary address
 *  proposal packets.
 */

#define T_PAP 3

//#define MILI_SECOND 1
//#define MICRO_SECOND 2

/*
 *  Max number of PAP that will be accepted
 */

#define T_PANC 6

//Seconds to wait for the SAN

#define T_SAN 2


#define MAX_RCVD_PAP 10

#define NO_ADDR_AVLB -1
#define ADDR_OK 0
#define NULL_ADDR_SET -2

/*
 *  Number of miliseconds we will wait between sending Heart Beats
 */

#define T_HB 5000

/*
 *  Number of miliseconds we will wait between sending registration to RV
 */

#define T_RV 3000

/*
 * Dimension of the Hypercube. 
 * By default it's the 64 bits host portion of IPv6 addresses
 */

#define HC_DIM 64


/*
 * Time to wait for HB, in ms. Should be a little more than T_HB
 */ 


#define PLUS_HB 3000
#define T_LHB (T_HB + PLUS_HB)

/*
 * Time to delete a route from routing table
 */ 

#define T_ROUTE 3000

/*
 *  Max number of iterations without getting a HB for a neighbor
 *  before we consider it gone
 */

#define N_INACT 1

/*
 *  This is the mask used to get the IsReturned flag value.
 */

#define MASK_IR 1

/*
 *  This is the mask used to get the IsRendezVouz flag value.
 */

#define MASK_RV 2

/*
 *  This is the mask used to get the IsAntop flag value.
 */


#define MASK_IA 4

/*
 *  This is the mask used to get the Solved flag value.
 */


#define MASK_SOLVED 1

/*
 *  Offset to the flag field from the beggining of the packet.
 */ 

#define FLAGS_OFFSET 3


/*
 *  Offset to the src addr field from the beggining of the packet.
 */

#define SRC_ADDR_OFFSET 8

#define DST_ADDR_OFFSET 24

#define TTL_OFFSET 7

#define FROM_ADDR_OFFSET 56

#define MAX_TTL 10

#define MAX_NEIGHBORS 128

/*
 *  Timer for the visited bitmap, miliseconds
 */

#define T_VB 1400

/* Number of neighbors to visit before sending the packet
 * to the parent
 */

#define NEIGHBORS_BEFORE_PARENT 2

#define NXT_HDR_OFFSET 6

#define IPV6_HDR_LEN 40

#define DEST_OPT_HDR 32

#define UDP_HDR_LEN 8

#define ICMPV6_HDR_LEN 8

// This is the offset to the query field in the MDNS packet.

//2 bytes Transaction
//        +
//2 bytes Flags
//        +
//2 bytes Questions
//        +
//2 bytes Answers RRs
//        +
//2 bytes Authority RRs
//        +
//2 bytes Additional RRs
//----------------------
//     12 bytes

#define MDNS_QUERY_OFFSET 12

#define IPV6_MDNS_QUERY_OFF IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN + MDNS_QUERY_OFFSET

#define UDP_PKT_OFF IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN

#define MDNS_QUERY_PKT_OFF IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN

/*
 Maximun lenght for the universal address
 */

#define MAX_UA_LEN 10

#define ANTOP_PORT 4469
#define MDNS_PORT 5353
#define MDNS_PORT_AUX 4473
#define DNS_PORT 53
#define DNS_PORT_AUX 4477

#define RV_ACCEPT 1
#define RV_DROP 2














