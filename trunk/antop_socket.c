#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <stdio.h>
#define __USE_GNU
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <netinet/ip.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include "rtnl.h"
#include "antop_packet.h"
#include "labels.h"

extern int ifindex;



#define SO_RECVBUF_SIZE 256*1024

#define RV_ANTOP_PORT 4470
#define RECV_BUF_SIZE 1024 

int antop_socket_read(int fd, unsigned char **antop_msg);

extern struct in6_addr prim_addr;

int  antop_socket_init(int ifindex)
{
    struct ctrl_pkt * antop_msg;
    antop_msg = malloc(sizeof(struct ctrl_pkt));
    struct sockaddr_in6 antop_addr;
    struct ifreq ifr;
    int i, retval = 0;
    int on = 1;  
    int bufsize = SO_RECVBUF_SIZE;

    //For IPv6 Multicast
    struct ipv6_mreq mreq6;
    int if_index;
    int sock;
    struct in6_pktinfo pkt_info;
    struct in6_addr if_address = IN6ADDR_ANY_INIT;;
    char aux[5];

    char *ifname = &aux; 
    memset(ifname, 0, 5);
    ifname = if_indextoname(ifindex, ifname);
   
    /* Create a UDP socket */
    
    sock = socket(PF_INET6, SOCK_DGRAM, 0);

    if (sock < 0) {
        perror("antop_socket_init: ");
	exit(-1);
    }

    /* Bind the socket to the ANTOP port number */
    memset(&antop_addr, 0, sizeof(antop_addr));

    antop_addr.sin6_family = AF_INET6;
    antop_addr.sin6_port = htons(ANTOP_PORT);
    antop_addr.sin6_addr = in6addr_any;

    retval = bind(sock, (struct sockaddr_in6 *) &antop_addr, sizeof(antop_addr));

    if (retval < 0) {
        perror("antop_socket_init: Bind failed \n");
	exit(-1);
    }
        //IPv6 doesn't have broadcast

        mreq6.ipv6mr_multiaddr.s6_addr16[0]=0x02ff;
        mreq6.ipv6mr_multiaddr.s6_addr16[1]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[2]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[3]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[4]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[5]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[6]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[7]=0x0100;

	mreq6.ipv6mr_interface = ifindex;
	if_index = ifindex;

	//debug - For testing purpose
	/*
	printf("DEV_NR(%d).ifindex = %d\nMulticast Address = ", i, if_index);
	print_ipv6_addr(DEV_NR(i).multicast);
	printf("DEV_NR(%d).name = %s\n", i, DEV_NR(i).ifname);
	*/

	if(setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6))<0)
	  {
	    fprintf(stderr, "antop_socket_init: IPV6_ADD_MEMBERSHIP failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }

	if(setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_index, sizeof(if_index))<0)
	  {
	    fprintf(stderr, "antop_socket_init: IPV6_MULTICAST_IF failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }

        on = 0;

        if(setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(int))<0)
	  {
	    fprintf(stderr, "antop_socket_init: IPV6_MULTICAST_LOOP failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }

	memset(&ifr, 0, sizeof(struct ifreq));
  	strcpy(ifr.ifr_name, ifname);


/*
	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &if_index, sizeof(if_index)) < 0) {
	    fprintf(stderr,"antop_socket_init: SO_BINDTODEVICE failed for %s","eth0");
	    perror(" ");
	    exit(-1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_PRIORITY, &tos, sizeof(int)) < 0) {
	    perror("antop_socket_init: Setsockopt SO_PRIORITY failed ");
	    exit(-1);
	}
*/
        get_if_addr(&if_address);
        pkt_info.ipi6_addr = if_address;
        pkt_info.ipi6_ifindex = ifindex; 

	if (setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO, &pkt_info, sizeof(struct in6_pktinfo)) < 0) {
	    perror("antop_socket_init: Setsockopt IPV6_PKTINFO failed ");
	    exit(-1);
	}

        on=1;

        if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(int)) < 0) {
	    perror("antop_socket_init: Setsockopt IPV6_RECVPKTINFO failed ");
	    exit(-1);
	}

	//IPv6 has HOP_LIMIT instead of TTL
/*
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_HOPLIMIT, &on, sizeof(int)) < 0) {
	    perror("antop_socket_init: Setsockopt IPV6_HOPLIMIT failed ");
	    exit(-1);
	}
*/

	/* Set max allowable receive buffer size... */
/*
	for (;; bufsize -= 1024) {
	    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) == 0) {
		break;
	    }
	    if (bufsize < RECV_BUF_SIZE) {
		exit(-1);
	    }
	}
*/
	retval = attach_callback_func(sock, antop_socket_read, &antop_msg);
//        retval = attach_callback_func(sock, antop_socket_read, NULL);
	if (retval < 0) {
	    perror("antop_socket_init: register input handler failed ");
	    exit(-1);
	}
    
        return sock;
}

