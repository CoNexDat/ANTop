#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <libmnl/libmnl.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include "rtnl.h"
#include <sys/ioctl.h>

struct mnl_socket {
	int 			fd;
	struct sockaddr_nl	addr;
};

struct in6_ifreq {
    struct in6_addr ifr6_addr;
    __u32 ifr6_prefixlen;
    unsigned int ifr6_ifindex;
};

extern int ifindex;


static int data_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
		return MNL_CB_OK;

/*
	switch(type) {
	case IFLA_MTU:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case IFLA_IFNAME:
		if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
			perror("mnl_attr_validate2");
			return MNL_CB_ERROR;
		}
		break;
	}
*/
	tb[type] = attr;
	return MNL_CB_OK;
}

static int data_cb(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[IFLA_MAX+1] = {};
	struct ifaddrmsg *ifm = mnl_nlmsg_get_payload(nlh);

/*
	fprintf(stderr,"index=%d prefix lend=%d flags=%d family=%d ",
		ifm->ifa_index, ifm->ifa_prefixlen,
		ifm->ifa_flags, ifm->ifa_family);


	if (ifm->ifa_flags & IFF_RUNNING)
		fprintf(stderr,"[RUNNING] ");
	else
		fprintf(stderr,"[NOT RUNNING] ");
*/

	mnl_attr_parse(nlh, sizeof(*ifm), data_attr_cb, tb);
/*
	if (tb[IFLA_MTU]) {
		fprintf(stderr,"mtu=%d ", mnl_attr_get_u32(tb[IFLA_MTU]));
	}
	if (tb[IFLA_IFNAME]) {
		fprintf(stderr,"name=%s ", mnl_attr_get_str(tb[IFLA_IFNAME]));
	}
*/
        if (tb[IFA_ADDRESS]) {
		const struct in6_addr *addr = malloc(sizeof(struct in6_addr));
                addr = mnl_attr_get_payload(tb[IFA_ADDRESS]);
//                fprintf(stderr,"IF Address= %s \n", ip6_to_str(*addr));                
                if(ifm->ifa_index == ifindex)
                memcpy(data,addr,sizeof(struct in6_addr));
//                free(addr);
	}

         if (tb[IFA_MULTICAST]) {
		struct in6_addr *addr = mnl_attr_get_payload(tb[IFA_MULTICAST]);
//                fprintf(stderr,"IF Multicast Address= %s \n", ip6_to_str(*addr));
	}

        if (tb[IFA_BROADCAST]) {
		struct in6_addr *addr = mnl_attr_get_payload(tb[IFA_BROADCAST]);
//                fprintf(stderr,"IF Broadcast Address= %s \n", ip6_to_str(*addr));
	}


	fprintf(stderr,"\n");
	return MNL_CB_OK;


}




int get_if_addr(struct in6_addr *if_addr){

      struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];   //(*)
	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	int ret;
	unsigned int seq, portid;



	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_GETADDR;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq = time(NULL);
	rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_INET6;
	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}
	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}
	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, data_cb, (void *)if_addr);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}
	if (ret == -1) {
		perror("error");
		exit(EXIT_FAILURE);
	}
	mnl_socket_close(nl);
	return 0;
}



int set_if_addr(struct in6_addr *if_addr){

        struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
/*
	struct rtgenmsg *rt;
*/
        struct ifaddrmsg *ifa;
	int ret;
	unsigned int seq, portid;


        fprintf(stderr, "For this interface, the address %s will be set \n", ip6_to_str(*if_addr));

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWADDR;
	nlh->nlmsg_flags = NLM_F_REPLACE;
	nlh->nlmsg_seq = seq = time(NULL);


        ifa = mnl_nlmsg_put_extra_header(nlh, sizeof(struct ifaddrmsg));

        ifa->ifa_family= AF_INET6;
        ifa->ifa_flags = IFA_F_PERMANENT;
        ifa->ifa_index = 2; //  
        ifa->ifa_prefixlen = 64;
        ifa->ifa_scope = RT_SCOPE_LINK;
/*
	rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_INET6;
*/
	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}
	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}
	portid = mnl_socket_get_portid(nl);

        mnl_attr_put(nlh, IFA_ADDRESS, sizeof(struct in6_addr), if_addr);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}
/*
	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, data_cb, (void *)if_addr);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}
*/
/*
	if (ret == -1) {
		perror("error");
		exit(EXIT_FAILURE);
	}
*/
	mnl_socket_close(nl);
	return 0;

}




int set_if_addr2(struct in6_addr *if_addr/*, int ifindex*/, int change_addr){

        struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
/*
	struct rtgenmsg *rt;
*/
        struct in6_ifreq ifr6;
        struct ifaddrmsg *ifa;
	int ret;
	unsigned int seq, portid;
        int fd;
        struct sockaddr_in6 s;


        nl = calloc(sizeof(struct mnl_socket), 1);
        
        nl->fd = socket(AF_INET6, SOCK_DGRAM, 0);
        nl->addr.nl_family = AF_INET6;
	nl->addr.nl_groups = 0;
	nl->addr.nl_pid = 0;
        

	bind(nl->fd, (struct sockaddr *) &nl->addr, sizeof (nl->addr));
        
        

        fd = nl->fd;


        get_if_addr(&(ifr6.ifr6_addr));
        ifr6.ifr6_prefixlen = 64;
        ifr6.ifr6_ifindex = ifindex;

        if(change_addr){       //Pablo Torrado --> Posible error

        if (ioctl(fd, SIOCDIFADDR, &ifr6) < 0) {
		    perror("SIOCDIFADDR");
		}
        }



        memcpy((char*)&(ifr6.ifr6_addr),(char*) if_addr, sizeof(struct in6_addr));
        fprintf(stderr, "For this interface, the address %s will be set \n", ip6_to_str(ifr6.ifr6_addr));
        ifr6.ifr6_ifindex = ifindex;  
        ifr6.ifr6_prefixlen = 64;   
        

        if (ioctl(fd, SIOCSIFADDR, &ifr6) < 0) {   //Pablo Torrado --> posible error
		    perror("SIOCSIFADDR");
		} 
	
	mnl_socket_close(nl);

	return 0;

}
