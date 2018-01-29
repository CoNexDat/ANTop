#include<stdio.h>
#include <netinet/ip6.h>
#include"antop_packet.h"
#include "timer_queue.h"
#include "labels.h"
#include "defs.h"


extern struct in6_addr prim_addr;

extern struct in6_addr sec_addr;

extern int prim_addr_flag;

extern unsigned char prim_mask;

extern unsigned char sec_mask;

extern unsigned char red_numero;//pablo

extern int flag_mezcla;//pablo

extern int flag_fragmentacion;//pablo2

extern unsigned char prim_mask_inicial;//pablo4

extern struct recovered_addr *rcvd_addr;

struct timer hb_t;

struct timer chk_neigh_t;

struct hb_param params;

extern int sec_addr_flag;

struct neighbor *neigh_table = NULL;

void send_hb(struct hb_param *params);
void check_neighbors();
void print_neighbors();





void heart_beat_init(int *sock){

    
    params.sock = sock;
    params.timer = 1;

    hb_t.handler = &send_hb;       
    hb_t.data = &params;
    hb_t.used = 0;
    strcpy(hb_t.name, "heart beat");
    


    timer_add_msec(&hb_t, T_HB);
    
    chk_neigh_t.handler = &check_neighbors;
    chk_neigh_t.data = NULL;
    chk_neigh_t.used = 0;
    strcpy(hb_t.name, "check neighbor");
    
    timer_add_msec(&chk_neigh_t, T_LHB);
    


}




void send_hb(struct hb_param *par){

    struct ctrl_pkt *hb;
    struct in6_addr destination;

    if(flag_fragmentacion == 0 && flag_mezcla == 0){//pablo2   //pablo3
	check_if_i_am_only();//pablo4    	
	check_neigh_of_secondary_address();//pablo4
	check_n_s_l_table();//pablo4
		}//pablo4

    hb = malloc(sizeof(struct ctrl_pkt));

    if(hb == NULL){
        fprintf(stderr,"Could not locate space for HB packet \n");
        return;
    };
    if(par->timer)
        timer_add_msec(&hb_t, T_HB);

    destination.s6_addr16[0]=0x02ff;
    destination.s6_addr16[1]=0x0000;
    destination.s6_addr16[2]=0x0000;
    destination.s6_addr16[3]=0x0000;
    destination.s6_addr16[4]=0x0000;
    destination.s6_addr16[5]=0x0000;
    destination.s6_addr16[6]=0x0000;
    destination.s6_addr16[7]=0x0100;

    hb->pkt_type_f = HB;
    //get_if_addr(&(hb->src_addr));//pablo4
    hb ->ad_addr.t_lenght=prim_mask_inicial;//pablo4 //agrego en este campo para mandar la informacion de la mascara inicial a los nodos
    hb ->ad_addr.mask = prim_mask;
    hb ->ad_addr.red_numero=red_numero;//pablo
    memcpy(&(hb->prim_addr), &prim_addr, sizeof(struct in6_addr));
    memcpy(&(hb->src_addr), &prim_addr, sizeof(struct in6_addr));//pablo4
    memcpy(&(hb->ad_addr.addr), &sec_addr, sizeof(struct in6_addr));//pablo4 

    fprintf(stderr,"A heart beat will be send\n");
    if(flag_fragmentacion == 0 &&  flag_mezcla == 0)//pablo2    //pablo3
    antop_socket_send((unsigned char *)hb, destination, sizeof(struct ctrl_pkt), 1, *(par->sock), ANTOP_PORT);
    
    if(sec_addr_flag){
        memcpy(&(hb->prim_addr), &sec_addr, sizeof(struct in6_addr));
        hb->ad_addr.mask = sec_mask;
        //memcpy(&(hb->src_addr), &sec_addr, sizeof(struct in6_addr));//pablo4
        fprintf(stderr,"A heart beat will be send to secondary address");
	if(flag_fragmentacion == 0 && flag_mezcla == 0)//pablo2   //pablo3
        antop_socket_send((unsigned char *)hb, destination, sizeof(struct ctrl_pkt), 1, *(par->sock), ANTOP_PORT);    
	}

   return;

}


