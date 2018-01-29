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

//DIRECCION PRIMARIA
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
unsigned char red_numero;

//PROPUESTA DIRECCION SECUNDARIA
//Del lado del que genera el SAP
int flag_addr_avai=1; //es el flag que se activa cuando se espera un SAN
struct in6_addr addr_node_secondary_link;
//Del lado del que recibe el SAP
int flag_neigh_by_second;  //este flag se activa cuando un nodo tiene vecinos por enlaces secundario, es decir que recibio algun paquete SAP
struct n_s_l *n_s_l_table=NULL;  //tabla de los vecinos en el cual se tiene un enlace secundario, sin que yo fuera el autor del enlace secundario

//RECUPERACION DE DIRECCIONES DEL ESPACIO
extern struct recovered_addr *rcvd_addr;




void san_t_handler(){
    fprintf(stderr,"The SAN timer has expired\n");
    if(!flag_addr_avai)
        flag_addr_avai =1;
    return;
}


void send_sap1(int sock, struct in6_addr *dst, struct in6_addr *sec_addr, int sec_mask){

struct ctrl_pkt *sap;
struct timer* san_t;
struct neighbor *neighbor_aux=malloc(sizeof(struct neighbor));

u_int8_t hop_l =20;

//NO ENVIAR SAP CUANDO SE ESPERA UN SAN
if(!flag_addr_avai){
        fprintf(stderr,"Hay una direccion propuesta, esperando confirmacion SAN\n");
	return;
	}

//Aca chequea si la secundaria a proponer tiene distancia uno con la primaria
memcpy(&(neighbor_aux->prim_addr),&prim_addr,sizeof(struct in6_addr));
neighbor_aux->prim_mask=prim_mask;
if(dist(sec_addr,*neighbor_aux) != 1)
	return;

//ERROR DE ASIGNACION DE MEMORIA
if((sap = malloc(sizeof(struct ctrl_pkt))) == NULL){
        fprintf(stderr,"Could not allocate space for ctrl message \n");
        return;
	}

//MENSAJE DE ENVIO
fprintf(stderr,"Se envia un mensaje SAP a %s y se propone la direccion %s con mascara %d\n", ip6_to_str(*dst),ip6_to_str(*sec_addr),sec_mask);

//CAMPOS DEL PAQUETE SAP
sap->pkt_type_f = SAP;
memcpy(&(sap->prim_addr), &prim_addr, sizeof(struct in6_addr));
memcpy(&(sap->src_addr), &prim_addr, sizeof(struct in6_addr));
memcpy(&(sap->ad_addr.addr), sec_addr, sizeof(struct in6_addr));
sap->ad_addr.mask = sec_mask;
sap->ad_addr.red_numero=red_numero;

//TIMEOUT DE RECEPCION MENSAJE SAN 
san_t = malloc(sizeof(struct timer));
if(san_t == NULL){
       fprintf(stderr,"Could not locate space for the timer in send sap\n");
       return;
	}
san_t->handler = &san_t_handler;
san_t->used = 0;
timer_add_msec(san_t, T_SAN);

antop_socket_send((unsigned char *)sap, *dst, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);

flag_addr_avai=0;  //se activa para no enviar mas SAP cuando se espera un SAN

return;

}


void send_san1(int sock, struct in6_addr* dst, struct in6_addr* sec_addr, unsigned char sec_mask, unsigned char prim_mask_source){

struct ctrl_pkt *san;

//ERROR DE ASIGNACION DE MEMORIA
if((san = malloc(sizeof(struct ctrl_pkt))) == NULL){
        fprintf(stderr,"Could not allocate space for ctrl message \n");
        return;
	}

u_int8_t hop_l =20;

memset(san, 0, sizeof(struct ctrl_pkt));

//CAMPOS DEL PAQUETE SAN
san->pkt_type_f = SAN;
memcpy(&(san->prim_addr), &prim_addr, sizeof(struct in6_addr));
memcpy(&(san->src_addr), &prim_addr, sizeof(struct in6_addr));
memcpy(&(san->ad_addr.addr), sec_addr, sizeof(struct in6_addr));
san->ad_addr.mask = sec_mask;
san->ad_addr.red_numero=red_numero;
san->ad_addr.t_lenght=prim_mask_source;

fprintf(stderr,"A SAN will be send to %s", ip6_to_str(*dst));

antop_socket_send((unsigned char *)san, *dst, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);

return;

}



void process_sap1(struct ctrl_pkt* msg){

init_tables_secondary_link();

flag_neigh_by_second=1;

add_n_s_l_table(&(msg->ad_addr.addr), msg->ad_addr.mask);

send_san1(socket_global, &(msg->src_addr), &(msg->ad_addr.addr),msg->ad_addr.mask, prim_mask);

return;
}



