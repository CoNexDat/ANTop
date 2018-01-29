#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <libmnl/libmnl.h>
#include <linux/if.h>
#include <linux/rtnetlink.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <net/route.h>
#include "rtnl.h"
#include <errno.h>
#include <sys/ioctl.h>
#include "timer_queue.h"
#include "defs.h"
#include "labels.h"
#include "pkt_handle.h"
#define RTACTION_ADD   1
#define RTACTION_DEL   2
#define RTACTION_HELP  3
#define RTACTION_FLUSH 4
#define RTACTION_SHOW  5


// Pointer to the beggining of the routing table in user space
struct route *route_t = NULL;
// Pointer to surf the routing table in user space
struct route *rt_ptr;
extern int ifindex;
//struct rt_route_param rt_params;
//static struct timer rt_route_t;



int data_attr_cb2(const struct nlattr *attr, void *data)
{
	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, RTAX_MAX) < 0)
		return MNL_CB_OK;

	if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
		perror("mnl_attr_validate");
		return MNL_CB_ERROR;
	}
	return MNL_CB_OK;
}

void attributes_show_ipv6(struct nlattr *tb[], char *print)
{   
        if(*print)
            fprintf(stderr,"\n-----------------------------------\n");

    
        if (tb[RTA_TABLE]) {
            if((rt_ptr->table = mnl_attr_get_u32(tb[RTA_TABLE])) != 254)
                return;
            if(*print)
		fprintf(stderr,"table=%u \n", mnl_attr_get_u32(tb[RTA_TABLE]));
	}
        

	if (tb[RTA_DST]) {
		struct in6_addr *addr = mnl_attr_get_payload(tb[RTA_DST]);
                if(*print)
                fprintf(stderr,"dst=%s \n", ip6_to_str(*addr));
                memcpy(&(rt_ptr->dst), addr, sizeof(struct in6_addr));
//                fprintf(stderr,"dst=%s ", inet_ntoa(*addr));
	}      
	if (tb[RTA_SRC]) {
		struct in6_addr *addr = mnl_attr_get_payload(tb[RTA_SRC]);
                if(*print)
		fprintf(stderr,"src=%s \n", ip6_to_str(*addr));
//                memcpy(&(route_t->src), addr, sizeof(struct in6_addr));
//                fprintf(stderr,"src=%s ", inet_ntoa(*addr));
	}
	if (tb[RTA_OIF]) {
//		fprintf(stderr,"oif=%u \n", mnl_attr_get_u32(tb[RTA_OIF]));
	}
        if (tb[RTA_PRIORITY]) {
//		fprintf(stderr,"metric=%u \n", mnl_attr_get_u32(tb[RTA_PRIORITY]));
                rt_ptr->distance =  mnl_attr_get_u32(tb[RTA_PRIORITY]);
	}
	if (tb[RTA_FLOW]) {
//		fprintf(stderr,"flow=%u \n", mnl_attr_get_u32(tb[RTA_FLOW]));
	}
	if (tb[RTA_PREFSRC]) {
		struct in6_addr *addr = mnl_attr_get_payload(tb[RTA_PREFSRC]);
   //             fprintf(stderr,"prefsrc=%s ", inet_ntoa(*addr));
  //              fprintf(stderr,"pref src=%s \n", ip6_to_str(*addr));
	}
	if (tb[RTA_GATEWAY]) {
		struct in6_addr *addr = mnl_attr_get_payload(tb[RTA_GATEWAY]);
                if(*print)
                    fprintf(stderr,"gw=%s \n", ip6_to_str(*addr));
                
                if(addr != NULL);
                memcpy(&(rt_ptr->gtw), addr, sizeof(struct in6_addr));
//                fprintf(stderr,"gw=%s ", inet_ntoa(*addr));
	}
	if (tb[RTA_METRICS]) {
		int i;
		struct nlattr *tbx[RTAX_MAX+1] = {};
		mnl_attr_parse_nested(tb[RTA_METRICS], data_attr_cb2, tbx);
		for (i=0; i<RTAX_MAX; i++) {
			if (tbx[i]) {
//				fprintf(stderr, "metrics[%d]=%u ",i, mnl_attr_get_u32(tbx[i]));
			}
		}
	}        
        if(rt_ptr->next == NULL){
            if((rt_ptr->next = malloc(sizeof(struct route))) == NULL){
                fprintf(stderr, "Could not locate space for dumping routing table \n");
                return;
            }
            (rt_ptr->next)->next = NULL;
        }
        rt_ptr = rt_ptr->next;
        
        
}



