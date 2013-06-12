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

static int data_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);
	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
		return MNL_CB_OK;
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
	tb[type] = attr;
	return MNL_CB_OK;
}


static int data_cb(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[IFLA_MAX+1] = {};
	struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);

	fprintf(stderr,"index=%d type=%d flags=%d family=%d ",
		ifm->ifi_index, ifm->ifi_type,
		ifm->ifi_flags, ifm->ifi_family);

	if (ifm->ifi_flags & IFF_RUNNING)
		fprintf(stderr,"[RUNNING] ");
	else
		fprintf(stderr,"[NOT RUNNING] ");

	mnl_attr_parse(nlh, sizeof(*ifm), data_attr_cb, tb);
	if (tb[IFLA_MTU]) {
		fprintf(stderr,"mtu=%d ", mnl_attr_get_u32(tb[IFLA_MTU]));
	}
	if (tb[IFLA_IFNAME]) {
		fprintf(stderr,"name=%s ", mnl_attr_get_str(tb[IFLA_IFNAME]));
	}
/*
        if (tb[IFLA_ADDRESS]) {

         * MAC ADDRESS
         *
         *
	}
*/
	fprintf(stderr,"\n");
	return MNL_CB_OK;

        
}




int get_if(){

      struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	int ret;
	unsigned int seq, portid;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_GETLINK;
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
		ret = mnl_cb_run(buf, ret, seq, portid, data_cb, NULL);
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
