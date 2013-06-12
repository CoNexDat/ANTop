#include <sys/time.h>
#include <stdio.h>
#include <netinet/in.h>
#include "antop_packet.h"
#include "labels.h"
#include "defs.h"
#include "timer_queue.h"


struct recovered_addr *rcvd_addr;
struct in6_addr prim_addr;
struct in6_addr sec_addr;
unsigned char prim_mask;
unsigned char sec_mask;
int sec_addr_flag = 0;
int prim_addr_flag;
extern int hc_dim;
extern int socket_global;
extern int ifindex;
extern struct neighbor *neigh_table;
// This is a flag to indicate if we are waiting for a response about an address
//proposition
int addr_avai=1;
struct in6_addr *rcvd_offered = NULL;




int prim_addr_init(){

    memset(&prim_addr, 0, sizeof(struct in6_addr));
/*
    prim_addr.s6_addr16[0] = 0x80fe;
    prim_addr.s6_addr16[7] = 0x0100;
*/
    prim_addr.s6_addr16[0] = 0x0120;
    prim_addr.s6_addr16[7] = 0x0100;

    memset(&sec_addr, 0, sizeof(struct in6_addr));

    prim_addr_flag = 0;
    
    prim_mask = 0;
    sec_mask = 0;
    rcvd_addr = NULL;
    return 0;
}


int set_timer(struct timeval *tv, time_t sec)
{
    gettimeofday(tv,NULL);
    tv->tv_sec+=sec;
    return 1;
}




int check_timer(struct timeval tv, time_t sec)
{
    struct timeval ctv;
    gettimeofday(&ctv,NULL);
    if( (ctv.tv_sec == tv.tv_sec) )
    {
        gettimeofday(&tv,NULL);
        tv.tv_sec+=sec;
        return 1;
    }
    else
        return 0;
}




int send_pap(int sock, struct in6_addr dst){    

    struct ctrl_pkt *antop_msg;
    struct additional_addr *ad_addr;
    int word_index, bit_index;
    uint16_t aux, mask, power;
    struct recovered_addr *rcvd_aux;
    struct recovered_addr *prev;

    if((antop_msg = malloc(sizeof(struct ctrl_pkt))) == NULL)
        fprintf(stderr,"Could not allocate space for ctrl message \n");
    
    if((ad_addr = malloc(sizeof(struct additional_addr))) == NULL)
        fprintf(stderr,"Could not allocate space for ad addr message \n");
    
    antop_msg->pkt_type_f = PAP;
    memcpy(&(antop_msg->prim_addr), &prim_addr, sizeof(struct in6_addr));     
    
    // First we look for a recovered address
    // For now we assign the last recovered address.
    
    if(rcvd_addr != NULL && rcvd_offered == NULL){
        for(prev = NULL, rcvd_aux = rcvd_addr; rcvd_aux->next != NULL; prev = rcvd_aux, rcvd_aux = rcvd_aux->next){
            fprintf(stderr, "Recovered address: %s; MASK: %d", ip6_to_str(rcvd_aux->prim_addr), rcvd_aux->prim_mask);
        }        
        memcpy(&(antop_msg->ad_addr.addr), &(rcvd_aux->prim_addr), sizeof(struct in6_addr));
        antop_msg->ad_addr.mask = rcvd_aux->prim_mask;
        antop_msg->prim_addr_len = hc_dim;
        rcvd_offered = malloc(sizeof(struct in6_addr));
        memcpy(rcvd_offered, &(rcvd_aux->prim_addr), sizeof(struct in6_addr));

// This is wrong we only give the address space when we get the PAN
/*
        if(prev == NULL){
            // It means that the last one is the first entry
         
            rcvd_addr = NULL;
            free(rcvd_aux);            
        }else{
         
            prev->next = NULL;
            free(rcvd_aux);
        }
*/
        
    }else{
        

    if( prim_mask >= hc_dim )
        antop_msg->ad_addr.mask = 0;
    else{

        memcpy(&(antop_msg->ad_addr.addr), &prim_addr, sizeof(struct in6_addr));
        bit_index =  15 - (prim_mask % 16);
        //Word Index starts in four since we only use 64 of the 128 IPv6 address bits

        word_index = 4 + (prim_mask / 16);
        mask = htons(pow(2,bit_index));
        aux = antop_msg->ad_addr.addr.s6_addr16[word_index] & mask;
        power = pow(2, bit_index);

        if(aux)
                antop_msg->ad_addr.addr.s6_addr16[word_index] -= htons(power);  // REVISAR ESTO, ES LO MISMO QUE MASK
        else
                antop_msg->ad_addr.addr.s6_addr16[word_index] += htons(power);


        //*************************************************************
        //  PRUEBA DE DIRECCIONES SECUNDARIAS

/*
        struct in6_addr auxiliar_address;

         antop_msg->ad_addr.addr.s6_addr16[0]=0x0120;
    antop_msg->ad_addr.addr.s6_addr16[1]=0x0000;
    antop_msg->ad_addr.addr.s6_addr16[2]=0x0000;
    antop_msg->ad_addr.addr.s6_addr16[3]=0x0000;
    antop_msg->ad_addr.addr.s6_addr16[4]=0x00c0;
    antop_msg->ad_addr.addr.s6_addr16[5]=0x0000;
    antop_msg->ad_addr.addr.s6_addr16[6]=0x0000;
    antop_msg->ad_addr.addr.s6_addr16[7]=0x0000;
*/



        //*************************************************************



        antop_msg->ad_addr.mask = prim_mask +1;
        antop_msg->prim_addr_len = hc_dim;

    }   
    
    }
    antop_socket_send((unsigned char *)antop_msg, dst, sizeof(struct ctrl_pkt), 1, sock, ANTOP_PORT);

    return 0;

}