void antop_socket_send(unsigned char *antop_msg, struct in6_addr dst, int len, u_int8_t hop_limit, int sock, int port)
{
    int retval = 0;
    struct sockaddr_in6 dst_addr;
    int hop_limit_send = 0;
    struct iovec iov[1];
    struct msghdr msg;
    struct cmsghdr *cmsg = NULL;
    int cmsglen = 0;
    struct in6_pktinfo pinfo;
    struct in6_addr if_addr = IN6ADDR_ANY_INIT;;

    memset(&msg, 0, sizeof(msg));

    
    get_if_addr(&if_addr);
    fprintf(stderr, "%s \n", ip6_to_str(if_addr));
    hop_limit_send = hop_limit;
/*
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &hop_limit_send, sizeof(hop_limit_send)) == -1)
      {
	fprintf(stderr,"setsockopt IPV6_MULTICAST_HOPS");
	return;
      }
    
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (char *) &hop_limit_send, sizeof(hop_limit_send)) == -1)
      {
	fprintf(stderr,"setsockopt IPV6_UNICAST_HOPS");
	return;
      }
*/
    memset(&dst_addr, 0, sizeof(dst_addr));

    dst_addr.sin6_family = AF_INET6;
    copy_in6_addr(&(dst_addr.sin6_addr), &dst);
//    dst_addr.sin6_port = htons(ANTOP_PORT);
    dst_addr.sin6_port = htons(port);


    //use sendmsg instead of sendto
    memset(&pinfo, 0, sizeof(pinfo));
    memcpy(&pinfo.ipi6_addr, &if_addr, sizeof(struct in6_addr));
    
    
    pinfo.ipi6_ifindex = ifindex;         
  
    cmsglen = CMSG_SPACE(sizeof(pinfo));
    cmsg = malloc(cmsglen);
    if (cmsg == NULL) {
      perror(__FUNCTION__);
      return;
    }
    cmsg->cmsg_len = CMSG_LEN(sizeof(pinfo));
    cmsg->cmsg_level = IPPROTO_IPV6;
    cmsg->cmsg_type = IPV6_PKTINFO;
    memcpy(CMSG_DATA(cmsg), &pinfo, sizeof(pinfo));

    if(((*antop_msg) > MAX_CTRL_TYPE))
        iov[0].iov_base = (unsigned char *)antop_msg;
    else
        iov[0].iov_base = (struct ctrl_pkt *)antop_msg;

    iov[0].iov_len = len;    

    msg.msg_control = cmsg;
    msg.msg_controllen = cmsglen;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = (void *)&dst_addr;
    msg.msg_namelen = sizeof(dst_addr);


	retval = sendmsg(sock, &msg, 0);
	
	    
	if (retval < 0) {	  

	    fprintf(stderr,"antop_socket_send (UNICAST): Failed send to %s \n", ip6_to_str(dst));
            fprintf(stderr,"The error code is %d", retval);

	    return;
	} else {
	    fprintf(stderr,"antop_socket_send (UNICAST): Ok sent to %s \n", ip6_to_str(dst));
	}

        

}