int data_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, RTA_MAX) < 0)
		return MNL_CB_OK;
//        fprintf(stderr,"Attribute type: %d \n",type);
//        fprintf(stderr,"MAX Attribute type: %d \n",RTA_MAX);
        switch(type) {
	case RTA_TABLE:
	case RTA_DST:
	case RTA_SRC:
	case RTA_OIF:
	case RTA_FLOW:
	case RTA_PREFSRC:
	case RTA_GATEWAY:
/*
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
*/
	case RTA_METRICS:
		if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {

			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	}
	tb[type] = attr;    //REVISAR ESTO !!!!!! TYPE =0
	return MNL_CB_OK;
}



int data_cb(const struct nlmsghdr *nlh, void *data)
{


	struct nlattr *tb[RTA_MAX+1] = {};
	struct rtmsg *rm = mnl_nlmsg_get_payload(nlh);
        
  //      fprintf(stderr,"DATA_CB\n");
        
	/* protocol family = AF_INET | AF_INET6 */
    //    fprintf(stderr, "TOS: %u \n", rm->rtm_tos);
/*
        if(rm->rtm_family==AF_INET6)
        fprintf(stderr,"family= AF_INET6  ");
        if(rm->rtm_family==AF_INET)
        fprintf(stderr,"family= AF_INET  ");
*/
	/* destination CIDR, eg. 24 or 32 for IPv4 */
//	fprintf(stderr,"dst_len=%u ", rm->rtm_dst_len);
        rt_ptr->dst_mask = rm->rtm_dst_len;

	/* source CIDR */
//	fprintf(stderr,"src_len=%u ", rm->rtm_src_len);

	/* type of service (TOS), eg. 0 */
//	fprintf(stderr,"tos=%u ", rm->rtm_tos);

	/* table id:
	 *	RT_TABLE_UNSPEC		= 0
	 *
	 * 	... user defined values ...
	 *
	 *	RT_TABLE_COMPAT		= 252
	 *	RT_TABLE_DEFAULT	= 253
	 *	RT_TABLE_MAIN		= 254
	 *	RT_TABLE_LOCAL		= 255
	 *	RT_TABLE_MAX		= 0xFFFFFFFF
	 *
	 * Synonimous attribute: RTA_TABLE.
	 */
//	fprintf(stderr,"table=%u ", rm->rtm_table);

	/* type:
	 * 	RTN_UNSPEC	= 0
	 * 	RTN_UNICAST	= 1
	 * 	RTN_LOCAL	= 2
	 * 	RTN_BROADCAST	= 3
	 *	RTN_ANYCAST	= 4
	 *	RTN_MULTICAST	= 5
	 *	RTN_BLACKHOLE	= 6
	 *	RTN_UNREACHABLE	= 7
	 *	RTN_PROHIBIT	= 8
	 *	RTN_THROW	= 9
	 *	RTN_NAT		= 10
	 *	RTN_XRESOLVE	= 11
	 *	__RTN_MAX	= 12
	 */
//	fprintf(stderr,"type=%u ", rm->rtm_type);

	/* scope:
	 * 	RT_SCOPE_UNIVERSE	= 0   : everywhere in the universe
	 *
	 *      ... user defined values ...
	 *
	 * 	RT_SCOPE_SITE		= 200
	 * 	RT_SCOPE_LINK		= 253 : destination attached to link
	 * 	RT_SCOPE_HOST		= 254 : local address
	 * 	RT_SCOPE_NOWHERE	= 255 : not existing destination
	 */
//	fprintf(stderr,"scope=%u ", rm->rtm_scope);

	/* protocol:
	 * 	RTPROT_UNSPEC	= 0
	 * 	RTPROT_REDIRECT = 1
	 * 	RTPROT_KERNEL	= 2 : route installed by kernel
	 * 	RTPROT_BOOT	= 3 : route installed during boot
	 * 	RTPROT_STATIC	= 4 : route installed by administrator
	 *
	 * Values >= RTPROT_STATIC are not interpreted by kernel, they are
	 * just user-defined.
	 */
//	fprintf(stderr,"proto=%u ", rm->rtm_protocol);

	/* flags:
	 * 	RTM_F_NOTIFY	= 0x100: notify user of route change
	 * 	RTM_F_CLONED	= 0x200: this route is cloned
	 * 	RTM_F_EQUALIZE	= 0x400: Multipath equalizer: NI
	 * 	RTM_F_PREFIX	= 0x800: Prefix addresses
	 */
//	fprintf(stderr,"flags=%d\n", rm->rtm_flags);

	mnl_attr_parse(nlh, sizeof(*rm), data_attr_cb, tb);



	switch(rm->rtm_family) {
	case AF_INET6:
		attributes_show_ipv6(tb, (char *) &data);
//                fprintf(stderr, "\n % u \n\n", (*rt)->distance);
		break;
	}

	return MNL_CB_OK;
}




