#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <linux/sockios.h>
#include <getopt.h>
#include <ctype.h>
#include <ifaddrs.h>
#define INT_MIN 1
#define INT_MAX 100
#define BUFLEN 256
#define BUFF_SIZE		(64 * 1024)
#define TEMP_SIZE		50
#define MAX_RTA 100
#include </usr/include/linux/netfilter_ipv6.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/route.h>
#include "rtnl.h"
#include "libmnl.h"
#include "rt_table.h"
#include "defs.h"
#include "utils.h"
#include "antop_packet.h"
#include "labels.h"
#include "timer_queue.h"


#define BUFSIZE 100

typedef void (*callback_func_t) (int, void*);

#define CALLBACK_FUNCS 4
static struct callback {
    int fd;
    callback_func_t func;
    void * data;
} callbacks[CALLBACK_FUNCS];

static int nr_callbacks = 0;

extern struct in6_addr prim_addr;
extern unsigned char prim_mask;
extern unsigned char red_numero;//pablo
extern int prim_addr_flag;
int ifindex;
int hc_dim;
char *univ_addr=NULL;
int socket_global;
int socket_global_mdns;
int socket_global_dns;
int pkt_socket;
int flag_init=0;//pablo6

int attach_callback_func(int fd, callback_func_t func, void *data)
{
    if (nr_callbacks >= CALLBACK_FUNCS) {
	fprintf(stderr, "callback attach limit reached!!\n");
	exit(-1);
    }
    callbacks[nr_callbacks].fd = fd;
    callbacks[nr_callbacks].func = func;
    callbacks[nr_callbacks].data = data;
    nr_callbacks++;
    return 0;
}

extern struct ipq_handle *h;

/* Here we find out how to load the kernel modules... If the modules
   are located in the current directory. use those. Otherwise fall
   back to modprobe. */





void load_modules(char *ifname)
{
    struct stat st;
    char buf[1024], *l = NULL;
    int found = 0;
    FILE *m;

    memset(buf, '\0', 64);                                                     
                                                                               
//    if (stat("./lnx/kantop.ko", &st) == 0)
	sprintf(buf,"/sbin/insmod ./lnx/kantop.ko");
                                                                                
                                                                                
//    else
//	sprintf(buf, "/sbin/modprobe kantop");

    if (system(buf) == -1) {
        fprintf(stderr, "Could not load kantop module\n");
        exit(-1);
    }

    usleep(100000);

    /* Check result */
    m = fopen("/proc/modules", "r");
    while (fgets(buf, sizeof(buf), m)) {
	l = strtok(buf, " \t");
	if (!strcmp(l, "kantop"))
	    found++;
	if (!strcmp(l, "ipchains")) {
	    fprintf(stderr,"The ipchains kernel module is loaded and prevents antop-UU from functioning properly.\n");
	    exit(-1);
	}
    }
    fclose(m);

    if (found < 1) {
	fprintf(stderr,"A kernel module could not be loaded, check your installation... %d\n",found);
	exit(-1);
    }
}

void usage(void)
{
    fprintf(stderr,"Usage: antopd -i interface -u universal_name -l hypercube_dimension\n");
    return;
}


void dns_neighbor_add()
{
    struct stat st;
    char buf[1024], *l = NULL;
    int found = 0;
    FILE *m;

    memset(buf, '\0', 64);                                                     
                                                                               
//    if (stat("./lnx/kantop.ko", &st) == 0)
	sprintf(buf,"ip neigh add 2001:db8:0:0:0:0:0:c lladdr aa:aa:aa:aa:aa:aa dev eth1");
//        sprintf(buf,"ip neigh add 2001::a lladdr bb:bb:bb:bb:bb:bb dev eth0");
                                                                                
                                                                                
//    else
//	sprintf(buf, "/sbin/modprobe kantop");

    if (system(buf) == -1) {
        fprintf(stderr, "Could not add the DNS server to the Neighbor Table\n");
        exit(-1);
    }
        sprintf(buf,"sudo sysctl net.ipv6.conf.all.forwarding=1");
        if (system(buf) == -1) {
        fprintf(stderr, "Could not enable forwarding service\n");
        exit(-1);
    }
}




