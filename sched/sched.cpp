#include "sched.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define GROUPS_COUNT 1
#define TABLE_CORRUPT -2
#define WU_NOT_FREE -1
#define WU_NOT_FOUND -4
#define GRP_NOT_FREE -5
#define WU_CALCULATED 1
#define GRP_NOT_FOUND -7

#define USER_NOT_FOUND -6



#define DEBUG
#ifdef DEBUG
#define dprintf printf
#else
#define dprintf
#endif


int WUCONTAINER::connect()
{
    size_t sz;
    if((h_fd  = open(TEST_HASH_TBL,O_RDWR)) <0)
        return -1;

    sz = lseek(h_fd, 0, SEEK_END);

    if((h_hdr = (HASH_HEADER*) mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE, h_fd, 0)) == MAP_FAILED)
    {
        close(h_fd);
        return -1;
    }
    h_sz = sz;
    int old_sz = sz;
    h_data = (HASH_ENTRY*) ((char *) h_hdr + sizeof(HASH_HEADER));

    // now the same for groups data file

    if((g_fd  = open(TEST_GROUPS_DATA,O_RDWR)) <0)
    {
        munmap(h_hdr,sz);
        return -1;
    }

    sz = lseek(g_fd, 0, SEEK_END);

    if((g_hdr = (GROUP*) mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE, g_fd, 0)) == MAP_FAILED)
    {
        close(g_fd);
        munmap(h_hdr,old_sz);
        return -1;
    }
    g_sz = sz;
    return 0;

}

#define STRONG_DBG

key32 WUCONTAINER::allocate_wu_to_calc(key32 u_id, int coeff, key32* g_id)
{
    if( !coeff )
        return -1;
    *g_id =  h_hdr->groups_count % coeff;
    GROUP* group = (GROUP*)((char*)g_hdr +  h_data[*g_id].offset );
    WU* wu = (WU*)((char*) group + sizeof (GROUP) + group->first_free_wu * sizeof (WU));
    dprintf("[HASHDB]: first_free_wu = %d\n",group->first_free_wu);
    dprintf("[HASHDB]: %d wus avail in group %d\n",group->wu_count,*g_id);
    dprintf("[HASHDB]: %d users connected to wu\n",wu->users_connected);
    dprintf("[HASHDB]: flag = %d\n",wu->flag);
    dprintf("[HASHDB]: wu id = %d\n",wu->wu_id);
    if(group->wu_count == 0)
    {
        dprintf("[HASHDB]: wu is not present in this group (gid = %d)\n",*g_id);
        return -1;
    }

    if (wu->users_connected  >= MAX_USERS_PER_WU || group->first_free_wu >= group->wu_count) // at start must be 1
        return -1; //group calculated
    if(wu->flag == WU_CALCULATED)
        return -1;
    wu->users_connected ++;
    wu->users[wu->users_connected - 1] = u_id;
    if (wu->users_connected -1 >= MAX_USERS_PER_WU)
    {
        group->first_free_wu++;
    }

#ifdef STRONG_DBG
    msync(g_hdr,g_sz,MS_SYNC);
#endif
    dprintf("[DBHASH]: %d users connected to wu %d\n",wu->users_connected,wu->wu_id);
    //if(time_to_flush)T_HASH_TBL
    dprintf("[HASHDB]: offset to wu 0x%08x\n",wu);
    return wu->wu_id;
}

key32 inline hash(key32 inval, usz32 len)
{
    return inval % len;
}

key32 inline rehash(key32 inval, usz32 len,key32 i)
{
    return hash(inval + i,len) % len;
}

int WUCONTAINER::end_wu_calc(key32 u_id, key32 g_id, key32 wu_id)
{
    //key32 key,current_key;
    int wu_found = 0;
    int hash_end = 0;
    key32 i = 1;
    if (g_id >= h_hdr->groups_count)
        return GRP_NOT_FOUND;

    GROUP* group = (GROUP*)((char*)g_hdr +  h_data[g_id].offset * sizeof(HASH_ENTRY)  );
    WU* wu = (WU*)((char*) group + sizeof (GROUP));

    WU* pnt;
    if (group->first_free_wu >= group->wu_count)
        return GRP_NOT_FREE; //group calculated

    pnt = &wu[ hash (wu_id, group->wu_count)];
    key32 first_key = pnt->wu_id;
    dprintf("start addr = 0x%08x wuid = %d\n",pnt,pnt->wu_id);
    while ( !wu_found && !hash_end )
    {
        if(pnt->wu_id == wu_id)
            wu_found = 1;
        else
        {
            pnt = &wu[rehash(wu_id,group->wu_count,i)];
            if(pnt->wu_id == first_key)
                hash_end  = 1;
            i++;
        }


    }
    if( !wu_found )
        return WU_NOT_FOUND;

    if(pnt->flag == WU_CALCULATED)
        return WU_NOT_FREE;

    //=====lock wu here (not implemented yet :(
    int has_uid = 0;
    for(i = 0; i< MAX_USERS_PER_WU && ( !has_uid ); i++)
        if(pnt->users[i] == u_id) has_uid = 1;
    if(! has_uid)
        return USER_NOT_FOUND;

    pnt->flag = WU_CALCULATED;
    pnt->users[0] = u_id;
    dprintf("[HASHDB]: offset to wu 0x%08x\n",pnt);
    group->first_free_wu++;
    dprintf("[HASHDB]: user disconnected from wu %d\n",pnt->wu_id);
    return 0;

}

WUCONTAINER::~WUCONTAINER()
{
    munmap(h_hdr,h_sz);
    munmap(g_hdr,g_sz);
    if(msync(g_hdr,g_sz,MS_SYNC) != 0) dprintf("error msync() \n");
}

WUCONTAINER wu_set;
/*
int main()
{
    key32 gid,wuid;
    wu_set.connect();
    printf(" disconnect status: %d\n",wu_set.end_wu_calc(45454545,0,wuid));
    wuid = wu_set.allocate_wu_to_calc(067,17,&gid);
    printf(" allocate status: %d\n",wuid);
    printf(" disconnect status: %d\n",wu_set.end_wu_calc(0,gid,wuid));
    return 0;

}
*/
