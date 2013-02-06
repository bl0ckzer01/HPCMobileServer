


#define PROJECT_PATH
#define TEST_HASH_TBL "/home/god/server/prj/0/hdr"
#define TEST_GROUPS_DATA "/home/god/server/prj/0/grp"
#define MAX_USERS_PER_WU 2

typedef  unsigned long key32;
#define _64BIT
#ifdef _64BIT
typedef unsigned long long off64,off;
typedef unsigned int uid32,usz32,u32;
#else
typedef unsigned long off32,off;
#endif



#pragma pack (1)
typedef struct _HASH_HEADER
{
    u32 magic;
    u32 blocks_count;
    char existing_wu;
    u32 groups_count;
    u32 wu_count;
}HASH_HEADER;

typedef struct _HASH_ENTRY
{
    key32 gid; // just for debug
    off offset;
}HASH_ENTRY;

typedef struct _WU_ENTRY
{
    uid32 users[MAX_USERS_PER_WU];
    u32 users_connected;
    key32 wu_id;
    u32 flag;
}WU;

typedef struct _GROUP_HEADER
{
    usz32 wu_count;
    usz32 first_free_wu;
}GROUP;

class WUCONTAINER
{
    int h_fd, g_fd;
    int max_users_per_wu;
    HASH_ENTRY* h_data;
    GROUP* g_hdr;
    HASH_HEADER* h_hdr;
    WU* g_group;
    int time_to_flush ;//for debug only !~!!!! must be 0!!!!!!!
    u32 h_sz,g_sz;
public:
    key32 allocate_wu_to_calc(key32 u_id, int coeff, key32* g_id);
    int end_wu_calc(key32 u_id, key32 g_id, key32 wu_id);
    int inline get_max_upg()
    {
        return max_users_per_wu;
    }
    ~WUCONTAINER();
    int connect();

private:


};
