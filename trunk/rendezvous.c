#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <math.h>
#include <string.h>
#include "timer_queue.h"
#include "antop_packet.h"
#include "labels.h"
#include "defs.h"

struct rv_vector {
    struct in6_addr *addr;
    struct rv_vector *next;
};

struct lookup_table {
    struct in6_addr hc_addr;
    char univ_addr[20];
    struct lookup_table *next;
};

//  This is the DNS query queue

struct dns_q_queue {
    unsigned char *pkt;
    size_t size;
    int dst_port;
    int src_port;
    struct dns_q_queue *next;
};


extern char *univ_addr;
extern int hc_dim;
extern struct in6_addr prim_addr;
extern struct in6_addr sec_addr;
static struct timer rv_t;
// This is the server table with registered nodes
struct lookup_table *rv_table = NULL;
// This is the client table with the cached entries
struct lookup_table *cached_table = NULL;
extern int socket_global;
extern int socket_global_mdns;
extern int socket_global_dns;
//These are the servers that solve our universal address.
static struct rv_vector *rv_servers = NULL;
//These are the servers that solve the univ address of the dest node
static struct rv_vector *rv_servers_dst = NULL;
struct dns_q_queue *dns = NULL;
struct in6_addr main_rv_server;
extern int sec_addr_flag;


void rv_tables_init() {

// I think that it is better to initialize this function in rv_table_add. This solves the problem of
// not using the first struct.
//    rv_table = malloc(sizeof (struct lookup_table));
    cached_table = malloc(sizeof (struct lookup_table));
//    dns = malloc(sizeof (struct dns_q_queue));
/*
    if (rv_table == NULL || cached_table == NULL || dns == NULL) {
        fprintf(stderr, "rv_tables_init: Could not locate space for rv tables\n");
        return;
    }
*/
//    rv_table->next = NULL;
    cached_table->next = NULL;
    memset(cached_table->univ_addr, 0,20);
    //cached_table->univ_addr = NULL;
//    dns->next = NULL;
    return;
}

/**
 * @brief Hash the Universal Address generating a Hypercube address from it.
 *
 * @return a Hypercube address based on the Universal Address.
 */
void hash_2_hc(char *u_addr, long long *hash, struct in6_addr *rel_addr) {

    struct in6_addr *addr;
    uint16_t aux;
    int i, j;
    uint16_t mask;

    addr = malloc(sizeof (struct in6_addr));
    if (addr == NULL) {
        fprintf(stderr, "Hash to Hypercube: Could not locate space for hc\n");
        return NULL;
    }

    memset(addr, 0, sizeof (struct in6_addr));
//    addr->s6_addr16[0] = 0x80fe;
    addr->s6_addr16[0] = 0x0120;

    //8 bytes for long long

    long long hc[8];
    hc[0] = 0;
    for (i = 0; i < strlen(u_addr); i++) {
        hc[0] = hc[0] * 31 + u_addr[i];
    }
    for (i = 1; i < 8; i++) {
        hc[i] = (hc[i - 1] + 17) * 31;
    }

    if (hash != NULL)
        memcpy(hash, &hc, sizeof (long long));
    // hc_dim bits are taken from this hash

    // POR AHORA SE QUE ME ALCANZA CON hc[0] => REVISAR ESTO!!!!!

    

    for (i = 0; i < (hc_dim / 16); i++)
        memcpy(&(addr->s6_addr16[4 + i]), (uint16_t *) (hc + 2 * i), sizeof (uint16_t)); //  CHEQUEAR ESTO !!!!!!!!!!!

    

    for (j = 0; j < (hc_dim % 16); j++) {
        mask = htons(pow(2, j));
        addr->s6_addr16[4 + i] += mask & (*((uint16_t *) (hc + 2 * i)));

    }
    if (rel_addr != NULL)
        memcpy(rel_addr, addr, sizeof (struct in6_addr));
    return;

    //  return HypercubeAddress((byte *) hc,bitLength);
}

// dst is a boolean used to determine if the servers are for our univ_addr or another one

void distribute_in_hc(int server_count, char *u_addr, int dst) {

    int branch_bits = 0, n;
    double m, hash1, b_bits_aux;
    long long numeric, top;
    int last_bit, rem_zeros, i, j;
    //    struct rv_vector *rv_servers=NULL;
    struct rv_vector *rv_servers_aux;
    struct in6_addr *aux;
    struct rv_vector *rv_servers_prev;
    uint16_t mask;
    unsigned char *mask_aux;
    uint16_t *addr_aux;
    float fract;
    //    long long mask;
    //    long long *addr_aux;

    // Determine how many branch bits to use
    
    //The modf functions break value into the integral and fractional parts, 
    //each of which has the same sign as the argument. They return the fractional part, 
    //and store the integral part (as a floating-point number) in the object pointed to by second argument.
        
    fract = modf(log2(server_count), &b_bits_aux);
    branch_bits = b_bits_aux;    
    
    //fract = log2(server_count);

//    fract = modf((log2(16)), &b_bits_aux);



    branch_bits += fract == 0 ? 0 : 1;
    


    fprintf(stderr,"El resto de los BRANCH BITS es %f\n", fract);

    fprintf(stderr, "SE DISTRIBUIRAN LOS %d SERVERS RV, con % d BRANCH BITS\n", server_count, branch_bits);


    // Now determine the last bit.
    n = hc_dim - branch_bits;
    m = exp(n - 1) + 1;
    // We now need a random , reproducible hash function from 0 to 1. so we use the base of the RVServer hash calculation, divided
    // by the space

    hash_2_hc(u_addr, &numeric, NULL);

    fprintf(stderr, "El resultado de la funcion de HASH es %d \n", numeric);

    memset(&top, 1, sizeof (long long));
    hash1 = (double) numeric / (double) top;
    double sum = log(hash1 * m + 1);
    double roundSum = 0;
    modf(sum, &roundSum);
    last_bit = roundSum + 1;
    // The number of zeros behind the last bit
    rem_zeros = hc_dim - branch_bits - last_bit;
    //Determine the branches. There will be one server in each branch.

    if(!dst){

        rv_servers = malloc(sizeof (struct rv_vector));
        if (rv_servers == NULL) {
            fprintf(stderr, "Distribute_in_hc: Could not locate space for RendezVous servers\n");
            return NULL;
        }
        rv_servers_aux = rv_servers;
    }else{
        if(rv_servers_dst == NULL){
            rv_servers_dst = malloc(sizeof(struct rv_vector));
            rv_servers_aux = rv_servers_dst;
        }
    }


    for (i = 0; i < server_count; i++, rv_servers_aux = rv_servers_aux->next) {
        aux = malloc(sizeof (struct in6_addr));
        hash_2_hc(u_addr, NULL, aux);

        fprintf(stderr, "La IPV6 CALCULADA ES %s\n", ip6_to_str(*aux));
        rv_servers_aux->addr = malloc(sizeof (struct in6_addr));
        if (rv_servers_aux->addr == NULL) {
            fprintf(stderr, "Distribute_in_hc: Could not locate space for RendezVous servers\n");
            return NULL;
        }
        memcpy(rv_servers_aux->addr, aux, sizeof (struct in6_addr));


        //This is the address with the raw hash. Now set the branch bits.
        mask = 0;
        for (j = 0; j < branch_bits; j++)
//            mask += htons(pow(2, j));
            mask += pow(2, j);
        
        //In order to set the branch bits we will need a mask. First we will
        //use it to clear the number of necessary bits.
        
        fprintf(stderr,"LA MASCARA VALE : %x\n", mask);
    
        // We will use one 16 bit's word for branch bits at most.
        // Take the mask to the beginning of the word.
        mask = mask << (16 - branch_bits);
        fprintf(stderr,"LA MASCARA VALE : %x\n", mask);
        // Clear the branch bits.
        
        mask = ~mask;
        addr_aux = &(rv_servers_aux->addr->s6_addr16[4]);

        fprintf(stderr,"LA MASCARA VALE : %x\n", mask);


        *addr_aux = htons((*addr_aux) & htons(mask));
        fprintf(stderr,"LA DIR ENMASCARADA VALE : %x\n", *addr_aux);

        mask = i;

        fprintf(stderr,"LA NUEVA MASCARA VALE : %x\n", mask);

        mask = mask << (16 - branch_bits);

        fprintf(stderr,"BRANCH BITS VALE : %x\n", mask);

//        *addr_aux = htons((*addr_aux) | mask);
/*
        mask = 0;
        for (j = 0; j < last_bit; j++)
            //mask += pow(2, j);
            mask++;
        mask = ~mask;
*/
//        (*addr_aux) | mask;
        rv_servers_aux->addr->s6_addr16[4] = htons(*addr_aux) | htons(mask);

        rv_servers_aux->next = malloc(sizeof (struct in6_addr));
        if (rv_servers_aux->next == NULL) {
            fprintf(stderr, "Distribute_in_hc: Could not locate space for RendezVous servers\n");
            return NULL;
        }
        rv_servers_prev = rv_servers_aux;
    }


    //There's one register that is empty
    free(rv_servers_aux);
    rv_servers_prev->next = NULL;
    return;
}