int main(int argc, char **argv){


    struct in6_pktinfo {
	struct in6_addr	ipi6_addr;
	int		ipi6_ifindex;
    };

    struct in6_addr dest_addr, src_addr;           
    char buf[4096] __attribute__ ((aligned));
    struct nfq_q_handle *qh;
    int fd,rv,i,nfds,n;
    int option;
    char *ifname= NULL;
    struct timeval *timeout;

    if(argc != 4){
        fprintf(stderr,"\n");
        fprintf(stderr,"Usage: antopd interface universal_address hypercube_dimension\n");
        fprintf(stderr,"\n");
        fprintf(stderr,"Example: 'antopd eth0 user_node 5'\n");
        fprintf(stderr,"\n");
        exit(-1);
    }

    ifname = argv[1];
    if(strncmp(argv[1], "eth", 3) && strncmp(argv[1], "wlan", 4)){
        fprintf(stderr,"\n");
        fprintf(stderr,"Invalid Interface Format\n");
        fprintf(stderr,"\n");
        fprintf(stderr,"Examples: 'eth0', 'eth1', 'wlan0', ...\n");
        fprintf(stderr,"\n");
        exit(-1);
    }

    ifindex = if_nametoindex(ifname);
    if(!ifindex){
        fprintf(stderr,"\n");
        fprintf(stderr,"Invalid Interface: Correct format, but not present in system. Use 'ifconfig' for a list of available interfaces\n");
        fprintf(stderr,"\n");
        exit(-1);
    }

    univ_addr = argv[2];
    hc_dim = atoi(argv[3]);

    if((!hc_dim) || (hc_dim <0)){
        fprintf(stderr,"\n");
        fprintf(stderr,"Invalid Hypercube Dimension argument\n");
        fprintf(stderr,"\n");
        exit(-1);
    }
    
/*
    while(1){
        
        option = getopt(argc, argv, "i:u:l:");        
        if(option==EOF)
            break;
        switch(option){
            case 'i':
                ifname = optarg; 
                break;
            case 'u':
                univ_addr = optarg;
                break;

// This is the Hipercube Dimension. Normally it should be received from the node from which the address is received. If this is the first
// node in the network, then this Dimension will be use. i.e. only the first node in the network can set the Hipercube Dimension.
            case 'l':
                hc_dim = atoi(optarg);
                break;
            default :
                usage();
                exit;

        }
    }
*/

    
    fprintf(stderr,"The interface index is %d \n", ifindex);

    u_int8_t hop_l;
    hop_l = 20;
    timer_queue_init(); 

    struct in6_addr *if_addr = malloc(sizeof(struct in6_addr));

    socket_global_mdns = antop_socket_init_mdns(ifindex);

    socket_global = antop_socket_init(ifindex);
    
    socket_global_dns = antop_socket_init_dns(ifindex);
    
//    dns_neighbor_add();

    prim_addr_init();        

    get_if_addr(if_addr);

//******************************************************************************
// This is for reading the dns server address from resolv.conf
/*
    FILE *fr;            
    char line[80];
    fr = fopen ("/etc/resolv.conf", "rt");  
    
    while(fgets(line, 80, fr) != NULL)
    {
        if(!strncmp(line, "nameserver 2001", 15)){
            // We found the line of the DNS IPv6 server.
            fprintf (stderr,"%s\n", line);
        }
    }
    fclose(fr);  
*/

//******************************************************************************

    get_prim_addr(if_addr, socket_global);

    prim_addr_flag = 1;

    load_modules(ifname);  

    pkt_socket = pkt_hdlr_init();

    dns_neighbor_add();

    rt_info_init();
    
//    memcpy(&prim_addr,if_addr, sizeof(struct in6_addr));
    
    fprintf(stderr, "The primary address is %s \n", ip6_to_str(prim_addr));

    fprintf(stderr, "The universal address is %s, with length %d\n", univ_addr, strlen(univ_addr));
    
    fprintf(stderr, "The dimension is %d\n", hc_dim);
    
    heart_beat_init(&socket_global);

    fprintf(stderr, "Heart beats have already been initialized \n");

    rv_tables_init();
    
    fprintf(stderr, "Las tablas RV ya fueron inicializadas \n");

    //Register periodically to the RV server.
    timer_rv_init(socket_global); //Pablo Torrado (Aca puedo deshabilitar el timer de envios de paquetes RV)

    flag_init=1;//pablo6

    //register_rv(); //Pablo Torrado (Habilite el servicio de registracion rendez vous)
    
    // These are file descriptor sets.
    fd_set read_fd, rfds;
    // Initializes the file descriptor set fdset to have zero bits for all file descriptors.
    FD_ZERO(&read_fd);
    FD_ZERO(&rfds);
    //Sets the bit for the file descriptor socket_global in the file descriptor set read_fd. 
//    FD_SET(socket_global, &read_fd);
    
    // nfds specifies how many descriptors should be examined.
    //nfds=0;
    
    fprintf(stderr," CALLBACKS VALE: %d \n",nr_callbacks);
    
    for (i = 0; i < nr_callbacks; i++) {
	FD_SET(callbacks[i].fd, &read_fd);
	if (callbacks[i].fd >= nfds)
	    nfds = callbacks[i].fd + 1;
    }
    
    timeout = malloc(sizeof(struct timeval));  

    while(1){

    // This is neccesary because seclect() modifies rfds to indicate which socket 
    // is available for reading
        
    memcpy((char *) &rfds, (char *) &read_fd, sizeof(fd_set));

    timeout = timer_age_queue();  

    //    fprintf(stderr,"TIMEOUT MICRO: %d \n  ",(int)timeout->tv_usec);
//    fprintf(stderr,"Seconds left for timeout: %d \n  ",(int)timeout->tv_sec);

    if ((n = select(nfds, &rfds, NULL, NULL, timeout)) < 0) {

		fprintf(stderr,"main.c: Failed select (main loop) \n");
    }

//    fprintf(stderr,"Main Loop \n");
    
    //FD_ISSET Returns a non-zero value if the bit for the file descriptor fd is set in the
    //file descriptor set pointed to by fdset, and 0 otherwise.

    if (n > 0) {
        
//        fprintf(stderr,"N ES MAYOR A CERO !!!!! \n");
        for (i = 0; i < nr_callbacks; i++) {
		if (FD_ISSET(callbacks[i].fd, &rfds)) {
/*
                    if(callbacks[i].fd==socket_global_mdns)                        
                        fprintf(stderr,"ESTA ACTIVADO EL BIT DE CALLBACK para el socket ANTOP MDNS \n");
                    else if(socket_global== callbacks[i].fd)    
                        fprintf(stderr,"ESTA ACTIVADO EL BIT DE CALLBACK para el socket ANTOP\n");
                    else if(callbacks[i].fd == pkt_socket)
                        fprintf(stderr,"ESTA ACTIVADO EL BIT DE CALLBACK para el socket PKT_HANDLER\n");
*/
                    (*callbacks[i].func) (callbacks[i].fd, callbacks[i].data);
                    
		}
//                else
  //                  fprintf(stderr,"NO ESTA ACTIVADO EL BIT DE CALLBACK para la función n %d \n",callbacks[i].fd);
	    }
	}
    }

    return 0;

    


}



  






 



