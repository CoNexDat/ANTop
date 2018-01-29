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
extern struct in6_addr prim_addr_aux;  //es un auxiliar de prim_addr, que lo utilizo para guardar la direccion orginal prim_addr cuando esta cambia por algo

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
int flag_fragmentacion;
int flag_fragmentacion_primaria;
int flag_fragmentacion_secundaria;
unsigned char prim_mask_inicial; // es el valor del prim_mask incial, antes de que asigne o ceda espacio de direcciones a otros, es el prim_mask virgen de cada nodo
unsigned char sec_mask_inicial; //iden prim_mas_frag, pero para sec_mask

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


//***************************************************************************************
//***************************************************************************************
//						FRAGMENTACION
//***************************************************************************************
//***************************************************************************************

int send_far(int sock,struct in6_addr *src,struct in6_addr *dst, struct in6_addr *src_old){

    struct ctrl_pkt *far;

    far = malloc(sizeof(struct ctrl_pkt));

    u_int8_t hop_l;
    hop_l = 20;    

    far->pkt_type_f = FAR;
    memcpy(&(far->src_addr),src_old, sizeof(struct in6_addr));
    memcpy(&(far->prim_addr),src, sizeof(struct in6_addr));
    memcpy(&(far->ad_addr.addr),dst,sizeof(struct in6_addr));
    far->ad_addr.mask=prim_mask_inicial;
    far->ad_addr.red_numero=red_numero;
   
    antop_socket_send(((unsigned char *)far), *dst, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);
    
    free(far);
	
    return 0;

}


int send_fan(int sock,struct in6_addr *src,struct in6_addr *dst){ 

    struct ctrl_pkt *fan;

    fan = malloc(sizeof(struct ctrl_pkt));

    u_int8_t hop_l;
    hop_l = 20;    

    fan->pkt_type_f = FAN;
    memcpy(&(fan->src_addr),src, sizeof(struct in6_addr));
    fan->ad_addr.red_numero=red_numero;
   
    antop_socket_send(((unsigned char *)fan), *dst, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);
    
    free(fan);
	
    return 0;

}


void detector_de_fragmentacion() {

int flag=0;
int distance,distancia_a_local;   
struct neighbor *it;

//EL VALOR DEL NODO ROOT (primera direccion de hipercubo)
struct in6_addr *root_aux=malloc(sizeof(struct in6_addr));
memcpy(root_aux,&root,sizeof(struct in6_addr));
	
if(!memcmp(&prim_addr,root_aux,sizeof(struct in6_addr)))
      return;

//ESTE CILCO SE UTILIZA PARA EL RECONOCIMIENTO DEL VECINO MAS CERCA DEL ROOT
for(it=neigh_table; it != NULL; it=it->next){
	
	distance=dist(root_aux,*it);
	distancia_a_local=dist(&prim_addr,*it);  //cuando empieza la red, a veces hay vecinos mayor a distancia 1 que luego desaparecen cuando dan una direccion secundaria, por eso hay que descartarlos para que nunca sean root neighbor

	if((distance <= neighbor_to_root.distance) && (distancia_a_local==1) && !check_addr_in_n_s_l_table(&(it->prim_addr))){  //aca puse "igual" para que si en el caso de mezcla, haya mas de uno con la misma condiciones para ser neighbor root, entonces que vayan alternando hasta que se estabilice. Tambien chequea en la tabla de vecinos que tienen enlace conmigo por medio de su direccion secundaria, si es asi, entonces no puede ser el neighbor root.
	neighbor_to_root.distance=distance;
	memcpy(&(neighbor_to_root.addr),&(it->prim_addr),sizeof(struct in6_addr));
	}

	}

fprintf(stderr,"El vecino con menor distancia al root es: %s con distancia al root de %d\n",ip6_to_str(neighbor_to_root.addr),neighbor_to_root.distance);

//ESTE CICLO RESUELVE SI SIGUE SIENDO VECINO EL VECINO ROOT
for(it=neigh_table; it != NULL; it=it->next){
	
	if(memcmp(&(neighbor_to_root.addr),&(it->prim_addr),sizeof(struct in6_addr))==0)  {
 		flag=1;
		break;
		}

	else flag=0;
	}

if(flag==0)
    if(ping6(&root)==0)
	fragmentacion();

free(root_aux);

return;
}