void register_rv() {

    struct rv_ctrl_pkt *rv_msg;
    struct rv_vector *rv_servers_aux;
    struct neighbor neigh;
    struct in6_addr destination;
//    int min_dist = 1000;
//    int dist;


    //First param is the number of servers. In this case, it will be log2(nodes) i.e. dimensions of the hc
    //    rv_servers=distribute_in_hc(hc_dim, univ_addr);
    if (rv_servers == NULL) {
        distribute_in_hc(hc_dim, univ_addr, 0);

        fprintf(stderr, "register_rv: Se generará el mensaje rv_register\n");
    }
    rv_msg = malloc(sizeof (struct rv_ctrl_pkt));
    rv_msg->type = RV_REG;
    memcpy(&(rv_msg->prim_addr), &prim_addr, sizeof (struct in6_addr));
/*
    rv_msg->univ_addr = malloc(strlen(univ_addr));
    if (rv_msg->univ_addr == NULL) {
        fprintf(stderr, "register_rv: Could not locate space for rv_msg\n");
        return NULL;
    }
*/

//    memcpy(&(rv_msg->univ_addr), univ_addr, strlen(univ_addr));
    memcpy(rv_msg->univ_addr, univ_addr, strlen(univ_addr));

    timer_add_msec(&rv_t, T_RV);

    fprintf(stderr, "SE ENVIARA LA REGISTRACION RV DE LA DIRECCION PRIMARIA\n");

    for (rv_servers_aux = rv_servers; rv_servers_aux != NULL; rv_servers_aux = rv_servers_aux->next) {
        fprintf(stderr, "LA IP del SERVER RV es : %s", ip6_to_str(*(rv_servers_aux->addr)));
        antop_socket_send((unsigned char *) rv_msg, *(rv_servers_aux->addr), sizeof (struct rv_ctrl_pkt), 1, socket_global, ANTOP_PORT);
    }

    if(sec_addr_flag){
        fprintf(stderr, "SE ENVIARA LA REGISTRACION RV DE LA DIRECCION SECUNDARIA\n");
        memcpy(&(rv_msg->prim_addr), &sec_addr, sizeof(struct in6_addr));
        for(rv_servers_aux = rv_servers; rv_servers_aux != NULL; rv_servers_aux = rv_servers_aux->next){
            fprintf(stderr,"LA IP del SERVER RV es : %s\n", ip6_to_str(*(rv_servers_aux->addr)));
            antop_socket_send((unsigned char *) rv_msg, *(rv_servers_aux->addr), sizeof(struct rv_ctrl_pkt));
        }
    }

    return;

}

void timer_rv_init(int sock) {

    rv_t.handler = &register_rv;
    rv_t.data = sock;
    rv_t.used = 0;
    strcpy(rv_t.name, "RendezVous");

    timer_add_msec(&rv_t, T_RV);
}