struct rtnl_handle *rtnl_open(void)
{
	size_t addrlen;
	struct rtnl_handle *h;
	h = malloc(sizeof(struct rtnl_handle));
	if (!h)
		return NULL;
	addrlen = sizeof(h->rtnl_local);

	h->rtnl_local.nl_pid = getpid();
	h->rtnl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (h->rtnl_fd < 0) {
		fprintf(stderr, "unable to create rtnetlink socket \n");
		goto err;
	}

	memset(&h->rtnl_local, 0, sizeof(h->rtnl_local));
	h->rtnl_local.nl_family = AF_NETLINK;
//	h->rtnl_local.nl_groups = RTMGRP_LINK;
        	h->rtnl_local.nl_groups = 0;
	if (bind(h->rtnl_fd, (struct sockaddr *) &h->rtnl_local, addrlen) < 0) {
		fprintf(stderr,"unable to bind rtnetlink socket \n");
		goto err_close;
	}

	if (getsockname(h->rtnl_fd, (struct sockaddr *) &h->rtnl_local,	&addrlen) < 0) {
		fprintf(stderr, "cannot gescockname(rtnl_socket) \n");
		goto err_close;
	}

	if (addrlen != sizeof(h->rtnl_local)) {
		fprintf(stderr, "invalid address size %u \n", addrlen);
		goto err_close;
	}

	if (h->rtnl_local.nl_family != AF_NETLINK) {
		fprintf(stderr, "invalid AF %u \n", h->rtnl_local.nl_family);
		goto err_close;
	}

	h->rtnl_seq = time(NULL);

	return h;

err_close:
	close(h->rtnl_fd);
err:
	free(h);
	return NULL;
}