int send_panc(int sock, struct in6_addr dst, struct in6_addr primary, struct in6_addr chosen_addr){



    struct ctrl_pkt *antop_msg;
    struct additional_addr *ad_addr;
    struct recovered_addr *rcvd_aux;
    struct recovered_addr *prev;

    if((antop_msg = malloc(sizeof(struct ctrl_pkt))) == NULL)
        fprintf(stderr,"Could not allocate space for ctrl message \n");

    if((ad_addr = malloc(sizeof(struct additional_addr))) == NULL)
        fprintf(stderr,"Could not allocate space for ad addr message \n");

    fprintf(stderr, "PANC enviara a %s\n",ip6_to_str(dst));

    antop_msg->pkt_type_f = PANC;
    antop_msg->ad_addr.mask = 5;
    memcpy(&(antop_msg->ad_addr.addr), &primary, sizeof(struct in6_addr));

    ad_addr->conn_count = 0;
    ad_addr->mask = 0;
    ad_addr->t_lenght =0;

    //**********************************************************************************
    // FOR SECONDARY ADDRESS TESTING WE COMENT THE FOLLOWING LINE
    if(rcvd_offered == NULL || ((rcvd_offered != NULL) && (memcmp(&chosen_addr, rcvd_offered, sizeof(struct in6_addr)))))
        prim_mask++;
    else if(!memcmp(&chosen_addr, rcvd_offered, sizeof(struct in6_addr))){
        // The recovered address has been chosen
        for(prev = NULL, rcvd_aux = rcvd_addr; rcvd_aux->next != NULL; prev = rcvd_aux, rcvd_aux = rcvd_aux->next);
        if(prev == NULL){
            // It means that the last one is the first entry

            rcvd_addr = NULL;
            free(rcvd_aux);
        }else{
         
            prev->next = NULL;
            free(rcvd_aux);
        }

    }


    //**********************************************************************************

    antop_socket_send((unsigned char *)antop_msg, dst, sizeof(struct ctrl_pkt), 1, sock, ANTOP_PORT);

    return 0;

}




int send_par(struct in6_addr *src, int sock){

    struct ctrl_pkt *par = NULL;
    struct in6_addr destination = IN6ADDR_ANY_INIT;

    par = (ctrl_pkt*) malloc(sizeof(struct ctrl_pkt));

    u_int8_t hop_l;
    hop_l = 20;    
    par->pkt_type_f = PAR;
    
    memcpy(&(par->src_addr), src, sizeof(struct in6_addr));

/*
    MULTICAST ADDRESS
*/

    destination.s6_addr16[0]=0x02ff;
    destination.s6_addr16[1]=0x0000;
    destination.s6_addr16[2]=0x0000;
    destination.s6_addr16[3]=0x0000;
    destination.s6_addr16[4]=0x0000;
    destination.s6_addr16[5]=0x0000;
    destination.s6_addr16[6]=0x0000;
    destination.s6_addr16[7]=0x0100;

    antop_socket_send(((unsigned char *)par), destination, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);
    
    return 0;

}