void rv_table_add(struct rv_ctrl_pkt *rv_pkt) {

    struct lookup_table *rv_table_aux;
    char add_entry = 0;
    char str_aux[20];
    struct neighbor *aux;

    // We only save it as a neighbor if the distance is one.

    aux = malloc(sizeof(struct neighbor));
    if(aux==NULL){
        fprintf(stderr,"Could not locate space in memory for process_hb\n");
        return;
    }

    memcpy(&(aux->prim_addr), &(rv_pkt->prim_addr), sizeof(struct in6_addr));

    fprintf(stderr,"NEIGHBOR ADDRESS antes: %s\n", ip6_to_str(aux->prim_addr));

//    This is actually wrong, there is no need for the node who register to be our neighbor

/*
    if(dist(&prim_addr, *aux) != 1){
        fprintf(stderr,"The distance to this node is not one, so It can't be our neighbor\n");
        if(dist(&prim_addr, *aux) != 0){
            fprintf(stderr,"The distance to this node is not zero either, so it is not this node\n");
            return;
        }
    }
*/

    
    fprintf(stderr,"An entry will be added to the RV table, since a registration request has been received\n");

    if (rv_table == NULL) {       
        
        // The table was empty, so we know for sure that we need to add this entry
        add_entry = 1;
    }else{        
        //The table already existed so look for the requested entry
        for(rv_table_aux = rv_table; rv_table_aux != NULL; rv_table_aux = rv_table_aux->next){
            
            if(!strcmp(&rv_table_aux->univ_addr, &rv_pkt->univ_addr)){                
                fprintf(stderr,"An entry for this Universal Address already existed. No new entries will be created\n");
                //The entry already existed for the universal address. Update the Hc address if necessary
                if(memcmp(&(rv_table_aux->hc_addr), &(rv_pkt->prim_addr), sizeof(struct in6_addr)))
                    memcpy(&(rv_table_aux->hc_addr), &(rv_pkt->prim_addr), sizeof(struct in6_addr));
                break;
                
            }
        }
        if(rv_table_aux == NULL){
            //The entry does not exist yet. Add it
            add_entry = 1;
        }
        
    }   
    
    if(add_entry){

        if(rv_table == NULL){
            fprintf(stderr,"The RV table did not exist. Creating it...\n");
            rv_table = malloc(sizeof (struct lookup_table));
            if (rv_table == NULL) {
                fprintf(stderr, "rt_table_add: Could not locate space for RV Table\n");
                return;
            }
            rv_table_aux = rv_table;
        }else{
        
            for(rv_table_aux = rv_table; rv_table_aux->next != NULL; rv_table_aux = rv_table_aux->next);
            rv_table_aux->next = malloc(sizeof(struct lookup_table));
            if (rv_table_aux->next == NULL) {
                fprintf(stderr, "rv_table_add: Could not locate space for new entry \n");
                return;
            }

            rv_table_aux = rv_table_aux->next;
//        rv_table_aux->univ_addr = malloc(strlen(&rv_pkt->univ_addr));
        }
        memset(rv_table_aux->univ_addr, 0, strlen(&rv_pkt->univ_addr));

        fprintf(stderr, "El tamaño de la direccion universal del paquete es %d\n", strlen(&rv_pkt->univ_addr));
//        rv_table_aux->hc_addr = malloc(sizeof (struct in6_addr));
/*
        if (rv_table_aux->univ_addr == NULL) {
                fprintf(stderr, "rv_table_add: Could not locate space for adding an entry to the rv_table\n");
                return;
        }
*/
  //      memcpy(&(rv_table_aux->univ_addr), &(rv_pkt->univ_addr), strlen(&(rv_pkt->univ_addr)));
        strcpy(str_aux, &(rv_pkt->univ_addr));
        strcpy(rv_table_aux->univ_addr, str_aux);
 //       strcpy(&(rv_table_aux->univ_addr), &(rv_pkt->univ_addr));
        memcpy(&(rv_table_aux->hc_addr), &(rv_pkt->prim_addr), sizeof(struct in6_addr));
        rv_table_aux->next = NULL;
        fprintf(stderr,"Auxiliar string %s\n", str_aux);
        fprintf(stderr,"Primary address received: %s\n",&rv_pkt->univ_addr);
        fprintf(stderr, "Primary address to be added: %s\n", ip6_to_str(rv_table_aux->hc_addr));
        fprintf(stderr, "Universal address to be added: %s\n", &(rv_table_aux->univ_addr));
        fprintf(stderr, "Universal address to be added Lenght: %d\n", strlen(&(rv_table_aux->univ_addr)));
    }
    return;

}


void rv_table_delete(struct in6_addr* addr){
    
    struct lookup_table *curr;
    struct lookup_table *prev;
    
    fprintf(stderr,"The entry with HCA %s will be deleted\n", ip6_to_str(*addr));
    
    if(rv_table == NULL){ 
        fprintf(stderr,"There are no entries in the RV table\n");
        return;
    }        
    
    for(curr = rv_table, prev = NULL; (curr != NULL); prev = curr, curr = curr->next){
        if(memcmp(&(curr->hc_addr), addr, sizeof(struct in6_addr))){
            fprintf(stderr, "HCA: %s\n", ip6_to_str(curr->hc_addr));
        }else{
            break;
        }
    }
    
    if(curr == NULL){
        fprintf(stderr, "Could not find the entry to be deleted in the rv table\n");
        return;
    }    
    
    if(prev == NULL){
        // The entry to be deleted is the first one
        rv_table = curr->next;
        free(curr);
        return;
    }       
    prev->next = curr->next;
    fprintf(stderr,"The space in memory will now be deleted\n");
    free(curr);
    return;          
    
}

struct in6_addr *solve_name(unsigned char *uaddr, int sock) {

    struct lookup_table *cached_table_aux;
    struct rv_ctrl_pkt *rv_msg;
//    struct in6_addr *rv_server;
    struct rv_vector *rv_servers_aux;
    struct neighbor neigh;
    int dist;
    int min_dist = 1000;
    
    fprintf(stderr,"This is solve_name, solving the address, %s\n", uaddr);

    for (cached_table_aux = cached_table; cached_table_aux != NULL; cached_table_aux = cached_table_aux->next)
        if (cached_table_aux->univ_addr != NULL)
            if (!memcmp(uaddr, cached_table_aux->univ_addr, strlen(uaddr) < strlen(cached_table_aux->univ_addr) ? strlen(uaddr) : cached_table_aux->univ_addr))
                break;
    // This means the address is on the cache, so use that       
    if (cached_table_aux != NULL)
        return &(cached_table_aux->hc_addr);

    // The address it is not in the cache, so solve it

    rv_msg = malloc(sizeof (struct rv_ctrl_pkt));
    if (rv_msg == NULL) {
        fprintf(stderr, "solve_name: Could not locate space for rv_msg\n");
        return NULL;
    }    

    rv_msg->type = RV_ADDR_LOOKUP;
    memcpy(&(rv_msg->prim_addr), &prim_addr, sizeof (struct in6_addr));

/*
    rv_msg->univ_addr = malloc(strlen(uaddr));
    if (rv_msg->univ_addr == NULL) {
        fprintf(stderr, "solve_name: Could not locate space for rv_msg\n");
        return NULL;
    }   
*/

    memcpy(rv_msg->univ_addr, uaddr, strlen(uaddr));    
//    strcpy(rv_msg->univ_addr, uaddr);
    memcpy(&(rv_msg->src_addr), &(prim_addr), sizeof(struct in6_addr));   

    
    