void fragmentacion(){

struct in6_addr *root_aux=malloc(sizeof(struct in6_addr));
memcpy(root_aux,&root,sizeof(struct in6_addr));
struct neighbor *it;
struct recovered_addr *it2;

	printf("\n\n\n\n\n*******************************************\n");
	printf("SE DETECTO FRAGMENTACION CON LA RED ANTop\n");
	printf("*******************************************\n");
	printf("Se inicia una nueva red ANTop!!!\n");
	
	struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
	memcpy(aux1,&prim_addr,sizeof(struct in6_addr));
	struct in6_addr *aux2=malloc(sizeof(struct in6_addr));
	memcpy(aux2,&prim_addr,sizeof(struct in6_addr));
	
	//INCIO EL PROCESO DE FRAGMENTACION
	flag_fragmentacion=1;

	//MARCO QUE SE ESTA TRATANDO LA DIRECCION PRIMARIA
	if(sec_addr_flag==1)
		flag_fragmentacion_primaria=1;

	//NUMERO DE RED
	red_numero=mac_address();

	//DIRECCION PRIMARIA
	shift(1,prim_mask_inicial,*root_aux,aux1);
	memcpy(&prim_addr,aux1,sizeof(struct in6_addr));
	del_and_addnew(aux2,aux1,1);
	sleep(2);  //le doy tiempo para que cambie de direccion de interfaz y no salte error cuando mande un mensaje far
	
	//MASCARA PRIMARIA
	prim_mask=prim_mask - prim_mask_inicial;

	//TABLA DE RECUPERACION DE DIRECCIONES
	for(it2=rcvd_addr; it2!=NULL; it2=it2->next){
		shift(1,prim_mask_inicial,*root_aux,&(it2->prim_addr));
		it2->prim_mask= it2->prim_mask - prim_mask_inicial;
			}	
	
	//BORRO TABLA DE RUTAS
	if(route_t!=NULL)
	borrar_tabla_de_rutas(route_t);

	//BORRO TABLA DE NOMBRES
	clear_rv_tables();

	//CICLO DE ENVIOS FAR
	for(it=neigh_table; it != NULL; it=it->next){
	if(sec_addr_flag==1 && dist(&sec_addr,*it)==1)  //Esto se hace para que no se forme bucles de FAR, si se tiene sec_addr, solo le envio a los vecinos que no se tiene enlace secundario.
		delete_neighbor(it);
	else{
	  send_far(socket_global,aux1,&(it->prim_addr),aux2);
	  delete_neighbor(it); //borro dicho vecino ya que va cambiar de direccion
		}	
	}
	
	//MASCARA INCIAL
	prim_mask_inicial=0;

	//LIBERO MEMORIA
	free(aux1);
	free(aux2);

}