int receive_pap(int sock, struct ctrl_pkt ** antop_msg1){

    struct ctrl_pkt *antop_msg;       
    unsigned char aux;
    if(antop_msg = malloc(sizeof(struct ctrl_pkt)) == NULL){
        
        fprintf(stderr,"Couldn't locate space for antop_msg \n");
        return NULL;
    };        
    
    if(antop_socket_read(sock, (unsigned char **)&(antop_msg))<0)
        return -1;    
    
    aux= antop_msg->pkt_type_f;
    fprintf(stderr,"Receive PAP pkt_type: %u \n",antop_msg->pkt_type_f);
/*
    fprintf(stderr,"Primary address is %s \n",ip6_to_str(antop_msg->prim_addr));
    fprintf(stderr,"Source address is %s \n",ip6_to_str(antop_msg->src_addr));
    fprintf(stderr,"Total length is %u \n",antop_msg->t_length);
*/
    *antop_msg1 = antop_msg;    

    /*
     *  If mask = 0, then there are no available addresses to be given.
     */

    fprintf(stderr,"PKT TYPE: %u \n",antop_msg->pkt_type_f);
    fprintf(stderr,"MASK: %u \n",antop_msg->ad_addr.mask);

    if( antop_msg->pkt_type_f == PAP && antop_msg->ad_addr.mask)
        return 0;
    else
        return 1;
}



int send_pan(struct in6_addr *addr, int sock, struct in6_addr *chosen_addr){

    struct ctrl_pkt *pan;
    pan=malloc(sizeof(struct ctrl_pkt));
    u_int8_t hop_l;
    hop_l = 20;

    memset(pan, 0, sizeof(struct ctrl_pkt));

    memcpy(&(pan->ad_addr.addr), addr, sizeof(struct in6_addr));

    // Here prim_addr holds the chosen addr.
    memcpy(&(pan->prim_addr), chosen_addr, sizeof(struct in6_addr));
    get_if_addr(&(pan->src_addr));   
    
    pan->pkt_type_f = PAN;

    struct in6_addr destination;

/*
    MULTICAST ADDRESS
*/

    destination.s6_addr16[0]=0x02ff;
    destination.s6_addr16[1]=0x0000;
    destination.s6_addr16[2]=0x0000;
    destination.s6_addr16[3]=0x0000;
    destination.s6_addr16[4]=0x0000;
    destination.s6_addr16[5]=0x0000;
    destination.s6_addr16[6]=0x0000;
    destination.s6_addr16[7]=0x0100;

    antop_socket_send((unsigned char *)pan, destination, sizeof(struct ctrl_pkt), hop_l, sock, ANTOP_PORT);

    return 0;


}



void send_san(struct in6_addr* dst, struct in6_addr* sec_addr){
    struct ctrl_pkt *san;
    san=malloc(sizeof(struct ctrl_pkt));
    u_int8_t hop_l =20;

    memset(san, 0, sizeof(struct ctrl_pkt));

    memcpy(&(san->ad_addr.addr), sec_addr, sizeof(struct in6_addr));
    get_if_addr(&(san->src_addr));

    san->pkt_type_f = SAN;
    struct in6_addr destination;

    memcpy(&destination, dst, sizeof(struct in6_addr));

    fprintf(stderr,"A SAN will be send to %s", ip6_to_str(destination));

    antop_socket_send((unsigned char *)san, destination, sizeof(struct ctrl_pkt), hop_l, socket_global, ANTOP_PORT);

    return 0;

}