void add_neighbor(struct ctrl_pkt *hb, int sec_flag){

    struct neighbor *it = neigh_table;
    struct in6_addr aux;

    memset(&aux, 0, sizeof(struct in6_addr));

    if(!memcmp(&(neigh_table->prim_addr), &aux, sizeof(struct in6_addr))){
        goto add;
    }else{
        while(1){        
            if(it->next == NULL){            
                it->next = malloc(sizeof(struct neighbor));
                if(it->next == NULL){
                    fprintf(stderr,"Could not locate space for new neighbor\n");
                    return;
                }
                break;
            }        
            it = it->next;
        }
    }
    it=it->next;
add:it->count =0;
    it->next = NULL;
    memcpy(&(it->prim_addr), &(hb->prim_addr), sizeof(struct in6_addr));
    //it->visited_bitmap = 0;
    it->available = 1;
    it->prim_mask = hb->ad_addr.mask;
    it->prim_mask_inicial = hb->ad_addr.t_lenght;//pablo4
    it->sec_addr_conn = 0;
    if(sec_flag)
        it->sec_addr_conn = 1;

    return;
};


int delete_neighbor(struct neighbor* neigh){

    fprintf(stderr,"The neighbor %s will be deleted\n", ip6_to_str(neigh->prim_addr));

    struct neighbor *prev, *curr;
    if (neigh == NULL)
	return -1;
    
//    t->used = 0;
    for (prev = NULL, curr = neigh_table; curr != NULL; prev = curr, curr = curr->next) {
//	if (curr == neigh) {
        if(!memcmp(&(curr->prim_addr), &(neigh->prim_addr), sizeof(struct in6_addr))){
            if (prev == NULL)
		neigh_table = curr->next;
	    else
		prev->next = curr->next;            
            rv_table_delete(&(curr->prim_addr));            
//	    neigh->next = NULL;
	    return 0;
	}
    }        
    fprintf(stderr, "The neighbor was not found \n");
    /* We didn't find the neighbor... */
    return -1;
}


struct neighbor *find_neighbor(struct in6_addr *p_addr){

    struct neighbor *it = neigh_table;
    struct in6_addr aux;

    memset(&aux, 0, sizeof(struct in6_addr)); 

    if(!memcmp(&aux, &(neigh_table->prim_addr), sizeof(struct in6_addr)))
        return NULL;

    while(it != NULL){
        fprintf(stderr,"SEARCHING %s\n", ip6_to_str(it->prim_addr));
        if(!memcmp(p_addr, &(it->prim_addr), sizeof(struct in6_addr))){
            // This means we found the neighbor
            return it;
        };
        it = it->next;
    }
    //The neighbor is not present
    return NULL;


};


void process_hb(struct ctrl_pkt *hb){

    struct neighbor *aux;
    struct neighbor aux2;//pablo4
    struct neighbor aux3;//pablo4
    struct in6_addr aux4;//pablo4
    int flag_aux=0, flag_detector_mezcla;//pablo3
    struct in6_addr addr_aux;

    if(flag_fragmentacion ==1) //pablo2
	return; //pablo2

    if(flag_mezcla==1)  //pablo3
	return; //pablo3

    flag_detector_mezcla=detector_de_mezcla(hb);//pablo3   Detector de mezcla
    if(flag_detector_mezcla == -1 || flag_detector_mezcla ==1)//pablo3
    	return;//pablo3


    if(!memcmp(&(hb->prim_addr), &prim_addr, sizeof(struct in6_addr))){
        fprintf(stderr,"HB from this primary address, nothing to be done\n");
        return;
    }

    if(!memcmp(&(hb->prim_addr), &sec_addr, sizeof(struct in6_addr))){
        fprintf(stderr,"HB from this secondary address, nothing to be done\n");
        return;
    }

    // We only save it as a neighbor if the distance is one.

    aux = malloc(sizeof(struct neighbor));
    if(aux==NULL){
        fprintf(stderr,"Could not locate space in memory for process_hb\n");
        return;
    }

    memcpy(&(aux->prim_addr), &(hb->prim_addr), sizeof(struct in6_addr));
    memcpy(&(aux2.prim_addr), &(hb->src_addr), sizeof(struct in6_addr));//pablo4
    memcpy(&(aux3.prim_addr), &(hb->ad_addr.addr), sizeof(struct in6_addr));//pablo4
    memset(&aux4,0,sizeof(struct in6_addr));//pablo4  //como la dist(de algo , con el vector nulo) siempre da 1, entonces se acompaña de comparar si la direccion secundaria del paquete hb es nulo, entonces que obvie la operacion dist(prim_addr,  aux3) o dist(sec_addr,  aux3)


    //Do not offer sec address unless we have obtained a definitive primary address

    if(dist(&prim_addr, *aux) != 1 && prim_addr_flag){
        if(!sec_addr_flag){
            fprintf(stderr,"The distance to this node is not one, so it can't be our neighbor\n");
	    if(dist(&prim_addr, aux2)!=1 &&  (dist(&prim_addr, aux3)!=1 || !memcmp(&(hb->ad_addr.addr),&(aux4),sizeof(struct in6_addr))))//pablo4
            	chk_sec_addr1(aux);// chk_sec_addr(aux); //pablo4
            return;
        }else{
            if(dist(&sec_addr, *aux) != 1){
                fprintf(stderr,"The distance to this node sec addr is not one, so it can't be our neighbor\n");
		if(dist(&sec_addr, aux2)!=1 &&  (dist(&sec_addr, aux3)!=1 || !memcmp(&(hb->ad_addr.addr),&(aux4),sizeof(struct in6_addr))))//pablo4                
			chk_sec_addr1(aux);//chk_sec_addr(aux);//pablo4
                return;
            }
            flag_aux=1;
        }
    }

    // First create the table if it doesn't exist

    init_neigh_table();

    // Now check if the neighbor already exists in table

    fprintf(stderr,"A HB from neighbor %s has been received\n", ip6_to_str(hb->prim_addr));
    aux = find_neighbor(&(hb->prim_addr));
    if(aux == NULL){        
        fprintf(stderr, "This neighbor did not exist in the table. Adding it\n");
        add_neighbor(hb, flag_aux);
    }
    // The node is already in table
    else{
        aux->count =0;
        aux->prim_mask = hb->ad_addr.mask;
        fprintf(stderr,"The neighbor already existed in the table\n");
    }
    print_neighbors();
    detector_de_fragmentacion();//pablo
    return;
}




