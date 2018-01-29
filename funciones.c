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

//DIRECCION PRIIMARIA
extern struct in6_addr prim_addr;
extern unsigned char prim_mask;
struct in6_addr prim_addr_aux;  //es un auxiliar de prim_addr, que lo utilizo para guardar la direccion orginal prim_addr cuando esta cambia por algo

//DIRECCION SECUNDARIA
extern struct in6_addr sec_addr;
extern unsigned char sec_mask;
extern int sec_addr_flag;

//VECINOS
extern struct neighbor *neigh_table;
extern struct neighbor_to_root neighbor_to_root;

//RUTAS
extern struct route *route_t;

//OTROS
extern int hc_dim;  //dimension de la red, tercer parametro (ejemplo: wlan0 nodo1 "64")
extern int socket_global;
extern int ifindex;
extern struct in6_addr root;
extern struct in6_addr dir_default;//direccion default 2001::a
extern unsigned char red_numero;
extern int flag_conexion;//pablo6
extern int flag_init;//pablo6

//FRAGMENTACION
extern int flag_fragmentacion;
extern int flag_fragmentacion_primaria;
extern int flag_fragmentacion_secundaria;
extern unsigned char prim_mask_inicial; // es el valor del prim_mask incial, antes de que asigne o ceda espacio de direcciones a otros, es el prim_mask virgen de cada nodo
extern unsigned char sec_mask_inicial; //iden prim_mas_frag, pero para sec_mask

//MEZCLA
extern int flag_mezcla;
extern int flag_mezcla_primaria;
extern int flag_mezcla_secundaria;
extern unsigned char prim_mask_mezcla;  //es el valor de shift de offset que se va a propagar en la rama de abajo del nodo de mezcla

//PROPUESTA DIRECCION SECUNDARIA
//Del lado del que genera el SAP
extern struct in6_addr addr_node_secondary_link;
//Del lado del que recibe el SAP
extern int flag_neigh_by_second;  //este flag se activa cuando un nodo tiene vecinos por enlaces secundario, es decir que recibio algun paquete SAP 

//RECUPERACION DE DIRECCIONES DEL ESPACIO
extern struct recovered_addr *rcvd_addr;


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
i=i+50;
return i;
}



//FUNCION PARA AGREGAR UNA DIRECCION IPV6 "add"
int add (struct in6_add *add){

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
	memcpy(&(aux1->ifr6_addr),add,sizeof(struct in6_addr));
	aux1->ifr6_prefixlen=64;
        aux1->ifr6_ifindex=ifindex; 
	if (ioctl(fd, SIOCSIFADDR, aux1) < 0) {
		perror("SIOCSIFADDR");
	}
	
	mnl_socket_close(nl);

	free(aux1);

        return 0;
}



//FUNCION PARA BORRAR LA DIRECCION IPV6 "del"
int del (struct in6_add *del){

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
	memcpy(&(aux1->ifr6_addr),del,sizeof(struct in6_addr));
	aux1->ifr6_prefixlen=64;
        aux1->ifr6_ifindex=ifindex; 
	if (ioctl(fd, SIOCDIFADDR, aux1) < 0) {
		perror("SIOCDIFADDR");
	}
	

	mnl_socket_close(nl);

	free(aux1);

        return 0;
}




//FUNCION PARA BORRAR LA DIRECCION IPV6 "del" Y AGREGAR UNA NUEVA "addnew"
int del_and_addnew (struct in6_add *delold, struct in6_addr *addnew, int flag){

	
if(flag==0){ //cuida la direccion primaria
	if(!memcmp(addnew,&prim_addr,sizeof(struct in6_addr)) ){
		del(delold);
		return 0; 
			}
	if(!memcmp(delold,&prim_addr,sizeof(struct in6_addr)) ){
		add(addnew);
		return 0; 
			}
	del(delold);
	add(addnew);
	return 0;
	}

if(flag==1){ //cuida la direccion secundaria
	if(!memcmp(addnew,&sec_addr,sizeof(struct in6_addr)) ){
		del(delold);
		return 0; 
			}
	if(!memcmp(delold,&sec_addr,sizeof(struct in6_addr)) ){
		add(addnew);
		return 0; 
			}
	del(delold);
	add(addnew);
	return 0;
	}

if(flag==2){
	del(delold);
	add(addnew);
	return 0;
	}

}