int rtnl_dump_type(struct rtnl_handle *rtnl_handle, unsigned int type)
{
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;
	struct sockaddr_nl nladdr;

	memset(&nladdr, 0, sizeof(nladdr));
	memset(&req, 0, sizeof(req));
	nladdr.nl_family = AF_NETLINK;

	req.nlh.nlmsg_len = sizeof(req);
 	req.nlh.nlmsg_type = type;
	req.nlh.nlmsg_flags = NLM_F_REQUEST|NLM_F_ROOT;


/*

        ------------------- FLAGS -----------------------

        – NLM F ROOT - Used With various data retrieval (GET) oper-
  ations for various netlink protocols, request messages with this flag
  set indicate that an entire table of data should be returned rather
  than a single entry. Setting this flag usually results in a response
  message with the NLM F MULTI flag set. Note that while this
  flag is set in the netlink header, the get request is protocol specific,
  and the specific get request is specified in the nlmsg type field.

         – NLM F REQUEST - This flag identifies a request message. It
  should be set on almost all application initiated messages

         – NLM F MATCH - This flag indicates only a subset of data
  should be returned on a protocol specific GET request, as specified
  by a protocol specific filter. Not yet implemented.


        ---------------------------------------------------

*/
	req.nlh.nlmsg_pid = 0;
	req.nlh.nlmsg_seq = rtnl_handle->rtnl_dump = ++(rtnl_handle->rtnl_seq);
	req.g.rtgen_family = AF_INET6;

	return sendto(rtnl_handle->rtnl_fd, (void*)&req, sizeof(req), 0,
		      (struct sockaddr*)&nladdr, sizeof(nladdr));
}




int rtnl_handler_register(struct rtnl_handle *rtnl_handle, struct rtnl_handler *hdlr)
{
	fprintf(stderr, "registering handler for type %u \n", hdlr->nlmsg_type);
	hdlr->next = rtnl_handle->handlers;
	rtnl_handle->handlers = hdlr;
	return 1;
}


int rtnl_parse_rta(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rta *) * max);

	while (RTA_OK(rta, len)) {
            fprintf(stderr,"RTA_OK \n");
		if (rta->rta_type <= max)
			tb[rta->rta_type] = rta;
		rta = RTA_NEXT(rta,len);
	}
	if (len)
		return -1;
	return 0;
}



struct route *rt_table_dump(char print){


        
        
        struct mnl_socket *nl;
	char bufer[MNL_SOCKET_BUFFER_SIZE];   //********************************************************
	struct nlmsghdr *nlh;
	struct rtmsg *rtm;
	int ret;
	unsigned int seq, portid;

        if(route_t == NULL){ // Check if it has already been created
	
        if((route_t = malloc(sizeof(struct route))) == NULL){
            fprintf(stderr, "Could not locate space for dumping routing table\n");
            return NULL;
        }
        }
        rt_ptr = route_t;
               
	
	nlh = mnl_nlmsg_put_header(bufer);
	nlh->nlmsg_type = RTM_GETROUTE;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ROOT;
	nlh->nlmsg_seq = seq = time(NULL);
	rtm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
	rtm->rtm_family = AF_INET6;

        nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		return NULL;
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		return NULL;
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		return NULL;
	}

	ret = mnl_socket_recvfrom(nl, bufer, sizeof(bufer));

        if(print)
            fprintf(stderr,"\nRT TABLE");
        
	while (ret > 0) {
                
//		ret = mnl_cb_run(bufer, ret, seq, portid, data_cb, NULL);

            ret = mnl_cb_run(bufer, ret, seq, portid, data_cb, print);                //************************
		
		if (ret <= MNL_CB_STOP){
                    
  //                  rt->next = NULL;
                    break;
                }                
/*
                if(rt->next = malloc(sizeof(struct route)) == NULL){
                    fprintf(stderr, "Could not locate space for dumping routing table\n");
                    return NULL;
                };
                rt = rt->next;
*/
                
		ret = mnl_socket_recvfrom(nl, bufer, sizeof(bufer));
                
	}
//        free(rt_ptr); ESTO DA ERROR !!!!!
 //       rt_ptr = NULL; //El ultimo estuvo de mas
	if (ret == -1) {
		perror("error");
		return NULL;
	}

	mnl_socket_close(nl);     
	
	return route_t;


}