int check_succesor(struct neighbor *neigh){

    int different_words = 0, i, j = 0;
    uint16_t mask, aux1, aux2;
    int succesor_flag=0;

    mask = pow(2,15);

    // The idea is to check that address are equal until the first bit that changes and then
    // the remaining bits are zero

    for(i = 4; i <= 7; i++){

        if(prim_addr.s6_addr16[i] != neigh->prim_addr.s6_addr16[i]){
            fprintf(stderr,"16 bit word number %d for primary address: %x\n",i ,htons(prim_addr.s6_addr16[i]));
            fprintf(stderr,"16 bit word number %d for neighbor address: %x\n",i ,htons(neigh->prim_addr.s6_addr16[i]));
            fprintf(stderr,"The mask: %x\n", mask);
            j = 0;
            different_words++;
            aux1 = htons(prim_addr.s6_addr16[i]) & mask;
 
            fprintf(stderr,"primary address del nodo mascareada %x\n", htons(aux1));
            aux2 = htons(neigh->prim_addr.s6_addr16[i]) & mask;
            fprintf(stderr,"primary address del neighbor mascareada %x\n", aux2);
//            while(!memcmp(&(prim_addr.s6_addr16[i] & mask), &(neigh->prim_addr.s6_addr16[i] & mask))){
            while(aux1 == aux2){
                mask = mask >> 1;
                j++;
                fprintf(stderr, "bit number %d is the same\n", j);
                aux1 = htons(prim_addr.s6_addr16[i]) & mask;
                aux2 = htons(neigh->prim_addr.s6_addr16[i]) & mask;

            }
 //           while(!memcmp(&(prim_addr.s6_addr16[i] & mask), &(neigh->prim_addr.s6_addr16[i] & mask)) && (prim_addr.s6_addr16[i] & mask) == 0 && j < 16){
            if(!aux1)
                succesor_flag=1;
            mask = mask>>1;
            fprintf(stderr, "bit number %d is different\n", j);
            fprintf(stderr,"the mask: %x\n", mask);
            aux1 = htons(prim_addr.s6_addr16[i]) & mask;
            aux2 = htons(neigh->prim_addr.s6_addr16[i]) & mask;
            

            fprintf(stderr, "AUX1: %x\n", aux1);
            fprintf(stderr,"AUX2: %x\n", aux2);
            fprintf(stderr,"primary address enmascarada %x\n",htons(prim_addr.s6_addr16[i]) & mask);

            while(aux1 == aux2 && (htons(prim_addr.s6_addr16[i]) & mask) == 0 && j < 16){
                fprintf(stderr,"Now we compare the rest of the bits in this word\n");

                mask = mask >> 1;
                j++;
                aux1 = htons(prim_addr.s6_addr16[i]) & mask;
                aux2 = htons(neigh->prim_addr.s6_addr16[i]) & mask;
            }

            if (j != 16){
                fprintf(stderr, "Not only one bit is different so it is not our neighbor\n");
                // It is not our succesor
                return 0;
            }
        }
    }
    // if it is our succesor, then the primary address is zero for the changing bit
    if(different_words == 1 && succesor_flag)
        // Only one word should be diferent. Since there is only one diferent bit between
        // the two addresses for the succesor.
        return 1;
    else{
        fprintf(stderr,"There is more than one different word or it is our predecesor, so it can't be our succesor\n");
        return 0;
    }
}



