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
unsigned char red_numero;
extern int flag_conexion;//pablo6
extern int flag_init;//pablo6

//FRAGMENTACION
extern int flag_fragmentacion;
extern int flag_fragmentacion_primaria;
extern int flag_fragmentacion_secundaria;
extern unsigned char prim_mask_inicial; // es el valor del prim_mask incial, antes de que asigne o ceda espacio de direcciones a otros, es el prim_mask virgen de cada nodo
extern unsigned char sec_mask_inicial; //iden prim_mas_frag, pero para sec_mask

//MEZCLA
int flag_mezcla;
int flag_mezcla_primaria;
int flag_mezcla_secundaria;
unsigned char prim_mask_mezcla;  //es el valor de shift de offset que se va a propagar en la rama de abajo del nodo de mezcla

//PROPUESTA DIRECCION SECUNDARIA
//Del lado del que genera el SAP
extern struct in6_addr addr_node_secondary_link;
//Del lado del que recibe el SAP
extern int flag_neigh_by_second;  //este flag se activa cuando un nodo tiene vecinos por enlaces secundario, es decir que recibio algun paquete SAP 

//RECUPERACION DE DIRECCIONES DEL ESPACIO
extern struct recovered_addr *rcvd_addr;



//***************************************************************************************
//***************************************************************************************
//						MEZCLA
//***************************************************************************************
//***************************************************************************************

int send_mar1(int sock,struct in6_addr *prim_addr,struct in6_addr *dst, struct in6_addr *src, unsigned char mask, unsigned char t_lenght){

    struct ctrl_pkt *mar1;

    mar1 = malloc(sizeof(struct ctrl_pkt));

    struct in6_addr *dst_m=malloc(sizeof(struct in6_addr));//pablo7
    memset(dst_m,0,sizeof(struct in6_addr));//pablo7
    dst_m->s6_addr16[0]=0x02ff;//pablo7
    dst_m->s6_addr16[7]=0x0100;//pablo7

    u_int8_t hop_l;
    hop_l = 20;    

    mar1->pkt_type_f = MAR1;
    memcpy(&(mar1->src_addr),src, sizeof(struct in6_addr));
    memcpy(&(mar1->prim_addr),prim_addr, sizeof(struct in6_addr));
    memcpy(&(mar1->ad_addr.addr),dst,sizeof(struct in6_addr));
    mar1->ad_addr.mask=mask;
    mar1->ad_addr.red_numero=red_numero;
    mar1->ad_addr.t_lenght=t_lenght;
   
    if(!memcmp(prim_addr,&dir_default,sizeof(struct in6_addr)) )//pablo7
    	antop_socket_send(((unsigned char *)mar1), *dst_m, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);//pablo7
    else//pablo7
        antop_socket_send(((unsigned char *)mar1), *dst, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);    

    free(mar1);
    free(dst_m);//pablo7
	
    return 0;

}


int send_mar2(int sock,struct in6_addr *prim_addr ,struct in6_addr *dst, struct in6_addr *src, unsigned char mask){

    struct ctrl_pkt *mar2;

    mar2 = malloc(sizeof(struct ctrl_pkt));

    u_int8_t hop_l;
    hop_l = 20;    

    mar2->pkt_type_f = MAR2;
    memcpy(&(mar2->src_addr),src, sizeof(struct in6_addr));
    memcpy(&(mar2->prim_addr),prim_addr, sizeof(struct in6_addr));
    memcpy(&(mar2->ad_addr.addr),dst,sizeof(struct in6_addr));
    mar2->ad_addr.mask=mask;
    mar2->ad_addr.red_numero=red_numero;
   
    antop_socket_send(((unsigned char *)mar2), *dst, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);
    
    free(mar2);	
    
    return 0;

}