int del_rt_route2(struct rt_route_param *parameters){
/*
	int iface;
	iface = if_nametoindex(s_if);

	if (iface == 0) {
		fprintf(stderr,"Bad interface name\n");
		return 0;
	}
*/

/*
        struct in6_addr src;

        src.s6_addr16[0]=0x80fe;
        src.s6_addr16[1]=0x0000;
        src.s6_addr16[2]=0x0000;
        src.s6_addr16[3]=0x0000;
        src.s6_addr16[4]=0x0c02;
        src.s6_addr16[5]=0xff6e;
        src.s6_addr16[6]=0xd3fe;
        src.s6_addr16[7]=0x6667;
*/
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct rtmsg *rtm;
        
        fprintf(stderr, "A route has expire. It will be deleted. DST: %s; GTW: %s\n", ip6_to_str(parameters->dst), ip6_to_str(parameters->gtw));

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_DELROUTE;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
	nlh->nlmsg_seq = time(NULL);
	rtm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
	rtm->rtm_family = AF_INET6;
	rtm->rtm_dst_len = parameters->mask;
	rtm->rtm_src_len = 0;
	rtm->rtm_tos = 0;
	rtm->rtm_protocol = RTPROT_BOOT;
	rtm->rtm_table = RT_TABLE_MAIN;
	rtm->rtm_type = RTN_UNICAST;
	// is there any gateway?
//	rtm->rtm_scope = (&gw != NULL) ? RT_SCOPE_LINK : RT_SCOPE_UNIVERSE;      //ESTO COMO ES???????????
        rtm->rtm_scope = RT_SCOPE_LINK;

	rtm->rtm_flags = 0;

//      mnl_attr_put(nlh, RTA_SRC, sizeof(struct in6_addr), &src);

        mnl_attr_put(nlh, RTA_DST, sizeof(struct in6_addr), &(parameters->dst));

	mnl_attr_put_u32(nlh, RTA_OIF, ifindex);

        //The priority represents the distance

        mnl_attr_put_u32(nlh, RTA_PRIORITY, parameters->distance);

//      mnl_attr_put_u32(nlh, RTA_METRICS, 2);

//	if (argc >= 5)
//	mnl_attr_put_u32(nlh, RTA_GATEWAY, gw);

        mnl_attr_put(nlh, RTA_GATEWAY, sizeof(struct in6_addr), &(parameters->gtw));       

	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}
	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}
	mnl_socket_close(nl);
        
        // Besides we have to delete the auxiliary entry.
        
        del_rt_info(&(parameters->dst), &(parameters->gtw));                
        
        return 0;
}



int add_rt_route(struct in6_addr dst, struct in6_addr gw, unsigned char mask, int distance){

/*
	int iface;
	iface = if_nametoindex(s_if);
        
	if (iface == 0) {
		fprintf(stderr,"Bad interface name\n");
		return 0;
	}
*/
/*
        struct in6_addr src;

        src.s6_addr16[0]=0x80fe;
        src.s6_addr16[1]=0x0000;
        src.s6_addr16[2]=0x0000;
        src.s6_addr16[3]=0x0000;
        src.s6_addr16[4]=0x1702;
        src.s6_addr16[5]=0xff31;
        src.s6_addr16[6]=0x48fe;
        src.s6_addr16[7]=0xa364;
*/

	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];    //***************************************************
	struct nlmsghdr *nlh;
	struct rtmsg *rtm;
        struct timer *rt_route_t;
        struct rt_route_param *rt_params;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWROUTE;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
	nlh->nlmsg_seq = time(NULL);

	rtm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
	rtm->rtm_family = AF_INET6;
	rtm->rtm_dst_len = mask;
	rtm->rtm_src_len = 0;
	rtm->rtm_tos = 0;
	rtm->rtm_protocol = RTPROT_BOOT;
	rtm->rtm_table = RT_TABLE_MAIN;
	rtm->rtm_type = RTN_UNICAST;
	// is there any gateway?
//	rtm->rtm_scope = (&gw != NULL) ? RT_SCOPE_LINK : RT_SCOPE_UNIVERSE;      //ESTO COMO ES???????????
        rtm->rtm_scope = RT_SCOPE_LINK;

	rtm->rtm_flags = 0;