    fprintf(stderr,"Address in the ADDR_LOOKUP pkt: %s\n", rv_msg->univ_addr);
    // Now we have to send this packet to the RV server.
    // Just for now, we choose any of them. Later we should check for the one that is closer to this node REVISAR ESTO!!!!!!!!!!!!
/*
    rv_server = malloc(sizeof(struct in6_addr));
    if(rv_server == NULL){
        fprintf(stderr,"No memory available\n");
        return;
    }
*/
    // Now we have to determine the closest server RV to solve this address

    
    // After this the RV servers will be saved in rv_servers_dst.
    distribute_in_hc(hc_dim, uaddr,1);
    // Now we have to determine the closest to the dst.
    for (rv_servers_aux = rv_servers_dst; rv_servers_aux != NULL; rv_servers_aux = rv_servers_aux->next) {
        memcpy(&(neigh.prim_addr), rv_servers_aux->addr, sizeof(struct in6_addr));
            if((dist = dist_w_mask(&prim_addr, neigh)) < min_dist ){
                min_dist = dist;
                memcpy(&main_rv_server, rv_servers_aux->addr, sizeof(struct in6_addr));
            }
    }


    antop_socket_send((unsigned char *) rv_msg, main_rv_server, sizeof (struct rv_ctrl_pkt), 1, sock, ANTOP_PORT);
//    fprintf(stderr,"An address lookup message will be send to %s\n", ip6_to_str(*(rv_server)));

    return NULL;
}



void answer_rv_query(struct rv_ctrl_pkt *rv_pkt, int sock) {

    struct lookup_table *rv_table_aux;
    struct rv_ctrl_pkt *rv_msg;
    
    fprintf(stderr,"We have entered answer_dns_query\n");
    
    fprintf(stderr,"We are looking for the following address: %s\n", rv_pkt->univ_addr);

    for (rv_table_aux = rv_table; rv_table_aux != NULL; rv_table_aux = rv_table_aux->next){
        fprintf(stderr,"Universal Address entry: %s\n", rv_table_aux->univ_addr);
 //       if (!memcmp(rv_pkt->univ_addr, rv_table_aux->univ_addr, strlen(rv_pkt->univ_addr)))
          if (!memcmp(rv_pkt->univ_addr, rv_table_aux->univ_addr, strlen(rv_table_aux->univ_addr)))
            break;
    }

    rv_msg = malloc(sizeof (struct rv_ctrl_pkt));
    if (rv_msg == NULL) {
        fprintf(stderr, "solve_name: Could not locate space for rv_msg\n");
        return NULL;
    }

    rv_msg->type = RV_ADDR_SOLVE;

    if (rv_table_aux != NULL) {
        // i.e. the requested entry was found
        rv_msg->flags = rv_msg->flags | MASK_SOLVED;
        memcpy(&(rv_msg->prim_addr), &(rv_table_aux->hc_addr), sizeof (struct in6_addr));
    } else
        rv_msg->flags = 0;

    memcpy(&(rv_msg->univ_addr), &(rv_pkt->univ_addr), strlen(rv_pkt->univ_addr));

    antop_socket_send((unsigned char *) rv_msg, rv_pkt->src_addr, sizeof (struct rv_ctrl_pkt), 1, sock, ANTOP_PORT);

}

/*  As RFC 2292 states in section 3, complete IPv6 packets cannot be accesed through raw sockets. It is
 *  needed to answer the MDNS query with a MDNS_PORT as source. But it's not possible to register
 *  a UDP socket in this port. So the only option I see right now is to "NAT ports" i.e. to register
 *  an auxiliary port and then to change the port in a hook.
 */

// source_port is the port from where we need to send the response (to this same node)
// It could be 5353 for MDNS or 53 for DNS. In the first case we should use socket_global_mdns
// and in the second case socket_global_dns