void recover_addr_space(struct neighbor *neigh){

    struct recovered_addr *aux_rcvd;

    fprintf(stderr,"The following address space will be recovered: %s\n", ip6_to_str(neigh->prim_addr));
    if(rcvd_addr == NULL){
        rcvd_addr = malloc(sizeof(struct recovered_addr));
        if(rcvd_addr == NULL){
            fprintf(stderr, "Could not allocate space for recovered address \n");
            return;
        }
        aux_rcvd = rcvd_addr;        
    }else{
        for(aux_rcvd = rcvd_addr; aux_rcvd->next != NULL; aux_rcvd = aux_rcvd->next);        
        aux_rcvd->next=malloc(sizeof(struct recovered_addr));
        if(aux_rcvd->next   == NULL){
            fprintf(stderr, "Could not allocate space for recovered address \n");
            return;
        }
        aux_rcvd = aux_rcvd->next;        
    }    
    aux_rcvd->next = NULL;    
    memcpy(&(aux_rcvd->prim_addr), &(neigh->prim_addr), sizeof(struct in6_addr));    
    aux_rcvd->prim_mask = neigh->prim_mask_inicial;//neigh->prim_mask; //pablo4
    fprintf(stderr,"The following address has been recovered: %s\n", ip6_to_str(aux_rcvd->prim_addr));
    return;
}


void print_neighbors(){


    struct neighbor *it;

    for(it = neigh_table; it != NULL; it = it->next){
        fprintf(stderr, "NEIGHBOR: %s \n", ip6_to_str(it->prim_addr));
        fprintf(stderr, "NEIGHBOR COUNT: %d \n", it->count);
        fprintf(stderr, "NEIGHBOR MASK: %d \n", it->prim_mask);
	fprintf(stderr, "NEIGHBOR INITIAL MASK: %d \n", it->prim_mask_inicial); //pablo4
    }

    return;

}

void check_neighbors(){

    struct neighbor *it;

    fprintf(stderr, "CHECKING NEIGHBORS\n");
    
    for(it = neigh_table; it != NULL; it = it->next){

        if(it ->count == 0)
            it->count = -1;


        else if(it->count == -1)
            it->count = 1;
        // This -1 should be changed when the hb is received
        else if(it->count >0 && it->count < (N_INACT) )
            it->count += 1;
        else if(it->count >= N_INACT){
            fprintf(stderr,"We need to delete one neighbor, check if it is a succesor\n");
            if(check_succesor(it)){
                fprintf(stderr,"The disconnected neighbor is a succesor\n");
                recover_addr_space(it);
            }else
                fprintf(stderr,"The disconnected neighbor is not a succesor\n");
            delete_neighbor(it);
        }
    }
    print_neighbors();
    fprintf(stderr,"Print neighbors ok\n");
    timer_add_msec(&chk_neigh_t, T_LHB);
    return;
}


//RESTA UN ELEMENTO A LA TABLA DEL ESPACIO DE DIRECCIONES A RECUPERAR
int del_rcvd_addr_table(struct in6_addr *addr/*, unsigned char mask*/){  //pablo4


    struct recovered_addr *prev, *curr;
    
    for (prev = NULL, curr = rcvd_addr; curr != NULL; prev = curr, curr = curr->next) {	
	if(!memcmp(&(curr->prim_addr), addr, sizeof(struct in6_addr))){
		if (prev == NULL)
			rcvd_addr=curr->next;
		else prev->next=curr->next;			
		return 0;
			}
     		}
	return -1;
}


//INICIALIZA LA TABLA DE VECINOS
void init_neigh_table(){  //pablo4

if(neigh_table == NULL){
        fprintf(stderr, "Neighbor table does not exist. Creating it...\n");
        neigh_table = malloc(sizeof(struct neighbor));
        if(neigh_table == NULL){
            fprintf(stderr,"Could not create the neighbor table\n");
            return;
        }
        neigh_table->next = NULL;
        memset(&(neigh_table->prim_addr), 0, sizeof(struct in6_addr));
    }

}