int send_man1(int sock,struct in6_addr *src,struct in6_addr *dst, struct in6_addr *src_orig, unsigned char mask_orig, unsigned char red_num){ 

    struct ctrl_pkt *man1;

    man1 = malloc(sizeof(struct ctrl_pkt));

    u_int8_t hop_l;
    hop_l = 20;    

    man1->pkt_type_f = MAN1;
    memcpy(&(man1->src_addr),src_orig, sizeof(struct in6_addr));
    memcpy(&(man1->prim_addr),src, sizeof(struct in6_addr));
    memcpy(&(man1->ad_addr.addr),dst,sizeof(struct in6_addr));
    man1->ad_addr.mask=mask_orig;    
    man1->ad_addr.red_numero=red_num;
   
    antop_socket_send(((unsigned char *)man1), *dst, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);
    
    free(man1);
	
    return 0;

}


int send_man2(int sock,struct in6_addr *src,struct in6_addr *dst, struct in6_addr *src_orig, unsigned char mask_orig, unsigned char red_num){ 

    struct ctrl_pkt *man2;

    man2 = malloc(sizeof(struct ctrl_pkt));

    u_int8_t hop_l;
    hop_l = 20;    

    man2->pkt_type_f = MAN2;
    memcpy(&(man2->src_addr),src_orig, sizeof(struct in6_addr));
    memcpy(&(man2->prim_addr),src, sizeof(struct in6_addr));
    memcpy(&(man2->ad_addr.addr),dst,sizeof(struct in6_addr));
    man2->ad_addr.mask=mask_orig;    
    man2->ad_addr.red_numero=red_num;
   
    antop_socket_send(((unsigned char *)man2), *dst, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);
    
    free(man2);
	
    return 0;

}



int detector_de_mezcla(struct ctrl_pkt *hb){

if(red_numero < hb->ad_addr.red_numero && flag_conexion==0 && flag_init==1){ //pablo6

	printf("\n\n\n\n\n****************************************\n");
	printf("SE DETECTO MEZCLA CON OTRA RED ANTop\n");
	printf("****************************************\n");	

	mezcla(&(hb->prim_addr));//pablo7
	return 1;
	}

if(red_numero == hb->ad_addr.red_numero) 
	return 0;

if(red_numero > hb->ad_addr.red_numero)
	return -1;

}


void delay(){ //pablo8

        int delay;

	//DELAY CON C (entre 0 y 4)
        //srand(time(NULL));
        //delay=rand()%5;

	//DELAY POR LA MAC DE CADA NODO
        //delay=(mac_address()^2) *(4/(250^2));
	delay=2;//(mac_address()-50);//+20)/50;
	sleep(delay);
 
        return;

}



void mezcla(struct in6_addr *dst){   //El nodo le manda al otro un mensaje MAR para que cambie la direccion //pablo7

	struct recovered_addr *it;	
	unsigned char mask_a_proponer;	

	//VERIFICO SI HAY ESPACIO QUE FUE DEVUELTO PARA QUE SEA ASIGNADO A LA NUEVA RED	
	if(rcvd_addr==NULL)
		mask_a_proponer=prim_mask;

	for(it=rcvd_addr; it!=NULL; it=it->next){
		if(it->prim_mask != 0){
			mask_a_proponer=it->prim_mask -1;
			del_rcvd_addr_table(&(it->prim_addr));
			break;
				}
		else mask_a_proponer=prim_mask;	
			}


	flag_mezcla=1;

	delay();//pablo8

	memcpy(&prim_addr_aux,&prim_addr,sizeof(struct in6_addr));  //guardo mi direccion prim_addr antes de asignar la default
	memcpy(&prim_addr,&dir_default,sizeof(struct in6_addr));  //le asigno la direccion default a prim_addr

	del_and_addnew (&prim_addr, &dir_default, 2);  //borro la direccion actual y seteo la direccion default en la interfaz
	sleep(2);	

	send_mar1(socket_global,&prim_addr,dst,&prim_addr_aux,mask_a_proponer/*prim_mask*/,NULL); //pablo7

	free(it);

	return;
}