void answer_dns_query(unsigned char *data, int size, int dest_port, char solved, int source_port) {

    unsigned char *data_aux;
    unsigned char *data_aux2;
    struct in6_addr destination;
    struct in6_addr destination_aux;
    struct in6_addr addr;
    struct in6_addr *aux = NULL;
    struct dns_q_queue *dns_aux;
    struct dns_q_queue *dns_aux_prev;
    unsigned char *udp;
    uint8_t aux_2;
    uint16_t aux_3;
    int idx;
    unsigned char *ptr_query_dns = NULL;
    size_t size_aux;
    char aux_str[150];
    int i;
    int src_port;

    //The difference between a query and an answer is...TTL (4); Length (2); data (16) = 22
    // Here debug_pkt info won't be valid in case that the query has been solved
    if(data!=NULL){
    //debug_pkt(data);
    }

    fprintf(stderr,"THIS IS ANSWER_DNS_QUERY \n");
    if(source_port != 0)
        fprintf(stderr,"ESTOY ENTRANDO CON UN SOURCE PORT DE :%d\n", htons(source_port));
    fprintf(stderr,"El tamaño vale %d y solved vale %d\n", size, solved);  


    if (!solved && !size && !dest_port) {
        fprintf(stderr, "ANSWER_DNS_QUERY: The requested address is not in the cache, nor the RV server could solve it\n");
        return;
    }

    // This is the case when a DNS query has been received. So first param (data) is the hole IPv6 packet
    // containing the query. Here we look for the address in the cache. If it is not there,
    // we send a RV query.

    

    if (!solved && size && dest_port) {

        // We look for the address in the cache and we send a RV query if it is not there.       
        // For some reason we have one additional char in the univ addr at the beggining. Delete that one.
        // I think that we are corrupting the packet here. Do not modify it
        
        fprintf(stderr,"PRUEBA1\n");
        
        strcpy(aux_str, data + IPV6_MDNS_QUERY_OFF);
        
        fprintf(stderr,"PRUEBA2\n");
        
//        fprintf(stderr,"La cadena copiada a str aux es %s, con lenght %d\n", aux_str, strlen(aux_str));
 //       memcpy(aux_str, data + IPV6_MDNS_QUERY_OFF, strlen((char *)(data + IPV6_MDNS_QUERY_OFF)));
 //       aux_str=(char *)(data + IPV6_MDNS_QUERY_OFF);
        for(i=0; i < (strlen((char *)(data + IPV6_MDNS_QUERY_OFF))-1); i++){
            *(aux_str+i) = *(aux_str+i+1);
 //           memcpy(data + IPV6_MDNS_QUERY_OFF, data + IPV6_MDNS_QUERY_OFF+i, 1);
        }
        
        
        *(aux_str + strlen((char *)(data + IPV6_MDNS_QUERY_OFF))-1)=0;
        fprintf(stderr,"The first char is %d\n",*aux_str);
        fprintf(stderr,"The second char is %d\n",*(aux_str+1));

        aux = solve_name(aux_str, socket_global);
//            fprintf(stderr,"The following universal address will be looked for in the RV table: %s\n", (char *)(data + IPV6_MDNS_QUERY_OFF));
//            fprintf(stderr,"The following universal address will be looked for in the RV table: %s\n", aux_str);
//            fprintf(stderr,"The length of the address is %d\n", strlen((char *)(data + IPV6_MDNS_QUERY_OFF)));
//            fprintf(stderr,"The length of the address is %d\n", strlen(aux_str));

        

        if (aux != NULL) {
            // The address is already solved in the cache
            // This is the pointer to the MDNS query, which is in this case contained in the first param.
            fprintf(stderr, "This universal address is already solved in the cache\n");

            ptr_query_dns = data + IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN;
            size_aux = size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN);

        }else{

            //  The address is not in the cache, so we need to queue the DNS original query, in order to generate a response later,
            //  a RV address lookup msg has already been sent in solve_name.
            // Add an entry at the end of the dns query queue
            
            // Besides we need to save whether the protocol is DNS or MDNS. For that we use source_port.
            
            fprintf(stderr,"The requested universal address is not cached in the RV table\n");
            
            if(dns == NULL){
                fprintf(stderr,"The DNS query queue did not exist. Creating it...\n");
                dns = malloc(sizeof(struct dns_q_queue));
                if(dns == NULL){
                    fprintf(stderr,"ANSWER DNS QUERY could not locate space for adding a pkt to the queue\n");
                    return;
                }
                dns->next = NULL;
                dns->pkt = malloc((size_t)size);
                if (dns->pkt == NULL) {
                        fprintf(stderr, "Anwer_dns_query: Could not locate space for pkt\n");
                        return;
                }
                memcpy(dns->pkt, data, size);
                dns->size = size;                
                dns->dst_port = dest_port;
                dns->src_port = source_port;
                
                // HASTA ACA ESTA BIEN
                fprintf(stderr, "first byte transaction ID: %x\n", *(dns->pkt + UDP_PKT_OFF));
                fprintf(stderr, "second byte transaction ID: %x\n", *(dns->pkt + UDP_PKT_OFF +1));
                
            }else{

            for (dns_aux = dns; dns_aux->next != NULL; dns_aux = dns_aux->next);

            dns_aux->next = malloc(sizeof (struct dns_q_queue));
            if (dns_aux->next == NULL) {
                fprintf(stderr, "Anwer_dns_query: Could not locate space for pkt\n");
                return;
            }
            dns_aux = dns_aux->next;
            fprintf(stderr,"answer_dns_query will save a packet of %d size\n", (size_t)size);
            dns_aux->pkt = malloc((size_t)size);
            if (dns_aux->pkt == NULL) {
                fprintf(stderr, "Anwer_dns_query: Could not locate space for pkt\n");
                return;
            }
          
            memcpy(dns_aux->pkt, data, size);
            dns_aux->size = size;
            dns_aux->next = NULL;
            dns_aux->dst_port = dest_port;
            dns_aux->src_port = source_port;
            
            fprintf(stderr, "first byte transaction ID: %x\n", *(dns_aux->pkt + UDP_PKT_OFF));
                fprintf(stderr, "second byte transaction ID: %x\n", *(dns_aux->pkt + UDP_PKT_OFF +1));
            }
            return;
        }
    }

    // This is the case when a RV Response has been received. In this case the first param is the
    // RV response.
        
    if(solved){
        fprintf(stderr, "The query has been solved\n");
        // We need to find the MDNS query that has been queued
        for (dns_aux = dns; dns_aux != NULL; dns_aux = dns_aux->next) {
            fprintf(stderr,"DST PORT: %d\n", dns_aux->dst_port);
            // ACA HAY QUE VALIDAR QUE SE COMPAREN DOS NOMBRES QUE SON DISTINTOS, PERO QUE CONCUERDEN EN STRLEN(...)
            // Here we should check that length is the same first
            fprintf(stderr,"Pkt guardado para esta entrada tiene la siguiente direccion universal %s, con tamaño %d\n",dns_aux->pkt + IPV6_MDNS_QUERY_OFF +1, strlen(dns_aux->pkt + IPV6_MDNS_QUERY_OFF)-1);
            fprintf(stderr, "El primer caracter es %d\n", *((dns_aux->pkt)+IPV6_MDNS_QUERY_OFF));
            fprintf(stderr, "El segundo caracter es %d\n", *((dns_aux->pkt)+IPV6_MDNS_QUERY_OFF +1));
            fprintf(stderr, "Query Class vale primer byte: %d\n", *((dns_aux->pkt) + IPV6_MDNS_QUERY_OFF + strlen(dns_aux->pkt +IPV6_MDNS_QUERY_OFF) +3));
            fprintf(stderr, "Query Class vale segundo byte: %d\n", *((dns_aux->pkt) + IPV6_MDNS_QUERY_OFF + strlen(dns_aux->pkt +IPV6_MDNS_QUERY_OFF) +4));
            fprintf(stderr, "Tamaño de la direccion recibida %d\n", strlen(((struct rv_ctrl_pkt *)data)->univ_addr));
            fprintf(stderr, "first byte transaction ID: %x\n", *(dns_aux->pkt + UDP_PKT_OFF));
            fprintf(stderr, "second byte transaction ID: %x\n", *(dns_aux->pkt + UDP_PKT_OFF +1));
            
            fprintf(stderr,"Size for the saved string: %d\n",strlen(dns_aux->pkt + IPV6_MDNS_QUERY_OFF + 1));
            fprintf(stderr,"Saved string: %s\n", dns_aux->pkt + IPV6_MDNS_QUERY_OFF + 1);
            fprintf(stderr,"Size for the packet: %d\n", strlen(((struct rv_ctrl_pkt *) data)->univ_addr));
            fprintf(stderr,"Packet string: %s\n", ((struct rv_ctrl_pkt *) data)->univ_addr);
            
            fprintf(stderr,"Source port saved: %d\n", dns_aux->src_port);
            
            
            if(strlen(dns_aux->pkt + IPV6_MDNS_QUERY_OFF + 1) == strlen(((struct rv_ctrl_pkt *) data)->univ_addr)){
                fprintf(stderr, "The sizes are the same, now we compare the addresses\n");
                if (!memcmp(dns_aux->pkt + IPV6_MDNS_QUERY_OFF + 1, ((struct rv_ctrl_pkt *) data)->univ_addr, strlen(dns_aux->pkt + IPV6_MDNS_QUERY_OFF)-1)) {
                    fprintf(stderr, "The entry exists in the RV table\n");
                   // ptr_query_dns = malloc(dns_aux->size);
                    //memcpy(ptr_query_dns, dns_aux->pkt, dns_aux->size);
                    ptr_query_dns = dns_aux->pkt + UDP_PKT_OFF;
                    fprintf(stderr,"DNS_AUX_SIZE = %d\n", dns_aux->size);
                    size_aux = (dns_aux->size) - 80 + strlen(dns_aux->pkt + IPV6_MDNS_QUERY_OFF)+1 +26;
                    fprintf(stderr,"SIZE_AUX = %d\n", size_aux); 
                    dest_port = dns_aux->dst_port;
                    src_port = dns_aux->src_port;
                    break;
                }
            }
            dns_aux_prev = dns_aux;                   
        }
    } 
    

    // data_aux will contain the MDNS response to this same host
    // We must reserve the size of the query plus the answer of the response
    if(size){
//        data_aux = malloc((size_t) (size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN) + 25 + strlen(data + IPV6_MDNS_QUERY_OFF)));
              data_aux = malloc((size_t) (size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN) + 25 + strlen(data + IPV6_MDNS_QUERY_OFF))+45);
    }else{
//        data_aux = malloc(size_aux);
                data_aux = malloc(size_aux +42);
    }

    fprintf(stderr, "ORIGINALMENTE SON BYTES (TODO EL PAQUETE) %d \n", size);
    fprintf(stderr, "ESTOY PIDIENDO BYTES %d \n", size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN) + 25 + strlen(data + IPV6_MDNS_QUERY_OFF));
    memcpy(data_aux, ptr_query_dns, size_aux - strlen(dns_aux->pkt + IPV6_MDNS_QUERY_OFF)-1 -26);


    // We need to change some of the query fields
    //Change the flags field to be 0x8400 i.e. Standard Query Response
    
    fprintf(stderr, "Transaction ID for stored pkt (first byte): %x\n", *(data_aux));
    fprintf(stderr, "Transaction ID for stored pkt (second byte): %x\n", *(data_aux +1));
    
    fprintf(stderr, "Transaction ID for stored pkt (first byte) PTR_QUERY_DNS: %x\n", *(ptr_query_dns));
    fprintf(stderr, "Transaction ID for stored pkt (second byte) PTR_QUERY_DNS: %x\n", *(ptr_query_dns +1));

  //  *(data_aux + 2) = 0x84;
      *(data_aux + 2) = 0x85;
    *(data_aux + 3) = 0;

    // One Answer...

    *(data_aux + 6) = 0;
    *(data_aux + 7) = 1;
    
    // One authority 
    
    *(data_aux + 8) = 0;
    *(data_aux + 9) = 1;
    
    // One additional
    
    *(data_aux + 10) = 0;
    *(data_aux + 11) = 1;
    
    // Class = 1
    
    *(data_aux + MDNS_QUERY_OFFSET + strlen(data_aux + MDNS_QUERY_OFFSET) + 3) = 0;
    *(data_aux + MDNS_QUERY_OFFSET + strlen(data_aux + MDNS_QUERY_OFFSET) + 4) = 1;

    // Now insert the answer in the response MDNS pkt

    data_aux2 = data_aux + MDNS_QUERY_OFFSET + 5 + strlen(data_aux + MDNS_QUERY_OFFSET);

    // Copy the name to solve from the Question to the Answer plus the type and class fields

    memcpy(data_aux2, ptr_query_dns + MDNS_QUERY_OFFSET, strlen(data_aux + MDNS_QUERY_OFFSET) + 5);

    // TTL field

    data_aux2 = data_aux2 + strlen(data_aux + MDNS_QUERY_OFFSET) + 5;

    *(data_aux2) = 0;
    *(data_aux2 + 1) = 0;
    *(data_aux2 + 2) = 0;
    *(data_aux2 + 3) = 0x0a;

    //Data length (ipv6 address -> 16 bytes)
    *(data_aux2 + 4) = 0;
    *(data_aux2 + 5) = 16;
    //Now copy the solved address to the MDNS response