int antop_process_pkt(int fd, unsigned char *antop_msg){


    unsigned char aux;
    aux=*antop_msg;
    struct ctrl_pkt *hb;

    switch (aux){

        case PAR:
            fprintf(stderr,"se recibio un paquete de tipo PAR \n");
            send_pap(fd, ((struct ctrl_pkt *)antop_msg)->src_addr);
            break;

        case PAP:
            fprintf(stderr,"se recibio un paquete de tipo PAP \n");
            break;

        case PAN:
            fprintf(stderr,"se recibio un paquete de tipo PAN \n");
            fprintf(stderr, "PPPPANC enviara a %s\n",ip6_to_str(((struct ctrl_pkt *)antop_msg)->src_addr));
            if(memcmp(&prim_addr, &(((struct ctrl_pkt *)antop_msg)->ad_addr.addr), sizeof(struct in6_addr)) == 0){
                send_panc(fd, ((struct ctrl_pkt *)antop_msg)->src_addr, ((struct ctrl_pkt *)antop_msg)->ad_addr.addr, ((struct ctrl_pkt *)antop_msg)->prim_addr);
            }
            break;
        case HB:
            fprintf(stderr, "Se recibio un HB \n");
            process_hb(((struct ctrl_pkt *)antop_msg));
            

            break;
        case RV_REG:
            fprintf(stderr,"Se recibio un paquete de tipo RV_REG\n");
            rv_table_add((struct rv_ctrl_pkt *)antop_msg);
            break;

        case RV_ADDR_LOOKUP:
            fprintf(stderr,"A request to solve a name has been received, RV_ADDR_LOOKUP packet\n");
            answer_rv_query((struct ctrl_pkt *)antop_msg, fd);
            break;

        case RV_ADDR_SOLVE:
            fprintf(stderr,"A RV query response has been received\n");                       

            // In this case the second and third parameter (destintation port and packet size)
            // are zero, we will get this values from que dns query queue.
            // We will compare the universal address in antop_msg with the ones in the queue, so we know "who"
            // to reply to.

            // In this case, the first param is the RV_ADDR_SOLVE pkt

            if(((struct rv_ctrl_pkt *)antop_msg)->flags & MASK_SOLVED){
            // This means the RV server resolved our query succesfully
                fprintf(stderr,"Our query has been resolved\n");
                answer_dns_query(antop_msg, 0, 0, 1, 0);
            }else{
                fprintf(stderr,"Our query hasn't been resolved\n");
                answer_dns_query(NULL, 0, 0, 0, 0);
            }

            break;

        case SAP:
            fprintf(stderr,"A secondary address proposal has been received\n");
            process_sap((struct ctrl_pkt *)antop_msg);
            break;

        case SAN:
            fprintf(stderr,"A SAN packet has been received\n");
            process_san((struct ctrl_pkt *)antop_msg);
    }
    return 0;
}