void shift_mezcla(struct in6_addr src, struct in6_addr *addr, int src_mask, int mf){

int j;

struct in6_addr *aux=malloc(sizeof(struct in6_addr));
struct in6_addr *uno=malloc(sizeof(struct in6_addr));
memcpy(aux,addr,sizeof(struct in6_addr));
memset(uno,0, sizeof(struct in6_addr));
uno->s6_addr16[0]=0x0080;

if((mf-1) >=0){

//REALIZO SHIFT DE CEROS HACIA LA IZQUIERDA LA CANTIDAD DE "mf-1"
shift(1,mf-1,root,aux);

//REALIZO SHIFT A DERECHA DE CEROS, LA CANTIDAD DE "src_mask" 
shift(0, src_mask, root, aux);

//SE REALIZA LA FUNCION OR CON LA DIRECCION "src" 
for(j=0;j<8;j++)  
	aux->s6_addr16[j]=aux->s6_addr16[j] | src.s6_addr16[j]; //en el mensaje mar2 ya se tiene que enviar la resultante del shift local
}

else{

//REALIZO SHIFT DEL UNO HACIA DERECHA LA CANTIDAD DE "src_mask"
shift_unos(0,(128-hc_dim) + src_mask,uno);

//REALIZO SHIFT A DERECHA DE CERO, LA CANTIDAD DE "src_mask"
shift(0,src_mask +1, root, aux);

//SE REALIZA LA FUNCION OR CON LA DIRECCION "src", "uno" y "aux"
for(j=0;j<8;j++) { 
			
	aux->s6_addr16[j]=aux->s6_addr16[j] | uno->s6_addr16[j];
	aux->s6_addr16[j]=aux->s6_addr16[j] | src.s6_addr16[j];
	}
}

memcpy(addr,aux,sizeof(struct in6_addr));

}



