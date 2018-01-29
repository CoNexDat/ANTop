#include <libmnl/libmnl.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "antop_packet.h"
#include "labels.h"
#include "defs.h"
#include "timer_queue.h"
#include <sys/socket.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "internal.h"
#include <sys/types.h>


struct mnl_socket {
	int 			fd;
	struct sockaddr_nl	addr;
};

struct in6_ifreq {
    struct in6_addr ifr6_addr;
    __u32 ifr6_prefixlen;
    unsigned int ifr6_ifindex;
};


extern struct in6_addr prim_addr;
extern struct in6_addr sec_addr;
extern unsigned char prim_mask;
extern unsigned char sec_mask;
unsigned char red_numero;
extern int hc_dim;
extern int socket_global;
extern int ifindex;
int flag_mezcla;

//Esta funcion lo que hace es devolverte el quinto octeto del la direccion mac para identificar la red en la que pertenece cada nodo
int mac_address(){

int fd,i;
struct ifreq ifr;
char *iface="wlan0";
unsigned char *mac;

fd= socket(AF_INET,SOCK_DGRAM,0);

ifr.ifr_addr.sa_family=AF_INET;
strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);

ioctl(fd, SIOCGIFHWADDR, &ifr);

close(fd);

mac=(unsigned char *)ifr.ifr_hwaddr.sa_data;

i=mac[4];

return i;
}



//FUNCION BORRAR LA DIRECCION IPV6 ACUTAL Y AGREGAR UNA NUEVA(addnew)

int del_and_addnew (struct in6_addr *addnew){

	int fd;
	struct mnl_socket *nl;
	nl = calloc(sizeof(struct mnl_socket),1);
	
	nl->fd = socket(AF_INET6, SOCK_DGRAM, 0);
        nl->addr.nl_family = AF_INET6;
	nl->addr.nl_groups = 0;
	nl->addr.nl_pid = 0;	

	bind(nl->fd, (struct sockaddr *) &nl->addr, sizeof (nl->addr));

	fd = nl->fd;

	struct in6_ifreq *aux1=malloc(sizeof(struct in6_ifreq));	
	get_if_addr(&(aux1->ifr6_addr));        
	aux1->ifr6_prefixlen=64;
        aux1->ifr6_ifindex=ifindex; 
	if (ioctl(fd, SIOCDIFADDR, aux1) < 0) {
		perror("SIOCDIFADDR");
	}
	
	
	struct in6_ifreq *aux2=malloc(sizeof(struct in6_ifreq));
        memcpy(&(aux2->ifr6_addr),addnew, sizeof(struct in6_addr));	
        aux2->ifr6_prefixlen=64;
        aux2->ifr6_ifindex=ifindex;
        if (ioctl(fd, SIOCSIFADDR, aux2) < 0) { 
		 perror("SIOCSIFADDR");
	} 


	mnl_socket_close(nl);

        return 0;
}












int mezcla(struct in6_addr *dst){   //se utiliza en la funcion "process_hb()" dle archivo "hear_beat.c"

	printf("\n\n\n\n\n****************************************\n");
	printf("SE DETECTO MEZCLA CON OTRA RED ANTop\n");
	printf("****************************************\n");

	flag_mezcla=1;

//------------------------------------------------------------------------
// ACA EN DONDE BORRO LA DIRECCION ACTUAL Y ESTABLEZCO UNA DEFAULT 2001::a
//------------------------------------------------------------------------
	
	struct in6_addr *addnew=malloc(sizeof(struct in6_addr));
	memset(addnew,0,sizeof(struct in6_addr));
        addnew->s6_addr16[0] = 0x0120;
        addnew->s6_addr16[7] = 0x0a00;	
	del_and_addnew(addnew);
//-----------------------------------------------------------------------


//------------------------------------------------------------------------
// ACA SE EMPIEZA EL PROCESOS DE ASIGNACION DE DIRECCION CON LA NUEVA RED
//------------------------------------------------------------------------
	
	
	fprintf(stderr,"Enviando PAR a la direccion:  %s\n",ip6_to_str(*dst)); 
	sleep(2);

        send_par(addnew,socket_global,dst,1);
	
	return;
}





int confirmacion(unsigned char *antop_msg) {

struct ctrl_pkt *antop_msg_aux;

antop_msg_aux=(struct ctrl_pkt *)antop_msg;



prim_mask=antop_msg_aux->ad_addr.mask;
red_numero=antop_msg_aux->ad_addr.red_numero;

printf("Se procesa el paquete PAP");
printf("\nMask: %d\n", prim_mask);
printf("Numero de red: %d\n", red_numero);
fprintf(stderr,"Se adquiere la nueva direccion:  %s\n",ip6_to_str(antop_msg_aux->ad_addr.addr)); 

send_pan(&(antop_msg_aux->prim_addr),socket_global,&(antop_msg_aux->ad_addr.addr));

memcpy(&prim_addr,&(antop_msg_aux->ad_addr.addr),sizeof(struct in6_addr));
fprintf(stderr,"prim_addr= %s\n", ip6_to_str(prim_addr));

del_and_addnew(&(antop_msg_aux->ad_addr.addr));


//getchar();
return;
}







