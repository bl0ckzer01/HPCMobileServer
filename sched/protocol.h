// Protocols

#define MAX_PID 10
#define MAX_NAME 10
#define MAX_PASS 33
#define MAX_FREQ 10
#define MAX_FREE 10
#define MAX_MEM 10
#define MAX_THREADS_QUEUE 1000
#define MAX_STRS 10
#define MAX_STR_LEN 20
#define GETSOURCE_PACKET_LEN MAX_NAME + MAX_PASS + MAX_FREQ + MAX_FREE + MAX_MEM + 10

typedef struct _SOURCE_PK
{
    char u_name[10];
    char u_pass[32];
    char pid[10];
    char d_freq[10];
    char d_mem[10];
    char d_free[10];
}GETSOURCEPK;

typedef struct _EXEC_PK
{
    char u_name[10];
    char u_pass[10];
    char pid[10];

}GETEXECPK;


void getsource_main();

void getexec_main();

int setup_db();

int setup_threads(int src_count,int exec_count);