void process_mar1(struct ctrl_pkt* antop_msg){

int i, j, flag=0, distancia_al_root, distancia_del_neighbor_al_root, flag_monitoreo=0, flagx=0, flag_sin_sucesores=0, flag_padre_caso_especial=0, flag_par_receptor=0, flagy=0;

struct recovered_addr *it2;
struct neighbor *it;
struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
struct in6_addr *aux2=malloc(sizeof(struct in6_addr));
struct in6_addr *aux3=malloc(sizeof(struct in6_addr));
struct in6_addr *aux4=malloc(sizeof(struct in6_addr));
struct in6_addr *aux5=malloc(sizeof(struct in6_addr));
struct in6_addr *aux6=malloc(sizeof(struct in6_addr));
struct in6_addr *aux7=malloc(sizeof(struct in6_addr));

struct ctrl_pkt *aux_antop_msg=malloc(sizeof(struct ctrl_pkt));

memcpy(aux_antop_msg,antop_msg,sizeof(struct ctrl_pkt));
memcpy(aux1,&prim_addr,sizeof(struct in6_addr));
memcpy(aux2,&root,sizeof(struct in6_addr));
memcpy(aux3,&(antop_msg->src_addr),sizeof(struct in6_addr));
memcpy(aux4,&prim_addr,sizeof(struct in6_addr));
memcpy(aux5,&prim_addr,sizeof(struct in6_addr));
memcpy(aux6,&sec_addr,sizeof(struct in6_addr));
memcpy(aux7,&sec_addr,sizeof(struct in6_addr));

//SI EL NODO NO TIENE SUCESORES, MARCARLO CON UN FLAG
if(prim_mask == prim_mask_inicial)  // si el nodo no tiene sucesores, que no envie ningun mensaje MAR, y se asigne directamente la direccion que le corresponde
	flag_sin_sucesores=1;

//INICIAL EL PROCESO DE MEZCLA
flag_mezcla=1;


//EN EL CASO DE QUE LA DIRECCION DESTINO DEL PAQUETE MAR COINCIDE CON LA DIRECCION SECUNDARIA
//--------------------------------------------------------------------------------------------
if(!memcmp(&(antop_msg->ad_addr.addr),&sec_addr,sizeof(struct in6_addr)) ){

//MARCO QUE SE ESTA PROCESANDO LA DIRECCION SECUNDARIA
flag_mezcla_secundaria=1;

//SI LA TABLA DE RUTAS TIENE ENTRADAS, ENTONCES QUE LAS BORRE
if(route_t!=NULL)
borrar_tabla_de_rutas(route_t);

//ECUACION PARA "sec_mask"
sec_mask= sec_mask + antop_msg->ad_addr.mask - (antop_msg->ad_addr.t_lenght - 1);

//NUMERO DE RED LOCAL
red_numero = antop_msg->ad_addr.red_numero;

//ENVIO CONFIRMACION DE RECEPCION DEL PAQUETE MAR1
send_man1(socket_global,&sec_addr,&(antop_msg->prim_addr), &(antop_msg->src_addr), antop_msg->ad_addr.mask, red_numero);

//CAMBIO LA DIRECCION SECUNDARIA
shift_mezcla(*aux3,aux6,antop_msg->ad_addr.mask, antop_msg->ad_addr.t_lenght);
memcpy(&sec_addr,aux6,sizeof(struct in6_addr));		
del_and_addnew(aux7,aux6,0);
sleep(2);

//SI YA SE PROCESO LA DIRECCION PRIMARIA, ENTONCES TERMINAMOS EL PROCESO DE MEZCLA
if(flag_mezcla_primaria==1){ //F 
flag_mezcla=0;
flag_mezcla_primaria=0;
flag_mezcla_secundaria=0;
register_rv();
}


//LIBERO MEMORIA
free(aux1);
free(aux2);
free(aux3);
free(aux4);
free(aux5);
free(aux6);
free(aux7);
free(aux_antop_msg);

return;
}

//--------------------------------------------------------------------------------------------


//MARCO QUE SE ESTA PROCESANDO LA DIRECCION PRIMARIA
flag_mezcla_primaria=1;

//SI ES EL CASO ES EL NODO QUE RECIBE EL MENSAJE MAR1 DEL NODO DEFAULT
if(!memcmp(&(antop_msg->prim_addr),&dir_default,sizeof(struct in6_addr))){  //en el caso del nodo con dir default, no tiene prim_addr como la direccion original, por eso se le debe asignar la original para que luego sea agregada como vecino en la tabla
	flag_par_receptor=1;
	memcpy(&(aux_antop_msg->prim_addr),&(aux_antop_msg->src_addr),sizeof(struct in6_addr));

//SI EN EL CASO DEL NODO QUE RECIBE DEL DEFAULT, HAY QUE VER SI TIENE VECINOS Y NO SUCESORES, ES DECIR QUE TIENE PADRE Y ENTONCES TIENE QUE HACER EL CICLO DE NVIOS DE MAR2
	if(neigh_table != NULL)
		flag_padre_caso_especial=1;	

//MONITOREO SI HAY ALGUN NEIGHBOR QUE TIENE LA MISMA DIRECCION DEL NODO QUE PROVOCO LA MEZCLA
	char buf[1024];
	for(it=neigh_table; it != NULL; it=it->next){
	if( !memcmp( &(aux_antop_msg->prim_addr),&(it->prim_addr),sizeof(struct in6_addr) ) ){
		flag_monitoreo=1;
                sprintf(buf,"sudo ndisc6 -r 5 %s wlan0",ip6_to_str(it->prim_addr)); //pablo6		
		//sprintf(buf,"sudo ip -6 neigh flush all"); //Borro la tabla ARP
		system(buf);
		flagx=1;
			}
		}
	if(flagx==0 && neigh_table != NULL ) //si justo el nodo que provoca la mezcla es igual a un vecino, no hace falta agregarlo en la tabla de vecinos.
		add_neighbor(aux_antop_msg, 0); //agrego como vecino la direccion de la source del paquete MAR  (direccion NEW del nodo que envia el MAR)
	neighbor_to_root.distance=99; //seteo como la maxima distancia, ya que los vecino en el proximo paso seran borrado y solo quedara el nodo que mando el MAR, entonces seria nuestro padre. Cuando se revea quien es el menor distancia, solo seria el nodo que mando el MAR
	prim_mask_mezcla=prim_mask_inicial;
	}
else{
	prim_mask_mezcla=antop_msg->ad_addr.t_lenght;
	neighbor_to_root.distance=99;  //seteando la maxima distancia, lo que me permite que el neighbor root es el del prim_addr del MAR1, entonces cuando el nodo que esta arriba de mi cambie de direccion (shifte luego de que yo le haya mandado un MAN1), este se convertira en mi nuevo neighbor root
	}


//NUMERO DE RED LOCAL
red_numero=antop_msg->ad_addr.red_numero;


//ECUACIONES DE ASIGNACION DE "prim_mask" y prim_mask_inicial"
prim_mask=prim_mask + antop_msg->ad_addr.mask - (prim_mask_mezcla-1);  // el mecanismo 1, siempre la prim_mask resultante es la suma de las mascaras, menos la prim_mask_mezcla del flooding
prim_mask_inicial = antop_msg->ad_addr.mask + 1 + (prim_mask_inicial - prim_mask_mezcla); 


//CREO UN TIPO ESTRUCTURA NEIGHBOR CON DIRECCION DEL NODO LOCAL Y MEDIR LA DISTANCIA AL ROOT, QUE LUEGO SERA UTILIZADO..
struct neighbor aux_neighbor;  //creo un tipo struct neighbor para luego medir la distancia del nodo local al root y compararla con la distancia del vecino al root 
memcpy(&(aux_neighbor.prim_addr),&prim_addr,sizeof(struct in6_addr));
aux_neighbor.prim_mask=prim_mask;
distancia_al_root=dist(&root,aux_neighbor); //es la distancia del nodo local sin el shift, esto permite saber que nodos estan arriba o debajo de él en la jeraquia de direcciones

//BORRO TABLA DE NOMBRES
clear_rv_tables();

//CICLO DE ENVIO DE PAQUETES MAR1 O MAR2
if(flag_sin_sucesores==0  ||  flag_padre_caso_especial==1 || flag_neigh_by_second==1) // si tiene sucesores, entonces que empiece el ciclo de envio de paquetes MAR1 o MAR2. O tambien, si tiene vecino por enlace secundarios
for(it=neigh_table,i=0; it != NULL; it=it->next,i++){
	if(memcmp(&(aux_antop_msg->prim_addr),&(it->prim_addr),sizeof(struct in6_addr))  ||  flag_monitoreo==1 ){ //que no envia mar al nodo que le envio el mar que esta procesando
		if(sec_addr_flag==1 && dist(aux1,*it)>1)  //Esto se hace para que no se forme bucles de MAR, si se tiene sec_addr, solo le envio a los vecinos que no se tiene enlace secundario.
		     delete_neighbor(it);
		else{
			distancia_del_neighbor_al_root=dist(&root,*it);
			if( (distancia_al_root < distancia_del_neighbor_al_root) || check_addr_in_n_s_l_table(&(it->prim_addr))){  //Envio solo a los vecinos que estan debajo del nodo local, los de arriba se requiere otro tratamiento de shifteado. Excepto la direcciones que son secundaria de otro nodo y los tengo como vecino. 
			send_mar1(socket_global,&prim_addr,&(it->prim_addr),&(antop_msg->src_addr),antop_msg->ad_addr.mask,prim_mask_mezcla);
			delete_neighbor(it);//borro dicho vecino ya que va cambiar de direccion
			flag=1;
				}
			else{
			shift_mezcla(*aux3,aux5,antop_msg->ad_addr.mask, prim_mask_mezcla);		
			prim_mask=prim_mask + 1; //sumo uno mas a mi mascara ya que estoy entregando un espacio de direcciones al mandar un MAR2		
			send_mar2(socket_global,&prim_addr,&(it->prim_addr),aux5,prim_mask);  //le envio el prim_mask actualizada
			delete_neighbor(it);
			flag=1;
				}	
			}
		}	
	}

//SI NO SE ENVIO NINGUN PAQUETE MAR1 O MAR2, ENTONCES QUE SE ASIGNE YA LA DIRECCION RESULTANTE
if(flag==0){
	if(!memcmp(&(antop_msg->prim_addr),&dir_default,sizeof(struct in6_addr))  && neigh_table ==NULL){ // en el caso de que es un par receptor y no tiene vecinos, se debe incializar la tabla de vecinos  y sumar como vecino al par emisor. 
		init_neigh_table();
		add_neighbor(aux_antop_msg, 0);
		flagy=1;
			}
	else send_man1(socket_global,&prim_addr,&(antop_msg->prim_addr), &(antop_msg->src_addr), antop_msg->ad_addr.mask, red_numero);
	shift_mezcla(*aux3,aux1,antop_msg->ad_addr.mask, prim_mask_mezcla);
	memcpy(&prim_addr,aux1,sizeof(struct in6_addr));
	del_and_addnew(aux4,aux1,1);
	sleep(2);
	if(flagy==1)
		send_man1(socket_global,&prim_addr,&(antop_msg->prim_addr), &(antop_msg->src_addr), antop_msg->ad_addr.mask, red_numero);	
	for(it2=rcvd_addr; it2!=NULL; it2=it2->next){
		shift_mezcla(*aux3,&(it2->prim_addr),antop_msg->ad_addr.mask, prim_mask_mezcla);
		it2->prim_mask=it2->prim_mask + antop_msg->ad_addr.mask - (prim_mask_mezcla-1);
			}		
	if(sec_addr_flag == 0 || flag_mezcla_secundaria == 1){ //F // En el caso de que el nodo tenga direccion secundaria, entonces tenemos que ver si se termino el proceso de la direccion secundaria termino
		flag_mezcla=0;
		flag_mezcla_primaria=0;
		flag_mezcla_secundaria=0;
		register_rv();
			}
	}
else send_man1(socket_global,&prim_addr,&(antop_msg->prim_addr), &(antop_msg->src_addr), antop_msg->ad_addr.mask, red_numero);

//LIBERO MEMORIA
free(aux1);
free(aux2);
free(aux3);
free(aux4);
free(aux5);
free(aux6);
free(aux7);
free(aux_antop_msg);

return;

}


