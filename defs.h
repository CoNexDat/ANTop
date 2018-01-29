struct neighbor{

    struct in6_addr prim_addr;
    struct in6_addr sec_addr;
    // Connected to our sec addr flag
    int sec_addr_conn;

    /*
     * This is the inactivity counter
     */
    int count;   
    struct neighbor *next;
    unsigned char prim_mask;
    unsigned char prim_mask_inicial;//pablo4

    // This is 1 if the neighbor is available i.e. it is connected.
    unsigned char available;
};

struct recovered_addr{

    unsigned char prim_mask;
    struct in6_addr prim_addr;
    struct recovered_addr *next;
};

struct route{
    
    struct in6_addr dst;
    struct in6_addr gtw;
    int metric;
    int dst_mask;
    int distance;
    struct route *next;
    unsigned char table;

};

// This structure contains the parameters passed to the send_hb function. Since it's
// a callback, we pass only one variable. Timer is a boolean that specifies whether
// a timer should be triggered or not

struct hb_param{
    int *sock;
    int timer;
};

struct rt_route_param{
    struct in6_addr dst;
    struct in6_addr gtw;
    unsigned char mask;
    int distance;
};

struct src_nat{
    struct in6_addr dst;
    struct in6_addr src;
    struct src_nat *next;
};

struct neighbor_to_root {  //pablo
      struct in6_addr addr;  //pablo
      int distance; //pablo
};//pablo


//NEIGHBOR WITH SECOND LINK
struct n_s_l {//pablo4
	struct in6_addr addr; //pablo4
	unsigned char mask; //pablo4
	struct n_s_l *next;//pablo4
};//pablo4