void process_san1(struct ctrl_pkt* msg){

struct ctrl_pkt *msg_aux=malloc(sizeof(struct ctrl_pkt));
struct neighbor *neighbor_aux=malloc(sizeof(struct neighbor));
int flag=0;

memcpy(msg_aux, msg, sizeof(struct ctrl_pkt));

msg_aux->ad_addr.mask=msg->ad_addr.t_lenght;

sec_addr_flag=1;
flag_addr_avai=1;

//PARAMETROS DIRECCION SECUNDARIA
memcpy(&sec_addr,&(msg->ad_addr.addr),sizeof(struct in6_addr));
sec_mask=msg->ad_addr.mask;

//ASIGNACION DE DIRECCION SECUNDARIA EN LA INTERFAZ DE RED
add(&(msg->ad_addr.addr));
sleep(2);

//Aca chequea si la secundaria tiene distancia uno con la primaria
memcpy(&(neighbor_aux->prim_addr),&prim_addr,sizeof(struct in6_addr));
neighbor_aux->prim_mask=prim_mask;
if(dist(&sec_addr,*neighbor_aux) == 1)
	flag=1;

//ASIGNACION DE MASCARA PRIMARIA
if((prim_mask < msg->ad_addr.mask) && flag==1 )
prim_mask=msg->ad_addr.mask;

//ELIMINO EN LA DIRECCION DE LA TABLA DE RECUPERACION DE ESPACIO DE DIRECCIONES SI ES QUE ESTA
del_rcvd_addr_table(&(msg->ad_addr.addr));

//AGREGO A LA TABLA DE VECINOS AL NODO QUE ENVIO EL SAN
add_neighbor(msg_aux, 0);

//AGREGO COMO EL NODO QUE ESTA VINCULADO CON MI DIRECCION SECUNDARIA
memcpy(&addr_node_secondary_link, &(msg_aux->prim_addr), sizeof(struct in6_addr));

fprintf(stderr,"sec %s y %d\n", ip6_to_str(msg->ad_addr.addr),msg->ad_addr.mask);


return;

}



//CHEQUEA SI SE PUEDE ASIGNAR UNA DIRECCION SECUNDARIA AL VECINO QUE ENVIO EL HB
void chk_sec_addr1(struct neighbor *neigh){

if(!sec_addr_flag){

//PRIMERO SE CHEQUEA LA TABLA DE RECUPERACION DE ESPACIO
struct recovered_addr *it;
for(it=rcvd_addr;it!=NULL; it=it->next)
	if(dist(&(it->prim_addr), *neigh) == 1){
		init_tables_secondary_link();               
		send_sap1(socket_global, &(neigh->prim_addr), &(it->prim_addr), it->prim_mask);
		return;
			}

//CHEQUEO QUE SI HAY DIRECCION DEL ESPACIO PARA ASIGNAR
if( prim_mask >= hc_dim ) //si no hay direcciones para ofrecer, retornar
      return;

//ASIGNO UNA DIRECCION DE MI ESPACIO
int i,j;
unsigned char prim_mask_aux=prim_mask;
struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
struct in6_addr *aux2=malloc(sizeof(struct in6_addr));
    
memset(aux1,0,sizeof(struct in6_addr));
aux1->s6_addr16[0]=0x0080;

fprintf(stderr,"NEIGHBOR ADDRESS chk sec addr: %s\n", ip6_to_str(neigh->prim_addr));

shift_unos(0, hc_dim + prim_mask -1,aux1);

for(i=0; i<(128-(hc_dim + prim_mask)) ; i++){
	shift_unos(0,1,aux1);
	memcpy(aux2,&prim_addr,sizeof(struct in6_addr));
	prim_mask_aux = prim_mask_aux + 1;
	for(j=0;j<8;j++)
		aux2->s6_addr16[j]=aux2->s6_addr16[j] | aux1->s6_addr16[j];
	if(dist(aux2,*neigh) == 1){
		init_tables_secondary_link();
		send_sap1(socket_global, &(neigh->prim_addr), aux2, prim_mask_aux);
			}	
		}

free(aux1);
free(aux2);		
}

return;

}



//CHEQUEA SI EL VECINO QUE ESTA VINCULADO CON MI DIRECCION SECUNDARIA EXISTE EN LA TABLA DE VECINOS
void check_neigh_of_secondary_address() {
	
struct neighbor *it;
struct neighbor *neighbor_aux=malloc(sizeof(struct neighbor));
struct neighbor *neighbor_aux2=malloc(sizeof(struct neighbor));
struct in6_addr *aux=malloc(sizeof(struct in6_addr));
int flag=0;

if(sec_addr_flag){

//Aca chequea si la secundaria por alguna razon ya no tiene distancia uno con la primaria
memcpy(&(neighbor_aux2->prim_addr),&prim_addr,sizeof(struct in6_addr));
neighbor_aux2->prim_mask=prim_mask;
if(dist(&sec_addr,*neighbor_aux2) != 1)
	goto etiqueta;

for(it=neigh_table; it != NULL; it=it->next){
	if(!memcmp(&(it->prim_addr), &(addr_node_secondary_link), sizeof(struct in6_addr))  && dist(&sec_addr,*it) == 1)		
		flag=1;	
				}

etiqueta:
if(flag==0){
	neighbor_aux->prim_mask=sec_mask;
	neighbor_aux->prim_mask_inicial=sec_mask;
	memcpy(&(neighbor_aux->prim_addr),&sec_addr,sizeof(struct in6_addr));
	sec_addr_flag=0;
	memcpy(aux, &sec_addr, sizeof(struct in6_addr));
	memset(&sec_addr,0, sizeof(struct in6_addr));
	memset(&(addr_node_secondary_link),0,sizeof(struct in6_addr));
	if(prim_mask==sec_mask){
		prim_mask=prim_mask-1;
		sec_mask=0;
			}
	else{ 
		sec_mask=0;
		recover_addr_space(neighbor_aux);
		}
	del(aux);
	sleep(2);	
	//recover_addr_space(neighbor_aux);
	}
}

free(aux);
free(neighbor_aux);
free(neighbor_aux2);
return;

}




