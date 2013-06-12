/*  Messages Types
 *
 *
 */

#define PAR 1
#define PAP 2
#define PAN 3
#define PANC 4
#define HB 6
#define SAP 7
#define SAN 8
#define MAX_CTRL_TYPE 8
#define RV_REG 9
#define RV_ADDR_LOOKUP 10
#define RV_ADDR_SOLVE 11



typedef struct additional_addr{

    /*
     * hdr_type_f contains 5 bits of pkt type and 3 bits of flags
     */
//     unsigned char hdr_type_f; 
    unsigned char t_lenght;     
    unsigned char addr_bit_len;
    // PAP: addr: proposed address to the future child
    struct in6_addr addr;
    unsigned char mask;
    unsigned char conn_count;

};



typedef struct ctrl_pkt{

    /*
     * pkt_type_f constains 5 bits of pkt type and 3 bits of flags
     */

    unsigned char pkt_type_f;
    unsigned char t_length;
    // PAR: src_addr: original if_addr.
    struct in6_addr src_addr;   
    unsigned char prim_addr_len;
    // prim_addr: The primary address for the nodes that has already connected to the network.
    struct in6_addr prim_addr;
    struct additional_addr ad_addr;
};



typedef struct rv_ctrl_pkt{
    unsigned char type;
    unsigned char flags;
    // Identifier for the fragment of the RV table being sent
    unsigned int id;
    // Number of elements in the RV table to be sent
    unsigned int n;
    struct in6_addr prim_addr;
    char univ_addr[50];
    struct in6_addr src_addr;
};