int antop_socket_read(int fd, unsigned char **antop_msg)
{

    struct in6_addr src = IN6ADDR_ANY_INIT;
    struct in6_addr dst = IN6ADDR_ANY_INIT;
    int i, len, hop_limit = 0, moveon = 0;      
    struct sockaddr_in6 src_addr;
    unsigned char buffer[IP_MAXPACKET];
    unsigned char msgbuffer[1024];
    struct iovec iov = { (void *)&buffer, IP_MAXPACKET };
    struct sockaddr_in6 addr;
    struct msghdr msg = { (void *)&addr, sizeof(addr), &iov, 1, (void *)&msgbuffer, sizeof(msgbuffer), 0 };
    struct cmsghdr *cmsg;
    struct in6_pktinfo pktinfo;

    fprintf(stderr, "ANTOP SOCKET READ \n");
    
    
    /* Get the information control message first */
    if ((len = recvmsg(fd, &msg, 0)) < 0) {
	fprintf(stderr, "antop_socket_read: recvmsg ERROR! \n");
	return -1;
    }         

    memcpy(&src_addr, msg.msg_name, sizeof(struct sockaddr_in6));    
    *antop_msg = (unsigned char *) msg.msg_iov->iov_base;
    copy_in6_addr(&src, &src_addr.sin6_addr);   

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
      {

	if (cmsg->cmsg_type == IPV6_HOPLIMIT)
	  {
	    hop_limit = (int) *CMSG_DATA(cmsg);
	  }
	if (cmsg->cmsg_type == IPV6_PKTINFO)
	  {           
            memcpy(&pktinfo, CMSG_DATA(cmsg), sizeof(pktinfo));            
	    moveon = 1;
	  }
      }

    copy_in6_addr(&dst, &pktinfo.ipi6_addr);

    if(moveon)
      {
        antop_process_pkt(fd, *antop_msg);
        fprintf(stderr,"The pkt will be processed \n");
      }
    else
      {
	fprintf(stderr,"cannot retrieve cmsg correct,");
	fprintf(stderr,"will not proceed with antop_socket_process_packet.\n");
      }
    return 0;

}


