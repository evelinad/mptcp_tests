#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <linux/tcp.h>
#include <time.h>
#include <getopt.h>
#include <unistd.h>
#define SEC_IN_NANOS 1000000000
#define K 1024
#define M 1048576

static char usage[] =
    "usage: %s [--nodelay] [--help] [--serverip=<ip>] [--serverport=<port>] [--sendbuffer=<sizer>] [--rate=<rate>] [--reqsize=<size>] [--iterations=<iterations>] [--time=<seconds>] [--verbose=<level>]\n";

static char help[] = "\nTCP Sockets Client Test Options:\n\
--nodelay                  Disable Nagle's Algorithm\n\
--help                     Display this message and exit.\n\
--serverip <ip>            Server ip to connect to. Default 127.0.0.1\n\
--serverport <port>        Server port to connect to. Default 2000\n\
--send_buffer <size[K|M]>  Socket send socket buffer size. Unix system default\n\
--rate <rate[K|M]>         Number of packets per second. Default 10\n\
--reqsize <size[K|M]>      Size of a request message. Default 100\n\
--iterations <iterations>  Number of iterations for a test\n\
--time <seconds>           Test duration in seconds\n\
--verbose [0 1 2 3]      Verbosity level\n";

static struct option client_options[] = {
	{"nodelay", no_argument, 0, 'n'},
	{"help", no_argument, 0, 'h'},
	{"serverip", required_argument, 0, 'i'},
	{"serverport", required_argument, 0, 'p'},
	{"rate", required_argument, 0, 'r'},
	{"sendbuffer", required_argument, 0, 'b'},
	{"reqsize", required_argument, 0, 'q'},
	{"iterations", required_argument, 0, 'e'},
	{"time", required_argument, 0, 't'},
	{"verbose", required_argument, 0, 'v'},
	{0, 0, 0, 0}
};

int tcp_nodelay = 0;
char *server_ip = NULL;
unsigned int server_port = 2003;
unsigned int packet_rate = 100;
unsigned long send_buffer_size = 0;
char *req_buffer = NULL;
unsigned long req_size = 100;
unsigned long duration_sec = 0;
unsigned int iterations = 100;
int verb_level = 0;

unsigned long total_bytes_sent = 0;
unsigned long total_send_calls = 0;

void print_usage(char *exec_name)
{
	printf(usage, exec_name);
}

void print_help(char *exec_name)
{
	print_usage(exec_name);
	printf("%s", help);
}