/*
    destination.s6_addr16[0]=0x80fe;
    destination.s6_addr16[1]=0x0000;
    destination.s6_addr16[2]=0x0000;
    destination.s6_addr16[3]=0x0000;
    destination.s6_addr16[4]=0x0000;
    destination.s6_addr16[5]=0x0000;
    destination.s6_addr16[6]=0x0000;
    destination.s6_addr16[7]=0x0100;
*/
    
    memcpy(&destination, &prim_addr, sizeof (struct in6_addr));       

    if (aux != NULL) {
        memcpy(data_aux2 + 6, aux, sizeof(struct in6_addr));
    } else{
        destination_aux.s6_addr16[0]=0x0120;
    destination_aux.s6_addr16[1]=0x0000;
    destination_aux.s6_addr16[2]=0x0000;
    destination_aux.s6_addr16[3]=0x0000;
    destination_aux.s6_addr16[4]=0x0000;
    destination_aux.s6_addr16[5]=0x0000;
    destination_aux.s6_addr16[6]=0x0000;
    destination_aux.s6_addr16[7]=0x0a00;
        
        memcpy(data_aux2 + 6, &(((struct rv_ctrl_pkt *) data)->prim_addr), sizeof (struct in6_addr));
 //   memcpy(data_aux2 + 6, &(destination_aux), sizeof (struct in6_addr));
    
    //=======================================================================
    
    data_aux2 = data_aux2 +22;
   
    
    *data_aux2 = 0xc0;
    data_aux2++;
    *data_aux2 = 0x15;
    data_aux2++;
    
    
    
    
/*
    *data_aux2 = 108;
    data_aux2++;
    *data_aux2 = 111;
    data_aux2++;
    *data_aux2 = 99;
    data_aux2++;
    *data_aux2 = 97;
    data_aux2++;
    *data_aux2 = 108;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
*/
    
    
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 2;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 1;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 28;
    data_aux2++;
    *data_aux2 = 32;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    
    
    
    
    
/*
    *data_aux2 = 5;
    data_aux2++;

*/
    *data_aux2 = 2;
    data_aux2++;
    
    
    
    
    
/*
    *data_aux2 = 2;
    data_aux2++;
    *data_aux2 = 110;
    data_aux2++;
    *data_aux2 = 115;
    data_aux2++;
    *data_aux2 = 192;
    data_aux2++;
    *data_aux2 = 19;    
    data_aux2++;
*/
    
    *data_aux2 = 0xc0;
    data_aux2++;
    *data_aux2 = 0x15;
    data_aux2++;
    
    
    
    
    
    
    *data_aux2 = 192;
    data_aux2++;
/*
    *data_aux2 = 70;
    data_aux2++;
*/
    
    *data_aux2 = 0x15;
    data_aux2++;
    
    
    
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 28;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 1;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 28;
    data_aux2++;
    *data_aux2 = 32;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    *data_aux2 = 16;
    data_aux2++;
    *data_aux2 = 32;
    data_aux2++;
    *data_aux2 = 1;
    data_aux2++;
    *data_aux2 = 0;
    data_aux2++;
    
    
    memset(data_aux2, 0, 12);
    
    data_aux2 = data_aux2 + 12;
    *data_aux2 = 10;
    
    
    //=======================================================================
    
    
    }
    // size_aux is the size of the MDNS query.
    fprintf(stderr,"The MDNS response will be send\n");
    fprintf(stderr,"The size_aux is %d\n", size_aux);
    fprintf(stderr,"The socket is %d\n", socket_global_mdns);
    fprintf(stderr,"The dest port is %d\n", htons(dest_port));
    fprintf(stderr,"The size of the MDNS query is %d\n", strlen(data_aux + MDNS_QUERY_OFFSET));
    
    // Now we have to delete the solved MDNS query from the queue
    
    if(dns_aux == dns){
        // This means that the first register is the one to delete
        dns_aux_prev = dns;
        free(dns);
        dns = dns_aux_prev->next;        
    }else{
        dns_aux_prev->next = dns_aux->next;
        free(dns_aux);
    }
       
    