void shift_unos(int izq_der, int cantidad, struct in6_addr *addr){

int i, j , carry1, carry2;

struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
struct in6_addr *aux2=malloc(sizeof(struct in6_addr));
struct in6_addr *result=malloc(sizeof(struct in6_addr));

memcpy(result,addr,sizeof(struct in6_addr));


//Intercambio de lugar los bytes de cada dato de ..s6_addr16[i] para realizar desplazamiento
for(i=0;i<8;i++){
aux1->s6_addr16[i]=result->s6_addr16[i] & 0xff00;
aux2->s6_addr16[i]=result->s6_addr16[i] & 0x00ff;
aux1->s6_addr16[i]=aux1->s6_addr16[i]>>8;
aux2->s6_addr16[i]=aux2->s6_addr16[i]<<8;
result->s6_addr16[i]=aux1->s6_addr16[i] + aux2->s6_addr16[i];
}
//fprintf(stderr,"DOS= %s\n", ip6_to_str(*result));

//Realizo el shift
//A izquierda
if(izq_der==1){
for(j=0;j<cantidad;j++){
for(i=7,carry1=0,carry2=0;i>=0;i--){
carry1=carry2;
if((result->s6_addr16[i]& 0x8000)==0x0000)
carry2=0;
else 
carry2=1;
result->s6_addr16[i]=result->s6_addr16[i]<<1;
if(carry1==1)
result->s6_addr16[i]=result->s6_addr16[i] | 0x0001;
}
}
}
//A derecha
if(izq_der==0){
for(j=0;j<cantidad;j++){
for(i=0,carry1=0,carry2=0;i<=7;i++){
carry1=carry2;
if((result->s6_addr16[i]& 0x0001)==0x0000)
carry2=0;
else 
carry2=1;
result->s6_addr16[i]=result->s6_addr16[i]>>1;
if(carry1==1)
result->s6_addr16[i]=result->s6_addr16[i] | 0x8000;
}
}
}


//Vuelvo a dar vuelta los bytes de datos s6_addr16[i]
for(i=0;i<8;i++){
aux1->s6_addr16[i]=result->s6_addr16[i] & 0xff00;
aux2->s6_addr16[i]=result->s6_addr16[i] & 0x00ff;
aux1->s6_addr16[i]=aux1->s6_addr16[i]>>8;
aux2->s6_addr16[i]=aux2->s6_addr16[i]<<8;
result->s6_addr16[i]=aux1->s6_addr16[i] + aux2->s6_addr16[i];
}



memcpy(addr,result,sizeof(struct in6_addr));

return;

}