//INICIALIZA LOS REGISTROS/TABLAS QUE SE USA PARA LA GESTION DE ENLACES SECUNDARIOS
void init_tables_secondary_link() {

//TABLA DE LOS VECINOS EN EL CUAL ELLOS TIENEN UN DIRECCION SECUNDARIA FORMANDO ENLACE SECUNDARIA CONMIGO
 if(n_s_l_table == NULL){
        n_s_l_table = malloc(sizeof(struct n_s_l));
        if(n_s_l_table == NULL){
            fprintf(stderr,"Could not create the neighbor with second link table\n");
            return;
        }
        n_s_l_table->next = NULL;
        memset(&(n_s_l_table->addr), 0, sizeof(struct in6_addr));
	n_s_l_table->mask=0;
	
		}
return;

}



//SUMA UN ELEMENTO A LA TABLA DE LOS VECINOS EN EL CUAL TIENEN DIRECCIONES SECUNDARIAS CON ENLACES CONMIGO
void add_n_s_l_table(struct in6_addr *addr, unsigned char mask){

    struct n_s_l *it = n_s_l_table;
    struct in6_addr aux;

    memset(&aux, 0, sizeof(struct in6_addr));

    if(!memcmp(&(n_s_l_table->addr), &aux, sizeof(struct in6_addr))){
        goto add;
    }else{
        while(1){        
            if(it->next == NULL){            
                it->next = malloc(sizeof(struct n_s_l));
                if(it->next == NULL){
                    fprintf(stderr,"Could not locate space\n");
                    return;
                }
                break;
            }        
            it = it->next;
        }
    }
    it=it->next;
add:it->mask =mask;
    it->next = NULL;
    memcpy(&(it->addr), addr, sizeof(struct in6_addr));
    
    return;
}



//RESTA UN ELEMENTO A LA TABLA DE LOS VECINOS EN EL CUAL TIENEN DIRECCIONES SECUNDARIAS CON ENLACES CONMIGO
int del_n_s_l_table(struct in6_addr *addr/*, unsigned char mask*/){


    struct n_s_l *prev, *curr;
    
    for (prev = NULL, curr = n_s_l_table; curr != NULL; prev = curr, curr = curr->next) {	
	if(!memcmp(&(curr->addr), addr, sizeof(struct in6_addr))){
		if (prev == NULL)
			n_s_l_table=curr->next;
		else prev->next=curr->next;			
		return 0;
			}
     		}
	return -1;
}


//CHEQUEA SI LA n_s_l_table ESTA VACIA, O SI SIGUEN SIENDO VEICNOS LOS INTEGRANTES DE DICHA TABLA
void check_n_s_l_table(){
	
	int flag=0;
	struct n_s_l *it1;
	struct neighbor *it2;

//MONITOREO DE ELEMENTOS DE LA TABLA "n_s_l_table" SI ESTAN EN LA TABLA "neigh_table"
	for(it1=n_s_l_table; it1!=NULL; it1=it1->next){
		for(it2=neigh_table; it2 != NULL; it2=it2->next)
			if(!memcmp(&(it1->addr),&(it2->prim_addr),sizeof(struct in6_addr)) )
					flag=1;
			if(flag=0)			
				del_n_s_l_table(&(it1->addr));
			else flag=0;
			}

	if(n_s_l_table==NULL)
		flag_neigh_by_second=0;

	return;
	
}



//CHEQUEA SI UNA DIRECCION ESTA EN LA TABLA DE LOS VECINOS EN EL CUAL TIENEN DIRECCIONES SECUNDARIAS Y ENLACE CONMIGO
//Devuelve 1 si encontro la direccion en la lista, o devuelve 0 en caso contrario
int check_addr_in_n_s_l_table(struct in6_addr *addr){
	
	struct n_s_l *it;
	
	for(it=n_s_l_table ; it!=NULL ; it=it->next)
		if(!memcmp(&(it->addr),addr,sizeof(struct in6_addr)) )
			return 1;		

	return 0;	

}