void process_mar2(struct ctrl_pkt* antop_msg){

int i,j,flag=0, flag_sin_sucesores=0;

struct neighbor *it;
struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
struct in6_addr *aux2=malloc(sizeof(struct in6_addr));
struct in6_addr *aux3=malloc(sizeof(struct in6_addr));
struct in6_addr *aux4=malloc(sizeof(struct in6_addr));

memset(aux1,0,sizeof(struct in6_addr));
memcpy(aux2,&(antop_msg->src_addr),sizeof(struct in6_addr));
memcpy(aux3,&prim_addr,sizeof(struct in6_addr));
memcpy(aux4,&sec_addr,sizeof(struct in6_addr));

aux1->s6_addr16[0]=0x0080;

//SI EL NODO NO TIENE SUCESORES, MARCARLO CON UN FLAG
if(prim_mask == prim_mask_inicial) // si no tiene sucessores
	flag_sin_sucesores=1;

//INCIAL EL PROCESO DE MEZCLA
flag_mezcla=1;

//EN EL CASO DE QUE LA DIRECCION DESTINO DEL PAQUETE MAR COINCIDE CON LA DIRECCION SECUNDARIA
//--------------------------------------------------------------------------------------------
if(!memcmp(&(antop_msg->ad_addr.addr),&sec_addr,sizeof(struct in6_addr)) ){

//MARCO QUE SE ESTA PROCESANDO LA DIRECCION SECUNDARIA
flag_mezcla_secundaria=1;

//SI LA TABLA DE RUTAS TIENE ENTRADAS, ENTONCES QUE LAS BORRE
if(route_t!=NULL)
borrar_tabla_de_rutas(route_t);

//ECUACION PARA "sec_mask"
sec_mask = antop_msg->ad_addr.mask;

//NUMERO DE RED LOCAL
red_numero=antop_msg->ad_addr.red_numero;

//ENVIO CONFIRMACION DE RECEPCION DEL PAQUETE MAR2
send_man2(socket_global,&sec_addr,&(antop_msg->prim_addr), &(antop_msg->src_addr), sec_mask, red_numero);

//CAMBIO LA DIRECCION SECUNDARIA
shift_unos(0, (128-hc_dim) + (antop_msg->ad_addr.mask) - 1 , aux1);
for(i=0;i<8;i++)  
	aux1->s6_addr16[i]=aux1->s6_addr16[i] | aux2->s6_addr16[i];
memcpy(&sec_addr,aux1,sizeof(struct in6_addr));		
del_and_addnew(aux4,aux1,0);
sleep(2);


//SI YA SE PROCESO LA DIRECCION PRIMARIA, ENTONCES TERMINAMOS EL PROCESO DE MEZCLA
if(flag_mezcla_primaria==1){ //F 
flag_mezcla=0;
flag_mezcla_primaria=0;
flag_mezcla_secundaria=0;
register_rv();
}


//LIBERO MEMORIA
free(aux1);
free(aux2);
free(aux3);
free(aux4);

return;

}

//--------------------------------------------------------------------------------------------

//MARCO QUE SE ESTA PROCESANDO LA DIRECCION PRIMARIA
flag_mezcla_primaria=1;

//NUMERO DE RED LOCAL
red_numero = antop_msg->ad_addr.red_numero;

//ECUACIONES DE ASIGNACION DE "prim_mask" y prim_mask_inicial"
prim_mask = antop_msg->ad_addr.mask;
prim_mask_inicial = prim_mask; 

//SETEO LA MAXIMA DISTANCIA PARA CALCULAR EL VECINO MAS CERCANO AL ROOT
neighbor_to_root.distance=99; 

//ASIGNACION DE DIRECCION LOCAL
shift_unos(0, (128-hc_dim) + (antop_msg->ad_addr.mask) - 1 , aux1);
for(i=0;i<8;i++)  
	aux1->s6_addr16[i]=aux1->s6_addr16[i] | aux2->s6_addr16[i];

//BORRO TABLA DE NOMBRES
clear_rv_tables();

//CICLO DE ENVIO DE PAQUETES MAR2
if(flag_sin_sucesores == 0 || flag_neigh_by_second==1)
for(it=neigh_table,i=0; it != NULL; it=it->next,i++){
	if(memcmp(&(antop_msg->prim_addr),&(it->prim_addr),sizeof(struct in6_addr)) ){ //que no envia mar al nodo que le envio el mar que esta procesando
		if(sec_addr_flag==1 && dist(aux3,*it)>1)  //Esto se hace para que no se forme bucles de MAR, si se tiene sec_addr, solo le envio a los vecinos que no se tiene enlace secundario.
			delete_neighbor(it);
		else{
		prim_mask=prim_mask + 1;
		send_mar2(socket_global,&prim_addr,&(it->prim_addr),aux1,prim_mask);
		delete_neighbor(it);//borro dicho vecino ya que va cambiar de direccion
		flag=1;
			}		
		}	
	}

//SI NO SE ENVIO NINGUN PAQUETE MAR2, ENTONCES QUE SE ASIGNE YA LA DIRECCION RESULTANTE
if(flag==0){
	send_man2(socket_global,&prim_addr,&(antop_msg->prim_addr), &(antop_msg->src_addr), prim_mask, red_numero);
	memcpy(&prim_addr,aux1,sizeof(struct in6_addr));		
	del_and_addnew(aux3,aux1,1);
	sleep(2);
	rcvd_addr=NULL;			
	if(sec_addr_flag == 0 || flag_mezcla_secundaria == 1){ //F // En el caso de que el nodo tenga direccion secundaria, entonces tenemos que ver si se termino el proceso de la direccion secundaria termino
		flag_mezcla=0;
		flag_mezcla_primaria=0;
		flag_mezcla_secundaria=0;
		register_rv();
		}	
	}
else send_man2(socket_global,&prim_addr,&(antop_msg->prim_addr), &(antop_msg->src_addr), prim_mask, red_numero);

//LIBERO MEMORIA
free(aux1);
free(aux2);
free(aux3);
free(aux4);

return;
}