void process_far(struct ctrl_pkt* antop_msg){

int i,flag=0, aux_prim_mask_inicial;
struct neighbor aux_neighbor;
struct neighbor *it;
struct recovered_addr *it2;
struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
struct in6_addr *aux2=malloc(sizeof(struct in6_addr));
struct in6_addr *aux3=malloc(sizeof(struct in6_addr));


//INCIA EL PROCESO DE FRAGMENTACION 
flag_fragmentacion=1;
 

//EN EL CASO QUE LA DIRECCION DESTINO COINCIDE CON LA DIRECCION SECUNDARIA
//------------------------------------------------------------------------
if(!memcmp(&(antop_msg->ad_addr.addr),&sec_addr,sizeof(struct in6_addr))){

//MARCO QUE SE ESTA PROCESANDO LA DIRECCION SECUNDARIA
flag_fragmentacion_secundaria=1;

memcpy(aux1,&sec_addr,sizeof(struct in6_addr));
memcpy(aux2,&root,sizeof(struct in6_addr));
memcpy(aux3,&sec_addr,sizeof(struct in6_addr));

//SI LA TABLA DE RUTAS TIENE ENTRADAS, ENTONCES QUE LAS BORRE
if(route_t!=NULL)
borrar_tabla_de_rutas(route_t);

//ENVIO CONFIRMACION DE RECEPCION DEL PAQUETE FAR
send_fan(socket_global,aux1,&(antop_msg->prim_addr));

//NUMERO DE RED LOCAL
red_numero=antop_msg->ad_addr.red_numero;

//ASIGNACION DE DIRECCION SECUNDARIA
shift(1,antop_msg->ad_addr.mask,*aux2,aux1);
memcpy(&sec_addr,aux1,sizeof(struct in6_addr));
del_and_addnew(aux3,aux1,0);
sleep(2);

//MASCARA SECUNDARIA
sec_mask=sec_mask - antop_msg->ad_addr.mask;

//VECINO DEL ENLACE SECUNDARIO
//add_neighbor(antop_msg,1);
//memcpy(&(addr_node_secondary_link),&(antop_msg->prim_addr),sizeof(struct in6_addr));


if(flag_fragmentacion_primaria==1){
flag_fragmentacion=0;
flag_fragmentacion_primaria=0;
flag_fragmentacion_secundaria=0;
register_rv();
}

//LIBERO MEMORIA
free(aux1);
free(aux2);
free(aux3);

return;
}
//--------------------------------------------------------------------------

//MARCO QUE SE ESTA PROCESANDO LA DIRECCION PRIMARIA
flag_fragmentacion_primaria=1;

memcpy(aux1,&prim_addr,sizeof(struct in6_addr));
memcpy(aux2,&root,sizeof(struct in6_addr));
memcpy(aux3,&prim_addr,sizeof(struct in6_addr));

//GUARDO LA MASCARA INICAL ORIGINAL
aux_prim_mask_inicial=prim_mask_inicial;
prim_mask_inicial=antop_msg->ad_addr.mask;

//GUARDO LA DIRECCION ORIGEN DEL FAR COMO VECINO Y LO ASIGNO COMO NODO PADRE
add_neighbor(antop_msg, 0); //agrego como vecino la direccion de la source del paquete FAR  (direccion NEW del nodo que envia el FAR)
memcpy(&(neighbor_to_root.addr),&(antop_msg->prim_addr),sizeof(struct in6_addr)); //Seteo la direccion proveniente el FAR como neighbor root

memcpy(&(aux_neighbor.prim_addr),&(antop_msg->prim_addr),sizeof(struct in6_addr));
aux_neighbor.prim_mask=antop_msg->ad_addr.mask;
neighbor_to_root.distance=dist(aux2,aux_neighbor);

//ENVIO EL MESAJE DE CONFIRMACION
send_fan(socket_global,aux1,&(antop_msg->prim_addr));

//NUMERO DE RED
red_numero=antop_msg->ad_addr.red_numero;

//DIRECCION PRIMARIA
shift(1,antop_msg->ad_addr.mask,*aux2,aux1);
memcpy(&prim_addr,aux1,sizeof(struct in6_addr));
del_and_addnew(aux3,aux1,1);
sleep(2);

//TABLA DE RECUPERACION DE DIRECCIONES
for(it2=rcvd_addr; it2!=NULL; it2=it2->next){
	shift(1,antop_msg->ad_addr.mask,*aux2,&(it2->prim_addr));
	it2->prim_mask= it2->prim_mask - antop_msg->ad_addr.mask;
		}


//BORRO TABLA DE RUTAS
if(route_t!=NULL)
borrar_tabla_de_rutas(route_t);

//BORRO TABLA DE NOMBRES
clear_rv_tables();

//CICLO DE ENVIOS FAR
if(prim_mask != aux_prim_mask_inicial || flag_neigh_by_second == 1) //Solo va enviar paquetes FAR si asigno alguna direccion a alguien, es decir, si el prim_mask es distinto al prim_mask inicial, o tambien si tiene vecinos por enlaces secundarios.
for(it=neigh_table,i=0; it != NULL; it=it->next,i++){
	if(memcmp(&(neighbor_to_root.addr),&(it->prim_addr),sizeof(struct in6_addr))  &&  memcmp(&prim_addr,&(it->prim_addr),sizeof(struct in6_addr)) && memcmp(&(antop_msg->src_addr), &(it->prim_addr),sizeof(struct in6_addr))){
		if(sec_addr_flag==1 && dist(aux3,*it)>1)  //Esto se hace para que no se forme bucles de FAR, si se tiene sec_addr, solo le envio a los vecinos que no se tiene enlace secundario.
		     delete_neighbor(it);
		else{
		send_far(socket_global,aux1,&(it->prim_addr),aux3);
		delete_neighbor(it);//borro dicho vecino ya que va cambiar de direccion
		flag=1;  //flag=1 es que el nodo tiene subordinados y que tiene que esperar un FAN para que se acitven los demas servicios
		}
		}
	else if(!memcmp(&(antop_msg->src_addr),&(it->prim_addr),sizeof(struct in6_addr))){
		delete_neighbor(it);} //borro de la tabla de vecinos la direccion OLD del nodo en donde vino el FAR
}

//MASCARA PRIMARIA
prim_mask=prim_mask - antop_msg->ad_addr.mask;

//MASCARA INICIAL 
prim_mask_inicial=aux_prim_mask_inicial - antop_msg->ad_addr.mask;

if(flag==0){
	if(sec_addr_flag == 0 || flag_fragmentacion_secundaria == 1){
		flag_fragmentacion=0;
		flag_fragmentacion_primaria=0;
		flag_fragmentacion_secundaria=0;
		register_rv();
			}
	}

//LIBERO MEMORIA
free(aux1);
free(aux2);
free(aux3);

return;
}



void process_fan(struct ctrl_pkt* antop_msg){

if(flag_fragmentacion == 1){
	if(sec_addr_flag == 0 || (flag_fragmentacion_primaria == 1 && flag_fragmentacion_secundaria == 1)){
		flag_fragmentacion=0;
		flag_fragmentacion_primaria=0;
		flag_fragmentacion_secundaria=0;
		register_rv();
		}
	}
return;
}


//FUNCION QUE CUANDO NO TIENE VECINOS, ENTONCES QUE REINICIE TODOS LOS PARAMETROS
void check_if_i_am_only(){

	if(neigh_table==NULL && memcmp(&prim_addr, &root, sizeof(struct in6_addr))!=0){
		del_and_addnew(&prim_addr,&root,2);
		memcpy(&prim_addr, &root, sizeof(struct in6_addr));
		prim_mask=0;
		prim_mask_inicial=0;
		red_numero=mac_address();
		neighbor_to_root.distance=99;
		sec_mask=0;
		memset(&sec_addr,0, sizeof(struct in6_addr));
		flag_fragmentacion=0;
		flag_fragmentacion_primaria=0;
		flag_fragmentacion_secundaria=0;
		flag_mezcla=0;
		flag_mezcla_primaria=0;
		flag_mezcla_secundaria=0;
		rcvd_addr=NULL;
		}

	return;
}