void sanity_checks(int argc, char *argv[])
{
	int opt = 0;
	int long_index = 0;
	int scale = 1;
	server_ip = calloc(40, sizeof(char));
	strcpy(server_ip, "127.0.0.1");
	while ((opt = getopt_long(argc, argv, "nhi:p:r:b:q:t:e:",
				  client_options, &long_index)) != -1) {
		switch (opt) {
		case 'n':
			tcp_nodelay = 1;
			break;
		case 'i':
			strcpy(server_ip, optarg);
			break;
		case 'p':
			if (atoi(optarg) > 0) {
				server_port = atoi(optarg);
			} else {
				fprintf(stderr,
					"[ERROR] Invalid server port\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'r':

			if (optarg[strlen(optarg) - 1] == 'M') {
				scale = M;
				optarg[strlen(optarg) - 1] = '\0';
			} else if (optarg[strlen(optarg) - 1] == 'K') {
				scale = K;
				optarg[strlen(optarg) - 1] = '\0';				
			}

			if (atoi(optarg) > 0) {
				packet_rate = atoi(optarg) * scale;
				scale = 1;
			} else {
				fprintf(stderr,
					"[ERROR] Invalid packet rate\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'b':

			if (optarg[strlen(optarg) - 1] == 'M') {
				scale = M;
				optarg[strlen(optarg) - 1] = '\0';
			} else if (optarg[strlen(optarg) - 1] == 'K') {
				scale = K;
				optarg[strlen(optarg) - 1] = '\0';
			}

			if (atol(optarg) > 0) {
				send_buffer_size = atol(optarg) * scale;
				scale = 1;
			} else {
				fprintf(stderr,
					"[ERROR] Invalid request buffer size\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'q':

			if (optarg[strlen(optarg) - 1] == 'M') {
				scale = M;
				optarg[strlen(optarg) - 1] = '\0';
			} else if (optarg[strlen(optarg) - 1] == 'K') {
				scale = K;
				optarg[strlen(optarg) - 1] = '\0';
			}

			if (atoi(optarg) > 0) {
				req_size = atoi(optarg) * scale;
				scale = 1;

			} else {
				fprintf(stderr,
					"[ERROR] Invalid packet rate\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			print_help(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 't':
			if (atol(optarg) > 0) {
				duration_sec = atol(optarg);
			} else {
				fprintf(stderr, "[ERROR] Invalid duration\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'e':
			if (atoi(optarg) > 0) {
				iterations = atoi(optarg);
			} else {
				fprintf
				    (stderr,
				     "[ERROR] Invalid number of iterations\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'v':
			if (atoi(optarg) > 0) {
				verb_level = atoi(optarg);
			} else {
				fprintf(stderr,
					"[ERROR] Invalid verbosity level\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	req_buffer = calloc(req_size, sizeof(char));
	req_buffer[req_size-3] = '\r';
	req_buffer[req_size-2] = '\n';

}

int establish_connection()
{
	int sockfd = 0;
	struct sockaddr_in serv_addr;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "[ERROR] socket: Could not create socket\n");
		return -1;
	}
	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port);

	if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
		fprintf(stderr,
			"[ERROR] inet_pton: error initializing serv_addr\n");
		return -1;
	}
	struct protoent *tcp_proto = getprotobyname("tcp");
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <
	    0) {
		fprintf(stderr, "[ERROR] connect: Failed to connect\n");
		return -1;
	} else {
		if (verb_level == 3)
			fprintf(stdout,
				"[INFO] Successfully established connection to remote server: %s, port %d",
				server_ip, server_port);
	}
	int optval;
	optval = 1;
	if (setsockopt
	    (sockfd, tcp_proto->p_proto, TCP_NODELAY, &tcp_nodelay,
	     sizeof(optval)) != 0) {
		fprintf
		    (stderr,
		     "[ERROR] setsockopt: error setting TCP_NODELAY option\n");
		return -1;
	} 
	if(send_buffer_size > 0)
	if (setsockopt
	    (sockfd, SOL_SOCKET, SO_SNDBUF, &req_size, sizeof(send_buffer_size))
	    != 0) {
		fprintf(stderr,
			"[ERROR] setsockopt: error setting SO_SNDBUF option\n");
		return -1;
	} 
	
	if (verb_level >= 1) {
		fprintf(stdout,
			"TCP_NODELAY = %d\nserver ip = %s\nserver port = %u\nsend buffer size = %lu\nrate = %u\nrequest size = %lu\niterations = %u\n duration = %lu\nverbosity level = %d\n",
			tcp_nodelay, server_ip, server_port, send_buffer_size,
			packet_rate, req_size, iterations, duration_sec,
			verb_level);

	}
	return sockfd;

}

void do_test(int sockfd)
{
	int r;
	unsigned long bytes_sent = 0;
	unsigned long send_calls = 0;
	unsigned long time_in_nanos1, time_in_nanos2, time_diff_in_nanos;
	struct timespec tsp;
	bytes_sent = 0;
	send_calls = 0;
	clock_gettime(CLOCK_REALTIME, &tsp);
	time_in_nanos1 = SEC_IN_NANOS * tsp.tv_sec + tsp.tv_nsec;
	
	for (r = 0; r < packet_rate; r++) 
	{
		bytes_sent += write(sockfd, req_buffer, req_size);
		clock_gettime(CLOCK_REALTIME, &tsp);
		time_in_nanos2 = SEC_IN_NANOS * tsp.tv_sec + tsp.tv_nsec;
		time_diff_in_nanos = time_in_nanos2 - time_in_nanos1;
		if (time_diff_in_nanos > SEC_IN_NANOS) {
			if (r < packet_rate - 1 && verb_level >= 1)
				fprintf(stderr,
					"[WARNING] Didn't reach required packet rate\n");
			r = packet_rate;

		} 
		send_calls++;
	}
	
	if (time_diff_in_nanos < SEC_IN_NANOS)
	{
		tsp.tv_nsec = SEC_IN_NANOS - time_diff_in_nanos;
		tsp.tv_sec = 0; 
		nanosleep(&tsp, NULL);
	}		
	total_bytes_sent += bytes_sent;
	total_send_calls += send_calls;
	if (verb_level == 2)
		fprintf(stdout,
			"[INFO] bytes sent = %lu\nsend calls = %lu\ntotal bytes sent = %lu\ntotal send calls = %lu\n",
			bytes_sent, send_calls,
			total_bytes_sent, total_send_calls);

}

void do_test_iterations(int sockfd)
{

	int i;
	
	for (i = 0; i < iterations; i++)
	{
		if(verb_level >= 2)
			fprintf(stdout, "Starting iteration %d\n",i);
		do_test(sockfd);
	}


}

void do_test_duration(int sockfd)
{

	int i;
	struct timespec tsp;	
	unsigned long time_in_nanos1, time_in_nanos2, time_diff_in_nanos;
	clock_gettime(CLOCK_REALTIME, &tsp);
	time_in_nanos1 = SEC_IN_NANOS * tsp.tv_sec + tsp.tv_nsec;
	while (1) {
		do_test(sockfd);
		clock_gettime(CLOCK_REALTIME, &tsp);
		time_in_nanos2 = SEC_IN_NANOS * tsp.tv_sec + tsp.tv_nsec;
		time_diff_in_nanos = time_in_nanos2 - time_in_nanos1;
		if (time_diff_in_nanos > SEC_IN_NANOS)
			break;
			if(verb_level >= 2)
					fprintf(stdout, "Elapsed time = %luus\n",time_diff_in_nanos * 1000000L);	
	}

}

int main(int argc, char *argv[])
{

	int sockfd;
	sanity_checks(argc, argv);
	sockfd = establish_connection();
	if(sockfd == -1 )
	{
		free(req_buffer);
		free(server_ip);
		exit(EXIT_FAILURE);		
	}
		
	fprintf(stdout, "Start test ... \n");		
	if (duration_sec == 0)
		do_test_iterations(sockfd);
	else
		do_test_duration(sockfd);
	close(sockfd);
	fprintf(stdout, "End test ... \n");			

	return 0;
}