/*
    if(!dest_port)v
        dest_port = dns_aux->dst_port;
*/
    fprintf(stderr, "The UDP packet size without the header is %d\n", size_aux + 26 + strlen(data_aux + MDNS_QUERY_OFFSET));
 //   antop_socket_send((unsigned char *)(data_aux), destination, size_aux + 26 + strlen(data_aux + MDNS_QUERY_OFFSET), 1, socket_global_mdns, htons(dest_port));
    
    fprintf(stderr,"El puerto es %d\n", htons(src_port));
    
    if(htons(src_port) == MDNS_PORT){
        fprintf(stderr,"Se enviara un MDNS response\n");
        antop_socket_send((unsigned char *)(data_aux), destination, size_aux, 1, socket_global_mdns, htons(dest_port));
    }
    else if(htons(src_port) == DNS_PORT){
        fprintf(stderr,"Se enviara un DNS response\n");
        destination.s6_addr16[0]=0x0120;
    destination.s6_addr16[1]=0x0000;
    destination.s6_addr16[2]=0x0000;
    destination.s6_addr16[3]=0x0000;
    destination.s6_addr16[4]=0x0000;
    destination.s6_addr16[5]=0x0000;
    destination.s6_addr16[6]=0x0000;
    destination.s6_addr16[7]=0x0100;


  //      antop_socket_send((unsigned char *)(data_aux), destination, size_aux , 1, socket_global_dns, htons(dest_port));
          antop_socket_send((unsigned char *)(data_aux), prim_addr, size_aux +42, 1, socket_global_dns, htons(dest_port));
    }
    return;
}






/*
void answer_dns_query_a(unsigned char *data, int size, int dest_port) {

    unsigned char *data_aux;
    unsigned char *data_aux2;
    struct in6_addr destination1;
    struct in6_addr addr;
    struct in6_addr *aux = NULL;
    struct dns_q_queue *dns_aux;
    unsigned char *udp;
    uint8_t aux_2;
    uint16_t aux_3;
    int idx;
    unsigned char *ptr_query_dns = NULL;
    size_t size_aux;

    ptr_query_dns = data + IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN;
    size_aux = size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN);

    //The difference between a query and an answer is...TTL (4); Length (2); data (16) = 22

    debug_pkt(data);

     // data_aux will contain the MDNS response to this same host
    // We must reserve the size of the query plus the answer of the response

    data_aux = malloc((size_t) (size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN) + 25 + strlen(data + IPV6_MDNS_QUERY_OFF)+1));

    fprintf(stderr, "ORIGINALMENTE SON BYTES (TODO EL PAQUETE) %u \n", size);

    fprintf(stderr, "ESTOY PIDIENDO BYTES %u \n", size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN) + 25 + strlen(data + IPV6_MDNS_QUERY_OFF)+1);

    fprintf(stderr,"ESTAMOS LABURANDO CON ANSWER_DNS_QUERY_A \n");

    memcpy(data_aux, ptr_query_dns, size_aux);


    // We need to change some of the query fields
    //Change the flags field to be 0x8400 i.e. Standard Query Response

    *(data_aux + 2) = 0x84;
    *(data_aux + 3) = 0;

    // One Answer...

    *(data_aux + 6) = 0;
    *(data_aux + 7) = 1;


    // Now insert the answer in the response MDNS pkt

    data_aux2 = data_aux + MDNS_QUERY_OFFSET + 4 + strlen(data_aux + MDNS_QUERY_OFFSET)+1;

    // Copy the name to solve from the Question to the Answer plus the type and class fields

    memcpy(data_aux2, ptr_query_dns + MDNS_QUERY_OFFSET, strlen(data_aux + MDNS_QUERY_OFFSET)+1 + 4);

    // TTL field

    data_aux2 = data_aux2 + strlen(data_aux + MDNS_QUERY_OFFSET)+1 + 4;

    *(data_aux2) = 0;
    *(data_aux2 + 1) = 0;
    *(data_aux2 + 2) = 0;
    *(data_aux2 + 3) = 0x0a;

    //Data length (ipv6 address -> 16 bytes)

    *(data_aux2 + 4) = 0;
    *(data_aux2 + 5) = 4;

    
    // THIS IS FOR IPV4 FOR TESTING PURPOSES ONLY
    
    *(data_aux2 + 6) = 192;
    *(data_aux2 + 7) = 168;
    *(data_aux2 + 8) = 0;
    *(data_aux2 + 9) = 4;
    

    //Now copy the solved address to the MDNS response
    
    //COMENTAR UNO DE ESTOS

    destination1.s6_addr16[0]=0x02ff;
    destination1.s6_addr16[1]=0x0000;
    destination1.s6_addr16[2]=0x0000;
    destination1.s6_addr16[3]=0x0000;
    destination1.s6_addr16[4]=0x0000;
    destination1.s6_addr16[5]=0x0000;
    destination1.s6_addr16[6]=0x0000;
    destination1.s6_addr16[7]=0xfb00;


        destination1.s6_addr16[0]=0x80fe;
    destination1.s6_addr16[1]=0x0000;
    destination1.s6_addr16[2]=0x0000;
    destination1.s6_addr16[3]=0x0000;
    destination1.s6_addr16[4]=0x0000;
    destination1.s6_addr16[5]=0xa295;
    destination1.s6_addr16[6]=0x0000;
    destination1.s6_addr16[7]=0xfb00;

    memcpy(&destination1, &prim_addr, sizeof (struct in6_addr));
    // COPMENTAR ESTO
    if (aux != NULL) {
        memcpy(data_aux2 + 6, aux, sizeof (struct in6_addr));
    } else
        memcpy(data_aux2 + 6, &(((struct rv_ctrl_pkt *) data)->prim_addr), sizeof (struct in6_addr));


    fprintf(stderr, "ESTO SE VA A ENVIAR POR SOCKET RAW !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");


    //    antop_socket_send((unsigned char *)(udp+8), destination, size-80+22, 1, socket, MDNS_PORT);
    //   antop_socket_send(data_aux +80, destination, size-80+22, 1, socket_global, MDNS_PORT);
    //    antop_socket_send(data_aux, destination, size+22, 1, socket_global_mdns, htons(dest_port));

    // size_aux is the size of the MDNS query.

    fprintf(stderr,"EL TAMAÑO DE LA CONSULTA PARA ANSWER_DNS_QUERY_A es %d\n",strlen(data_aux + MDNS_QUERY_OFFSET));
    fprintf(stderr,"LA CADENA DE CONSULTA ES: %s \n",data_aux + MDNS_QUERY_OFFSET);
    // OJOOOOOOOOOO TENER CUIDADO CON EL SOCKET QUE SE USA ACA ABAJO !!!!!!!!!!!!!!!!!!
    antop_socket_send((unsigned char *) (data_aux), destination1, size_aux + 26 + strlen(data_aux + MDNS_QUERY_OFFSET)+1, 1, socket_global_mdns, htons(dest_port));
//    antop_socket_send((unsigned char *) (ptr_query_dns), destination1, size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN), 1, socket_global, htons(dest_port));

    return;
}
*/