int get_prim_addr(struct in6_addr *src, int sock){

    int i, recvd_paps, j, n, k, mask_index, mask_min, rec_pap;
    struct timeval tv;
    struct timeval timeout;
    static struct ctrl_pkt *antop_msg;
    struct ctrl_pkt *v_pap[MAX_RCVD_PAP];
    struct ctrl_pkt hb;
    struct hb_param *params;

    if(antop_msg = malloc(sizeof(struct ctrl_pkt)) == NULL){
        
        fprintf(stderr,"Couldn't locate space for antop_msg \n");
        return;
    };    

    
start:    i = N_PAR;
          recvd_paps = 0;
          j=0;

    /*
     *  Set to NULL all the values in the PAPs vector
     */

    memset(v_pap, 0, MAX_RCVD_PAP * sizeof(struct ctrl_pkt *));
    



    timeout.tv_sec = T_PAP;
    timeout.tv_usec = 0;

    fd_set read_fd;

    FD_ZERO(&read_fd);
    FD_SET(sock, &read_fd);



    while(i>0){

        send_par(src, sock);
        set_timer(&tv,T_PAP);
        printf("PAR sent. Waiting for PAPs.\n");
        fprintf(stderr, "PAR sent. Waiting for PAPs.\n");

        while(1){
            
            if((n = select(sock+1, &read_fd, NULL, NULL, &timeout))<0)     

                fprintf(stderr,"main.c: Failed select (get primary address) \n");

            if(n>0){
            
            if( FD_ISSET( sock, &read_fd ))                     

                rec_pap = receive_pap(sock, &antop_msg);
                
                if(rec_pap == 0){
                    recvd_paps++;
                    v_pap[j] = malloc(sizeof(struct ctrl_pkt));
                    memcpy(v_pap[j], antop_msg, sizeof(struct ctrl_pkt));  
                    fprintf(stderr,"PRIM ADDR: %s \n", ip6_to_str(v_pap[j]->prim_addr));

                    j++;
                } else if(rec_pap == 1)
                    recvd_paps == NO_ADDR_AVLB;

                else if(rec_pap == -1)
                    recvd_paps = NULL_ADDR_SET;
                
            }
            if (check_timer(tv,T_PAP)==1){
                printf("PAPs received: %u\n",recvd_paps);       // recvd_paps: available addresses
                fprintf(stderr, "PAPs received: %u\n",recvd_paps);       // recvd_paps: available addresses
                break;
            }
            if (j >= MAX_RCVD_PAP)
                break;
        }
        i--;
        if(recvd_paps > 0)
            break;
    }

    /*
     *  recvd_paps > 0 means that there are available adresses. Then search
     *  for the one with the lowest mask
     */

    if(recvd_paps > 0){
        
        mask_index = 0;
        mask_min = v_pap[0]->ad_addr.mask;
                  
    
        for(k=1; k < ((MAX_RCVD_PAP > j) ? j : MAX_RCVD_PAP ); k++){
//            fprintf(stderr, "The type is: %u \n",(v_pap[k])->pkt_type_f);
            if(v_pap[k]->ad_addr.mask < mask_min){
                
                mask_min = v_pap[k]->ad_addr.mask;
                mask_index = k;
                
            }
        }
        fprintf(stderr,"La direcciòn elegida es %s con máscara %u \n",ip6_to_str(v_pap[mask_index]->ad_addr.addr),mask_min);


        fprintf(stderr, "Sending PAN \n");
//        send_pan(&(v_pap[mask_index]->ad_addr.addr), sock);

        /*
         * We send the address of the node from which we choose our new address.
         */
        
        send_pan(&(v_pap[mask_index]->prim_addr), sock, &(v_pap[mask_index]->ad_addr.addr));


         fd_set read_fd;

        FD_ZERO(&read_fd);
        FD_SET(sock, &read_fd);

        timeout.tv_sec = T_PANC;
        timeout.tv_usec = 0;

        if((n = select(sock+1, &read_fd, NULL, NULL, &timeout))<0)     

            fprintf(stderr,"main.c: Failed select (get primary address) \n");        

        if(n>0){

            if( FD_ISSET( sock, &read_fd )){                     

                 antop_socket_read(sock, (unsigned char **)&(antop_msg));

                 fprintf(stderr,"AD ADDR: %s \n", ip6_to_str(antop_msg->ad_addr.addr));
                 fprintf(stderr,"PRIM ADDR: %s \n", ip6_to_str(v_pap[mask_index]->prim_addr));
                 fprintf(stderr,"CHOOSEN ADDR: %s \n", ip6_to_str(v_pap[mask_index]->ad_addr.addr));
                 fprintf(stderr,"MASK GUARDADA: %d \n", v_pap[mask_index]->ad_addr.mask);
                 fprintf(stderr,"PKT TYPE: %d \n", antop_msg->pkt_type_f);



                 if(antop_msg->pkt_type_f == PANC && !memcmp(&(antop_msg->ad_addr.addr), &(v_pap[mask_index]->prim_addr), sizeof(struct in6_addr))){
                     fprintf(stderr, "YA SE PUEDE UTILIZAR LA DIR OBTENIDA %s\n",ip6_to_str(v_pap[mask_index]->ad_addr.addr));
                     set_if_addr2(&(v_pap[mask_index]->ad_addr.addr)/*, ifindex*/, 1);
                     params = malloc(sizeof(struct hb_param));
                     
                     params->sock = &socket_global;
                     params->timer = 0;                     
                     send_hb(params);
//                     send_hb(&socket_global, 0);
                     
                     // Besides sending the heart beat, we add the predecesor as a neighbor
                     // This hb is a false one for adding the neighbor

                     
                     memcpy(&(hb.prim_addr), &(v_pap[mask_index]->prim_addr), sizeof(struct in6_addr));
                     hb.ad_addr.mask = v_pap[mask_index]->ad_addr.mask;
                     process_hb(&hb);
                     memcpy(&prim_addr, &(v_pap[mask_index]->ad_addr.addr), sizeof(struct in6_addr));
                     
                     //Set the Hipercube Dimension as the predecesor
                     hc_dim=v_pap[mask_index]->prim_addr_len;
                     prim_mask = v_pap[mask_index]->ad_addr.mask;
                     return ADDR_OK;
                 }
            }
        }

        goto start;    
    }
    
    /*
     *  This means that we have neighbors but there's no address available
     */

    if(recvd_paps == NO_ADDR_AVLB)
        return NO_ADDR_AVLB;

    /*
     *  No PAPs. The primary address has been already initialized. Use that one
     *
     */

        
    set_if_addr2(&prim_addr/*, ifindex*/, 1);
    prim_mask = 0;
    return NULL_ADDR_SET;
}