//FUNCION SHIFT, el desplazamiento se produce en el "struct in6_addr *addr"
void shift(int izq_der, int cantidad, struct in6_addr root_addr, struct in6_addr *addr){  //el "addr" es la direccion del in6_addr que se va realizar el desplazamiento

//izq_der   0 se desplaza hacia la derecha/ 1 se desplaza hacia la izquierda

int i,j,carry1,carry2;

struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
struct in6_addr *aux2=malloc(sizeof(struct in6_addr));
struct in6_addr *mask=malloc(sizeof(struct in6_addr));
memset(mask,0,sizeof(struct in6_addr));
struct in6_addr *result=malloc(sizeof(struct in6_addr));

//Operacion NOR de root con la mascara en 0, me queda los bits en 1 de los que no estan seteados en el root en 1
for(i=0;i<8;i++){
result->s6_addr16[i]=~(root_addr.s6_addr16[i] | mask->s6_addr16[i]);  //operacion NOR
}

//Seteo en el resultado todos en cero hasta donde me indica la dimension del hipercubo. Acordarse que hc_dim tiene que ser multiplo de 4
for(i=0;i<((128-hc_dim)/16);i++){  
result->s6_addr16[i]=0x0000;
}

//Hago una operacion del resultado hasta ahora con la direccion primaria y obtengo solo los bits de direccionamiento del hipercubo, lo que varian en la asignacion de direcciones
for(i=0;i<8;i++){
result->s6_addr16[i]=result->s6_addr16[i] & addr->s6_addr16[i];
}

/*
//Para realizar pruebas de desplazamiento---
    result->s6_addr16[0]=0x0000;
    result->s6_addr16[1]=0x0000;
    result->s6_addr16[2]=0x0000;
    result->s6_addr16[3]=0x0000;
    result->s6_addr16[4]=0x00c0;
    result->s6_addr16[5]=0x0000;
    result->s6_addr16[6]=0x0000;
    result->s6_addr16[7]=0x0000;
//-------------------------------------------
*/

//fprintf(stderr,"UNO= %s\n", ip6_to_str(*result));

//Intercambio de lugar los bytes de cada dato de ..s6_addr16[i] para realizar desplazamiento
for(i=0;i<8;i++){
aux1->s6_addr16[i]=result->s6_addr16[i] & 0xff00;
aux2->s6_addr16[i]=result->s6_addr16[i] & 0x00ff;
aux1->s6_addr16[i]=aux1->s6_addr16[i]>>8;
aux2->s6_addr16[i]=aux2->s6_addr16[i]<<8;
result->s6_addr16[i]=aux1->s6_addr16[i] + aux2->s6_addr16[i];
}
//fprintf(stderr,"DOS= %s\n", ip6_to_str(*result));

//Realizo el shift
//A izquierda
if(izq_der==1){
for(j=0;j<cantidad;j++){
for(i=7,carry1=0,carry2=0;i>=0;i--){
carry1=carry2;
if((result->s6_addr16[i]& 0x8000)==0x0000)
carry2=0;
else 
carry2=1;
result->s6_addr16[i]=result->s6_addr16[i]<<1;
if(carry1==1)
result->s6_addr16[i]=result->s6_addr16[i] | 0x0001;
}
}
}
//A derecha
if(izq_der==0){
for(j=0;j<cantidad;j++){
for(i=0,carry1=0,carry2=0;i<=7;i++){
carry1=carry2;
if((result->s6_addr16[i]& 0x0001)==0x0000)
carry2=0;
else 
carry2=1;
result->s6_addr16[i]=result->s6_addr16[i]>>1;
if(carry1==1)
result->s6_addr16[i]=result->s6_addr16[i] | 0x8000;
}
}
}


//fprintf(stderr,"TRES= %s\n", ip6_to_str(*result));

//Vuelvo a dar vuelta los bytes de datos s6_addr16[i]
for(i=0;i<8;i++){
aux1->s6_addr16[i]=result->s6_addr16[i] & 0xff00;
aux2->s6_addr16[i]=result->s6_addr16[i] & 0x00ff;
aux1->s6_addr16[i]=aux1->s6_addr16[i]>>8;
aux2->s6_addr16[i]=aux2->s6_addr16[i]<<8;
result->s6_addr16[i]=aux1->s6_addr16[i] + aux2->s6_addr16[i];
}
//fprintf(stderr,"CUATRO= %s\n", ip6_to_str(*result));

//Aplico nuevamente la mascara de la dimension de nuestra sesion, luego del desplazamiento
for(i=0;i<((128-hc_dim)/16);i++){  
result->s6_addr16[i]=0x0000;
}
//fprintf(stderr,"CINCO= %s\n", ip6_to_str(*result));

//Realizo la operacion OR con los datos desplazados y mascarados con la direccion root
for(i=0;i<8;i++)
result->s6_addr16[i]=result->s6_addr16[i] | root_addr.s6_addr16[i];
//fprintf(stderr,"SEIS= %s\n", ip6_to_str(*result));
memcpy(addr,result,sizeof(struct in6_addr));
free(mask);
free(result);
free(aux1);
free(aux2);
return;
}

void borrar_tabla_de_rutas(struct route *rt){

	int i;	
	for(i=0;rt->next,i++;rt!=NULL){
	prtinf("ID=%d\n\n",i);
	fprintf(stderr,"dst:%s\n",ip6_to_str(rt->dst));
	fprintf(stderr,"gtw:%s\n",ip6_to_str(rt->gtw));
	printf("metrica:%d\n",rt->metric);
	del_rt_route(rt->dst,rt->gtw,rt->dst_mask,rt->distance);
	}
return;

}

//**************************************************************************************