// ESTA ES UNA PRUEBA DE RESPUESTA MDNS SIN INCLUIR LA PREGUNTA

void answer_dns_query_a(unsigned char *data, int size, int dest_port) {

    unsigned char *data_aux;
    unsigned char *data_aux2;
    struct in6_addr destination1;
    struct in6_addr addr;
    struct in6_addr *aux = NULL;
    struct dns_q_queue *dns_aux;
    unsigned char *udp;
    uint8_t aux_2;
    uint16_t aux_3;
    int idx;
    unsigned char *ptr_query_dns = NULL;
    size_t size_aux;

    ptr_query_dns = data + IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN;
    size_aux = size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN);

    //The difference between a query and an answer is...TTL (4); Length (2); data (16) = 22

//    debug_pkt(data);
     // data_aux will contain the MDNS response to this same host
    // We must reserve the size of the query plus the answer of the response
    data_aux = malloc((size_t) (size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN) + 25));

    fprintf(stderr, "ORIGINALMENTE SON BYTES (TODO EL PAQUETE) %u \n", size);
    fprintf(stderr, "ESTOY PIDIENDO BYTES %u \n", size - (IPV6_HDR_LEN + DEST_OPT_HDR + UDP_HDR_LEN) + 25 + strlen(data + IPV6_MDNS_QUERY_OFF)+1);
    fprintf(stderr,"ANSWER_DNS_QUERY_A \n");

    memcpy(data_aux, ptr_query_dns, size_aux);

    // We need to change some of the query fields
    //Change the flags field to be 0x8400 i.e. Standard Query Response

    *(data_aux + 2) = 0x84;
    *(data_aux + 3) = 0;

    //  NO QUESTIONS
    
    *(data_aux + 4) = 0;
    *(data_aux + 5) = 0;
    
    // One Answer...

    *(data_aux + 6) = 0;
    *(data_aux + 7) = 1;

    // Now insert the answer in the response MDNS pkt

    data_aux2 = data_aux + MDNS_QUERY_OFFSET ;

    // Copy the name to solve from the Question to the Answer plus the type and class fields
    
    memcpy(data_aux2, ptr_query_dns + MDNS_QUERY_OFFSET, strlen(data_aux + MDNS_QUERY_OFFSET)+1 + 4);   

    // TTL field

    data_aux2 = data_aux2 + strlen(data_aux + MDNS_QUERY_OFFSET)+1 + 4;

    *(data_aux2) = 0;
    *(data_aux2 + 1) = 0;
    *(data_aux2 + 2) = 0;
    *(data_aux2 + 3) = 0x0a;

    //Data length (ipv6 address -> 16 bytes)

    *(data_aux2 + 4) = 0;
    *(data_aux2 + 5) = 4;

    
    // THIS IS FOR IPV4 FOR TESTING PURPOSES ONLY
    
    *(data_aux2 + 6) = 192;
    *(data_aux2 + 7) = 168;
    *(data_aux2 + 8) = 0;
    *(data_aux2 + 9) = 4;
    

    //Now copy the solved address to the MDNS response
    
    //COMENTAR UNO DE ESTOS

    destination1.s6_addr16[0]=0x02ff;
    destination1.s6_addr16[1]=0x0000;
    destination1.s6_addr16[2]=0x0000;
    destination1.s6_addr16[3]=0x0000;
    destination1.s6_addr16[4]=0x0000;
    destination1.s6_addr16[5]=0x0000;
    destination1.s6_addr16[6]=0x0000;
    destination1.s6_addr16[7]=0xfb00;


/*
        destination1.s6_addr16[0]=0x80fe;
    destination1.s6_addr16[1]=0x0000;
    destination1.s6_addr16[2]=0x0000;
    destination1.s6_addr16[3]=0x0000;
    destination1.s6_addr16[4]=0x0000;
    destination1.s6_addr16[5]=0xa295;
    destination1.s6_addr16[6]=0x0000;
    destination1.s6_addr16[7]=0xfb00;
*/

//    memcpy(&destination1, &prim_addr, sizeof (struct in6_addr));
    // COPMENTAR ESTO
/*
    if (aux != NULL) {
        memcpy(data_aux2 + 6, aux, sizeof (struct in6_addr));
    } else
        memcpy(data_aux2 + 6, &(((struct rv_ctrl_pkt *) data)->prim_addr), sizeof (struct in6_addr));
*/   
    // size_aux is the size of the MDNS query.

    fprintf(stderr,"EL TAMAÑO DE LA CONSULTA PARA ANSWER_DNS_QUERY_A es %d\n",strlen(data_aux + MDNS_QUERY_OFFSET));
    fprintf(stderr,"LA CADENA DE CONSULTA ES: %s \n",data_aux + MDNS_QUERY_OFFSET);

    // OJOOOOOOOOOO TENER CUIDADO CON EL SOCKET QUE SE USA ACA ABAJO !!!!!!!!!!!!!!!!!!

    antop_socket_send((unsigned char *) (data_aux), destination1, size_aux + 26 + strlen(data_aux + MDNS_QUERY_OFFSET)+1, 1, socket_global_mdns, htons(dest_port));

    return;



}

void print_rv_table(){
    struct lookup_table *rv_table_aux;
    int i;
    fprintf(stderr, "RV TABLE \n");
    for(i =0, rv_table_aux = rv_table; rv_table_aux != NULL; rv_table_aux = rv_table_aux->next, i++)
        fprintf(stderr, "--------------------------------------------\nEntry number %d; HCA %s; UA: %s\n", i, ip6_to_str(rv_table_aux->hc_addr), rv_table_aux->univ_addr);
}