int  antop_socket_init_mdns(int ifindex)
{
    struct ctrl_pkt * antop_msg;
    antop_msg = malloc(sizeof(struct ctrl_pkt));
    struct sockaddr_in6 antop_addr;
    struct ifreq ifr;
    int i, retval = 0;
    int on = 1;
    int bufsize = SO_RECVBUF_SIZE;

    //For IPv6 Multicast
    struct ipv6_mreq mreq6;
    int if_index;
    int sock;
    struct in6_pktinfo pkt_info;
    struct in6_addr if_address = IN6ADDR_ANY_INIT;

    char aux[5];

    char *ifname = &aux;
    memset(ifname, 0, 5);

    ifname = if_indextoname(ifindex, ifname);

    /* Create a UDP socket */

    sock = socket(PF_INET6, SOCK_DGRAM, 0);


	if (sock < 0) {
	    perror("antop_socket_init_mdns: ");
	    exit(-1);
	}

	
	memset(&antop_addr, 0, sizeof(antop_addr));

        // Here an aux port is registered. Later we will change the port to be
        // a MDNS source port, in a hook.

	antop_addr.sin6_family = PF_INET6;
	antop_addr.sin6_port = htons(MDNS_PORT_AUX);
	antop_addr.sin6_addr = in6addr_any; //Doesn't need to do htonl, all 0
	retval = bind(sock, (struct sockaddr_in6 *) &antop_addr, sizeof(antop_addr));

	if (retval < 0) {
	    perror("antop_socket_init_mdns: Bind failed \n");
	    exit(-1);
	}



        //PL: IPv6 doesn't have broadcast


        mreq6.ipv6mr_multiaddr.s6_addr16[0]=0x02ff;
        mreq6.ipv6mr_multiaddr.s6_addr16[1]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[2]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[3]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[4]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[5]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[6]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[7]=0x0100;

//	memcpy(&mreq6.ipv6mr_multiaddr, &(DEV_NR(i).multicast), sizeof(struct in6_addr));
	//copy_in6_addr(&mreq6.ipv6mr_multiaddr, &(DEV_NR(i).multicast));
	mreq6.ipv6mr_interface = ifindex;
	if_index = ifindex;

	//debug - For testing purpose
	/*
	printf("DEV_NR(%d).ifindex = %d\nMulticast Address = ", i, if_index);
	print_ipv6_addr(DEV_NR(i).multicast);
	printf("DEV_NR(%d).name = %s\n", i, DEV_NR(i).ifname);
	*/

	if(setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6))<0)
	  {
	    fprintf(stderr, "antop_socket_init_mdns: IPV6_ADD_MEMBERSHIP failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }

	if(setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_index, sizeof(if_index))<0)
	  {
	    fprintf(stderr, "antop_socket_init_mdns: IPV6_MULTICAST_IF failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }

        on = 0;

        if(setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(int))<0)
	  {
	    fprintf(stderr, "antop_socket_init_mdns: IPV6_MULTICAST_LOOP failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }



	memset(&ifr, 0, sizeof(struct ifreq));
  	strcpy(ifr.ifr_name, ifname);



/*
	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &if_index, sizeof(if_index)) < 0) {
	    fprintf(stderr,"antop_socket_init_mdns: SO_BINDTODEVICE failed for %s","eth0");
	    perror(" ");
	    exit(-1);
	}
*/
/*
	if (setsockopt(sock, SOL_SOCKET, SO_PRIORITY, &tos, sizeof(int)) < 0) {
	    perror("antop_socket_init: Setsockopt SO_PRIORITY failed ");
	    exit(-1);
	}
*/
/*
        if (setsockopt(sock, SOL_IP, IP_HDRINCL,&on, sizeof(int)) < 0) {
	    perror("antop_socket_init_mdns: Setsockopt IP_HDRINCL failed ");
	    exit(-1);
        }
*/



        get_if_addr(&if_address);
        pkt_info.ipi6_addr = if_address;
        pkt_info.ipi6_ifindex = ifindex;

/*
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO, &pkt_info, sizeof(struct in6_pktinfo)) < 0) {
	    perror("antop_socket_init_mdns: Setsockopt IPV6_PKTINFO failed ");
	    exit(-1);
	}
*/

        on=1;

/*
        if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(int)) < 0) {
	    perror("antop_socket_init_mdns: Setsockopt IPV6_RECVPKTINFO failed ");
	    exit(-1);
	}
*/



	//IPv6 has HOP_LIMIT instead of TTL
/*
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_HOPLIMIT, &on, sizeof(int)) < 0) {
	    perror("antop_socket_init: Setsockopt IPV6_HOPLIMIT failed ");
	    exit(-1);
	}
*/

	/* Set max allowable receive buffer size... */
/*
	for (;; bufsize -= 1024) {
	    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) == 0) {
		break;
	    }
	    if (bufsize < RECV_BUF_SIZE) {
		exit(-1);
	    }
	}
*/

  	retval = attach_callback_func(sock, antop_socket_read, &antop_msg);
        

	if (retval < 0) {
	    perror("antop_socket_init_mdns: register input handler failed ");
	    exit(-1);
	}

        return sock;
}


int  antop_socket_init_dns(int ifindex)
{
    struct ctrl_pkt * antop_msg;
    antop_msg = malloc(sizeof(struct ctrl_pkt));
    struct sockaddr_in6 antop_addr;
    struct ifreq ifr;
    int i, retval = 0;
    int on = 1;
    int bufsize = SO_RECVBUF_SIZE;

    //For IPv6 Multicast
    struct ipv6_mreq mreq6;
    int if_index;
    int sock;
    struct in6_pktinfo pkt_info;
    struct in6_addr if_address = IN6ADDR_ANY_INIT;

    char aux[5];

    char *ifname = &aux;
    memset(ifname, 0, 5);

    ifname = if_indextoname(ifindex, ifname);

    /* Create a UDP socket */

    sock = socket(PF_INET6, SOCK_DGRAM, 0);


	if (sock < 0) {
	    perror("antop_socket_init_mdns: ");
	    exit(-1);
	}

	
	memset(&antop_addr, 0, sizeof(antop_addr));

        // Here an aux port is registered. Later we will change the port to be
        // a MDNS source port, in a hook.

	antop_addr.sin6_family = PF_INET6;
	antop_addr.sin6_port = htons(DNS_PORT_AUX);
	antop_addr.sin6_addr = in6addr_any; //Doesn't need to do htonl, all 0
	retval = bind(sock, (struct sockaddr_in6 *) &antop_addr, sizeof(antop_addr));

	if (retval < 0) {
	    perror("antop_socket_init_mdns: Bind failed \n");
	    exit(-1);
	}



        //PL: IPv6 doesn't have broadcast


        mreq6.ipv6mr_multiaddr.s6_addr16[0]=0x02ff;
        mreq6.ipv6mr_multiaddr.s6_addr16[1]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[2]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[3]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[4]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[5]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[6]=0x0000;
        mreq6.ipv6mr_multiaddr.s6_addr16[7]=0x0100;

//	memcpy(&mreq6.ipv6mr_multiaddr, &(DEV_NR(i).multicast), sizeof(struct in6_addr));
	//copy_in6_addr(&mreq6.ipv6mr_multiaddr, &(DEV_NR(i).multicast));
	mreq6.ipv6mr_interface = ifindex;
	if_index = ifindex;

	//debug - For testing purpose
	/*
	printf("DEV_NR(%d).ifindex = %d\nMulticast Address = ", i, if_index);
	print_ipv6_addr(DEV_NR(i).multicast);
	printf("DEV_NR(%d).name = %s\n", i, DEV_NR(i).ifname);
	*/

	if(setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6))<0)
	  {
	    fprintf(stderr, "antop_socket_init_mdns: IPV6_ADD_MEMBERSHIP failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }

	if(setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_index, sizeof(if_index))<0)
	  {
	    fprintf(stderr, "antop_socket_init_mdns: IPV6_MULTICAST_IF failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }

        on = 0;

        if(setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(int))<0)
	  {
	    fprintf(stderr, "antop_socket_init_mdns: IPV6_MULTICAST_LOOP failed for %s", ifname);
	    perror(" ");
	    exit(-1);
	  }



	memset(&ifr, 0, sizeof(struct ifreq));
  	strcpy(ifr.ifr_name, ifname);



/*
	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &if_index, sizeof(if_index)) < 0) {
	    fprintf(stderr,"antop_socket_init_mdns: SO_BINDTODEVICE failed for %s","eth0");
	    perror(" ");
	    exit(-1);
	}
*/
/*
	if (setsockopt(sock, SOL_SOCKET, SO_PRIORITY, &tos, sizeof(int)) < 0) {
	    perror("antop_socket_init: Setsockopt SO_PRIORITY failed ");
	    exit(-1);
	}
*/
/*
        if (setsockopt(sock, SOL_IP, IP_HDRINCL,&on, sizeof(int)) < 0) {
	    perror("antop_socket_init_mdns: Setsockopt IP_HDRINCL failed ");
	    exit(-1);
        }
*/



        get_if_addr(&if_address);
        pkt_info.ipi6_addr = if_address;
        pkt_info.ipi6_ifindex = ifindex;

/*
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO, &pkt_info, sizeof(struct in6_pktinfo)) < 0) {
	    perror("antop_socket_init_mdns: Setsockopt IPV6_PKTINFO failed ");
	    exit(-1);
	}
*/

        on=1;

/*
        if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(int)) < 0) {
	    perror("antop_socket_init_mdns: Setsockopt IPV6_RECVPKTINFO failed ");
	    exit(-1);
	}
*/



	//IPv6 has HOP_LIMIT instead of TTL
/*
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_HOPLIMIT, &on, sizeof(int)) < 0) {
	    perror("antop_socket_init: Setsockopt IPV6_HOPLIMIT failed ");
	    exit(-1);
	}
*/

	/* Set max allowable receive buffer size... */
/*
	for (;; bufsize -= 1024) {
	    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) == 0) {
		break;
	    }
	    if (bufsize < RECV_BUF_SIZE) {
		exit(-1);
	    }
	}
*/

  	retval = attach_callback_func(sock, antop_socket_read, &antop_msg);
        

	if (retval < 0) {
	    perror("antop_socket_init_mdns: register input handler failed ");
	    exit(-1);
	}

        return sock;
}

