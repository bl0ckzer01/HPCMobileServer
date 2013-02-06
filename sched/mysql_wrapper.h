#include <sstream>



int auth_user(const char* u_name,const char* u_pass);

int mysql_init( const char* u_name, const char* u_pass );

int get_user_pids(const char* u_id, std::stringstream& pids);

int get_project_description(const char* p_id, std::stringstream& desc);