//	mnl_attr_put_u32(nlh, RTA_DST, dst);

//      mnl_attr_put(nlh, RTA_PREFSRC, sizeof(struct in6_addr), &src);

//      mnl_attr_put(nlh, RTA_SRC, sizeof(struct in6_addr), &src);

        mnl_attr_put(nlh, RTA_DST, sizeof(struct in6_addr), &dst);

	mnl_attr_put_u32(nlh, RTA_OIF, ifindex);

        // The priority represents the distance

        mnl_attr_put_u32(nlh, RTA_PRIORITY, distance);

 //       mnl_attr_put_u32(nlh, RTA_METRICS, 2);

//	if (argc >= 5)
//	mnl_attr_put_u32(nlh, RTA_GATEWAY, gw);

        mnl_attr_put(nlh, RTA_GATEWAY, sizeof(struct in6_addr), &gw);

	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	mnl_socket_close(nl);
        
        fprintf(stderr,"A route will be added\n");
        
        rt_route_t = malloc(sizeof(struct timer));    
        rt_params = malloc(sizeof(struct rt_route_param));
        
        memcpy(&(rt_params->dst), &(dst), sizeof(struct in6_addr));
        memcpy(&(rt_params->gtw), &(gw), sizeof(struct in6_addr));
        // CHECK THIS, DISTANCE WILL CHANGE
        rt_params->distance = distance;
        rt_params->mask = mask;        
        

        rt_route_t->handler = &del_rt_route2;
        rt_route_t->data = rt_params;

        rt_route_t->used = 0;
        
        timer_add_msec(rt_route_t, T_ROUTE);
        return 0;


}





int del_rt_route( struct in6_addr dst, struct in6_addr gw, unsigned char mask, int distance){
/*
	int iface;
	iface = if_nametoindex(s_if);

	if (iface == 0) {
		fprintf(stderr,"Bad interface name\n");
		return 0;
	}
*/

/*
        struct in6_addr src;

        src.s6_addr16[0]=0x80fe;
        src.s6_addr16[1]=0x0000;
        src.s6_addr16[2]=0x0000;
        src.s6_addr16[3]=0x0000;
        src.s6_addr16[4]=0x0c02;
        src.s6_addr16[5]=0xff6e;
        src.s6_addr16[6]=0xd3fe;
        src.s6_addr16[7]=0x6667;
*/
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];   //***************************************************
	struct nlmsghdr *nlh;
	struct rtmsg *rtm;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_DELROUTE;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
	nlh->nlmsg_seq = time(NULL);
	rtm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
	rtm->rtm_family = AF_INET6;
	rtm->rtm_dst_len = mask;
	rtm->rtm_src_len = 0;
	rtm->rtm_tos = 0;
	rtm->rtm_protocol = RTPROT_BOOT;
	rtm->rtm_table = RT_TABLE_MAIN;
	rtm->rtm_type = RTN_UNICAST;
	// is there any gateway?
//	rtm->rtm_scope = (&gw != NULL) ? RT_SCOPE_LINK : RT_SCOPE_UNIVERSE;      //ESTO COMO ES???????????
        rtm->rtm_scope = RT_SCOPE_LINK;

	rtm->rtm_flags = 0;

//      mnl_attr_put(nlh, RTA_SRC, sizeof(struct in6_addr), &src);

        mnl_attr_put(nlh, RTA_DST, sizeof(struct in6_addr), &dst);

	mnl_attr_put_u32(nlh, RTA_OIF, ifindex);

        //The priority represents the distance

        mnl_attr_put_u32(nlh, RTA_PRIORITY, distance);

//      mnl_attr_put_u32(nlh, RTA_METRICS, 2);

//	if (argc >= 5)
//	mnl_attr_put_u32(nlh, RTA_GATEWAY, gw);

        mnl_attr_put(nlh, RTA_GATEWAY, sizeof(struct in6_addr), &gw);

	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}
	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}
	mnl_socket_close(nl);
}