void process_man1(struct ctrl_pkt* antop_msg){

int i;
struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
struct in6_addr *aux2=malloc(sizeof(struct in6_addr));
struct in6_addr *aux3=malloc(sizeof(struct in6_addr));
struct in6_addr *aux4=malloc(sizeof(struct in6_addr));
struct recovered_addr *it2;

memcpy(aux1,&prim_addr,sizeof(struct in6_addr));
memcpy(aux2,&root,sizeof(struct in6_addr));
memcpy(aux3,&(antop_msg->src_addr),sizeof(struct in6_addr));
memcpy(aux4,&prim_addr,sizeof(struct in6_addr));

//SI ES EL CASO QUE ES EL NODO UE GENERO LA MEZCLA, O SEA EL NODO QUE TIENE LA DIRECCION DEFAULT
if(flag_mezcla==1 && !memcmp(&prim_addr,&dir_default,sizeof(struct in6_addr)) ){
	del_and_addnew(&prim_addr,&prim_addr_aux,2);
	sleep(2);
	memcpy(&prim_addr,&prim_addr_aux,sizeof(struct in6_addr));
	if(antop_msg->ad_addr.mask == prim_mask) 
		prim_mask=prim_mask + 1; //agrego uno mas a la mascara ya que todo el arbol de red que se introduce a la red depende del este nodo.
	flag_mezcla=0;
	}

//SI ESTA ACTIVO EL PROCESO DE MEZCLA
if(flag_mezcla == 1){
	shift_mezcla(*aux3,aux1,antop_msg->ad_addr.mask, prim_mask_mezcla);
	memcpy(&prim_addr,aux1,sizeof(struct in6_addr));
	del_and_addnew(aux4,aux1,1);
	sleep(2);
	for(it2=rcvd_addr; it2!=NULL; it2=it2->next){
		shift_mezcla(*aux3,&(it2->prim_addr),antop_msg->ad_addr.mask, prim_mask_mezcla);
		it2->prim_mask=it2->prim_mask + antop_msg->ad_addr.mask - (prim_mask_mezcla-1);
			}
	if(sec_addr_flag == 0 || (flag_mezcla_primaria == 1  &&  flag_mezcla_secundaria == 1)){ //F // En el caso de que el nodo tenga direccion secundaria, entonces tenemos que ver si se termino el proceso de la direccion primaria y secundaria		
		flag_mezcla=0;
		flag_mezcla_primaria=0;
		flag_mezcla_secundaria=0;
		register_rv();
		}	
	}

//LIBERO MEMORIA
free(aux1);
free(aux2);
free(aux3);
free(aux4);

return;
}





void process_man2(struct ctrl_pkt* antop_msg){

struct in6_addr *aux1=malloc(sizeof(struct in6_addr));
struct in6_addr *aux2=malloc(sizeof(struct in6_addr));

memcpy(aux1,&prim_addr,sizeof(struct in6_addr));
memcpy(aux2,&(antop_msg->src_addr),sizeof(struct in6_addr));

if(flag_mezcla==1){
	memcpy(&prim_addr,&(antop_msg->src_addr),sizeof(struct in6_addr));		
	del_and_addnew(aux1,aux2,1);
	sleep(2);
	rcvd_addr=NULL;
	if(sec_addr_flag == 0 || (flag_mezcla_primaria == 1  &&  flag_mezcla_secundaria == 1)){ //F // En el caso de que el nodo tenga direccion secundaria, entonces tenemos que ver si se termino el proceso de la direccion primaria y secundaria				
		flag_mezcla=0;
		flag_mezcla_primaria=0;
		flag_mezcla_secundaria=0;
		register_rv();
			}
		}
//LIBERO MEMORIA
free(aux1);
free(aux2);

return;

}


