void san_t_handler(){
    fprintf(stderr,"The SAN timer has expired\n");
    if(!addr_avai)
        addr_avai =1;
    return;
}

void send_sap(int sock, struct in6_addr *dst, struct in6_addr *sec_addr, int sec_mask){

    struct ctrl_pkt *antop_msg;
    struct additional_addr *ad_addr;
    //This is the timer to wait for the SAN
    struct timer* san_t;
    
    if(!addr_avai){
        fprintf(stderr,"We have proposed an address, waiting for the response\n");
        return;
    }

    if((antop_msg = malloc(sizeof(struct ctrl_pkt))) == NULL){
        fprintf(stderr,"Could not allocate space for ctrl message \n");
        return;
    }

    if((ad_addr = malloc(sizeof(struct additional_addr))) == NULL){
        fprintf(stderr,"Could not allocate space for ad addr message \n");
        return;
    }

    antop_msg->pkt_type_f = SAP;
    memcpy(&(antop_msg->prim_addr), &prim_addr, sizeof(struct in6_addr));
    memcpy(&(antop_msg->src_addr), &prim_addr, sizeof(struct in6_addr));
    memcpy(&(antop_msg->ad_addr.addr), sec_addr, sizeof(struct in6_addr));
    antop_msg->ad_addr.mask = sec_mask;
    antop_msg->prim_addr_len = hc_dim;

    san_t = malloc(sizeof(struct timer));
    if(san_t == NULL){
        fprintf(stderr,"Could not locate space for the timer in send sap\n");
        return;
    }
    san_t->handler = &san_t_handler;

    san_t->used = 0;
    timer_add_msec(san_t, T_SAN);

    antop_socket_send((unsigned char *)antop_msg, *dst, sizeof(struct ctrl_pkt), 1, sock, ANTOP_PORT);
    return;
}

void process_san(struct ctrl_pkt* antop_msg){

    struct recovered_addr *prev;
    struct recovered_addr *rcvd_aux;

    // We must look for the address that has just been accepted

    // First we look for the confirmed address between the recovered ones

    if(rcvd_addr != NULL){
        for(prev = NULL, rcvd_aux = rcvd_addr; rcvd_aux != NULL; prev = rcvd_aux, rcvd_aux = rcvd_aux->next){
            fprintf(stderr, "Recovered address: %s; MASK: %d", ip6_to_str(rcvd_aux->prim_addr), rcvd_aux->prim_mask);
            if(!memcmp(&(rcvd_aux->prim_addr), &(antop_msg->ad_addr.addr), sizeof(struct in6_addr))){
                break;
            }
        }
        if(rcvd_aux != NULL){
        // It is a recovered address!
            if(rcvd_aux->next == NULL){
                prev=NULL;
                free(rcvd_aux);
            }else{
                prev=rcvd_aux->next;
                free(rcvd_aux);
            }
            return;
        }
    }
    // We are here so we have no recovered address or this is not one of them, so it was in our
    // address space.
    prim_mask++;
    return;


}

