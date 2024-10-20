#ifndef _HTTP_SESSION_MGR_
#define _HTTP_SESSION_MGR_

#include "httpClient.h"

void httpSessionMgr_init( void );
void httpSessionMgr_startNewSession( tHttpClient_client* client );

#endif /* _HTTP_SESSION_MGR_ */