void process_sap(struct ctrl_pkt* msg){

    if(!sec_addr_flag){
    struct ctrl_pkt* hb;

    hb = malloc(sizeof(struct ctrl_pkt));

    memcpy(&(hb->prim_addr), &(msg->src_addr), sizeof(struct in6_addr));
    hb->ad_addr.mask = msg->ad_addr.mask;


    memcpy(&sec_addr, &(msg->ad_addr.addr), sizeof(struct in6_addr));
    set_if_addr2(&(msg->ad_addr.addr)/*, ifindex*/, 0);
    sec_mask = msg->ad_addr.mask;
    fprintf(stderr,"Salimos OK de set_if_addr\n");
    sec_addr_flag = 1;
    // First create the table if it doesn't exist
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
    add_neighbor(hb, 1);
    fprintf(stderr,"Salimos OK de add neighbor \n");
    send_san(&(msg->src_addr), &(msg->ad_addr.addr));
}
    return;

}


void chk_sec_addr(struct neighbor *neigh){
    
    // First we look for a recovered address
    
    struct recovered_addr *rcvd_aux;
    struct recovered_addr *prev;    
    int word_index, bit_index;
    uint16_t aux, mask, power;
    struct in6_addr aux_addr;
    fprintf(stderr,"NEIGHBOR ADDRESS chk sec addr: %s\n", ip6_to_str(neigh->prim_addr));
    if(rcvd_addr != NULL){
        for(prev = NULL, rcvd_aux = rcvd_addr; rcvd_aux->next != NULL; prev = rcvd_aux, rcvd_aux = rcvd_aux->next){
            fprintf(stderr,"Distance between neighbor and address to give %d\n", dist(&(aux_addr), *neigh));
            if(dist(&(rcvd_aux->prim_addr), *neigh) == 1){            
                fprintf(stderr, "Recovered address: %s has distance 1 with neighbor %s \n", ip6_to_str(rcvd_aux->prim_addr), ip6_to_str(neigh->prim_addr));
                send_sap(socket_global, &(neigh->prim_addr), &(rcvd_aux->prim_addr), rcvd_aux->prim_mask);
                return;
            }
        }       
    }
    fprintf(stderr,"NEIGHBOR ADDRESS chk sec addr: %s\n", ip6_to_str(neigh->prim_addr));
    // No recovered addresses to offer as a secondary one for this neighbor
    // Now we check our address space for available addresses.
    if( prim_mask >= hc_dim ){
        //It means that there are no available addresses
        return;
    }
    // Else we check if the address to give has unitary distance to the neighbor

    memcpy(&(aux_addr), &prim_addr, sizeof(struct in6_addr));
    bit_index =  15 - (prim_mask % 16);
    //Word Index starts in four since we only use 64 of the 128 IPv6 address bits

    word_index = 4 + (prim_mask / 16);
    mask = htons(pow(2,bit_index));
    aux = aux_addr.s6_addr16[word_index] & mask;
    power = pow(2, bit_index);
    if(aux)
        aux_addr.s6_addr16[word_index] -= htons(power);
    else
        aux_addr.s6_addr16[word_index] += htons(power);

    fprintf(stderr,"Address to give is %s\n", ip6_to_str(aux_addr));

    fprintf(stderr,"Neighbor address is %s\n", ip6_to_str(neigh->prim_addr));

    fprintf(stderr,"Distance between neighbor and address to give %d\n", dist(&(aux_addr), *neigh));

//    if(dist(&(aux_addr), *neigh) == 1){
        fprintf(stderr,"A SAP will be send, address within address space\n");
        send_sap(socket_global, &(neigh->prim_addr), &(aux_addr), prim_mask +1);        
//    }
    return;
}



/*          Control Packet
	________________________________________________
	|  Pkt Type (5) |  Flags (3) | Total Lenght (8) |
	|-----------------------------------------------|
	|               MAC ADDRESS (48)                |   Esto se cambia por la direccion IPv6 origen
	|-----------------------------------------------|
	|           Primary Address Length (8)          |
	|-----------------------------------------------|
	|                 Primary Address (n)           |
	|-----------------------------------------------|
	|                                               |
	.                   Optional Headers            .
	|_______________________________________________|
*/




