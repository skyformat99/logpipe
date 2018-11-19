#include "logpipe_api.h"

#include <sys/uio.h>
#define __need_IOV_MAX
#include <bits/stdio_lim.h>

/* communication protocol :
	|(log)|[key=(key)]|[file=(pathfile)]|[byteoffset=(byteoffset)]|\n|
*/

int	__LOGPIPE_OUTPUT_INGEEK_VERSION_0_2_2 = 1 ;

struct ForwardSession
{
	char			*ip ;
	int			port ;
	struct sockaddr_in   	addr ;
	int			sock ;
	time_t			enable_timestamp ;
} ;

/* ��������ṹ */
#define IP_PORT_MAXCNT			8

#define DISABLE_TIMEOUT			10

#define PARSE_BUFFER_SIZE		LOGPIPE_BLOCK_BUFSIZE*2

#define MAX_OUTPUT_BUF			256
#define MAX_TAIL_BUF			256

#define IOV_CONNECT_INTERVAL_DEFAULT	1
#define IOV_SEND_COUNT_DEFAULT		IOV_MAX
#define IOV_SEND_TIMEOUT_DEFAULT	20

struct OutputPluginContext
{
	char			*key ;
	char			*path ;
	struct ForwardSession	forward_session_array[IP_PORT_MAXCNT] ;
	int			forward_session_count ;
	struct ForwardSession	*p_forward_session ;
	int			forward_session_index ;
	int			iov_connect_interval ;
	int			disable_timeout ;
	int			iov_send_count ;
	int			iov_send_timeout ;
	
	char			*filename ;
	uint64_t		file_line ;
	uint64_t		block_len ;
	char			*block_buf ;
	
	char			parse_buffer[ PARSE_BUFFER_SIZE + 1 ] ; /* ���������� */
	int			parse_buflen ;
	
	char			tail_array[ IOV_SEND_COUNT_DEFAULT ][ MAX_TAIL_BUF + 1 ] ;
	int			tail_len[ IOV_SEND_COUNT_DEFAULT ] ;
	
	struct iovec		iov_array[ IOV_SEND_COUNT_DEFAULT ] ;
	int			iov_count ;
	int			iov_total_line_len ;
	int			iov_total_tail_len ;
} ;

funcLoadOutputPluginConfig LoadOutputPluginConfig ;
int LoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct OutputPluginContext	*p_plugin_ctx = NULL ;
	int				i ;
	struct ForwardSession		*p_forward_session = NULL ;
	char				*p = NULL ;
	
	/* �����ڴ��Դ�Ų�������� */
	p_plugin_ctx = (struct OutputPluginContext *)malloc( sizeof(struct OutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
	p_plugin_ctx->key = QueryPluginConfigItem( p_plugin_config_items , "key" ) ;
	INFOLOGC( "key[%s]" , p_plugin_ctx->key )
	
	p_plugin_ctx->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	INFOLOGC( "path[%s]" , p_plugin_ctx->path )
	if( p_plugin_ctx->path == NULL || p_plugin_ctx->path[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'path'" )
		return -1;
	}
	
	/* ����������� */
	for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < IP_PORT_MAXCNT ; i++ , p_forward_session++ )
	{
		if( i == 0 )
		{
			p_forward_session->ip = QueryPluginConfigItem( p_plugin_config_items , "ip" ) ;
			INFOLOGC( "ip[%s]" , p_forward_session->ip )
		}
		else
		{
			p_forward_session->ip = QueryPluginConfigItem( p_plugin_config_items , "ip%d" , i+1 ) ;
			INFOLOGC( "ip%d[%s]" , i+1 , p_forward_session->ip )
		}
		if( p_forward_session->ip == NULL || p_forward_session->ip[0] == '\0' )
			break;
		
		if( i == 0 )
			p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
		else
			p = QueryPluginConfigItem( p_plugin_config_items , "port%d" , i+1 ) ;
		if( p == NULL || p[0] == '\0' )
		{
			ERRORLOGC( "expect config for 'port'" )
			return -1;
		}
		p_forward_session->port = atoi(p) ;
		if( i == 0 )
		{
			INFOLOGC( "port[%d]" , p_forward_session->port )
		}
		else
		{
			INFOLOGC( "port%d[%d]" , i+1 , p_forward_session->port )
		}
		if( p_forward_session->port <= 0 )
		{
			ERRORLOGC( "port[%s] invalid" , p )
			return -1;
		}
		
		p_plugin_ctx->forward_session_count++;
	}
	if( p_plugin_ctx->forward_session_count == 0 )
	{
		ERRORLOGC( "expect config for 'ip'" )
		return -1;
	}
	
	p = QueryPluginConfigItem( p_plugin_config_items , "iov_connect_interval" ) ;
	if( p )
	{
		p_plugin_ctx->iov_connect_interval = atoi(p) ;
	}
	else
	{
		p_plugin_ctx->iov_connect_interval = IOV_CONNECT_INTERVAL_DEFAULT ;
	}
	INFOLOGC( "iov_connect_interval[%d]" , p_plugin_ctx->iov_connect_interval )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "disable_timeout" ) ;
	if( p )
	{
		p_plugin_ctx->disable_timeout = atoi(p) ;
	}
	else
	{
		p_plugin_ctx->disable_timeout = DISABLE_TIMEOUT ;
	}
	INFOLOGC( "disable_timeout[%d]" , p_plugin_ctx->disable_timeout )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "iov_send_count" ) ;
	if( p )
	{
		p_plugin_ctx->iov_send_count = atoi(p) ;
		if( p_plugin_ctx->iov_send_count <= 0 )
			p_plugin_ctx->iov_send_count = 1 ;
		if( p_plugin_ctx->iov_send_count > IOV_SEND_COUNT_DEFAULT )
			p_plugin_ctx->iov_send_count = IOV_SEND_COUNT_DEFAULT ;
	}
	else
	{
		p_plugin_ctx->iov_send_count = IOV_SEND_COUNT_DEFAULT ;
	}
	INFOLOGC( "iov_send_count[%d]" , p_plugin_ctx->iov_send_count )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "iov_send_timeout" ) ;
	if( p )
	{
		p_plugin_ctx->iov_send_timeout = atoi(p) ;
		if( p_plugin_ctx->iov_send_timeout <= 0 )
			p_plugin_ctx->iov_send_timeout = 1 ;
		if( p_plugin_ctx->iov_send_timeout > IOV_SEND_TIMEOUT_DEFAULT )
			p_plugin_ctx->iov_send_timeout = IOV_SEND_TIMEOUT_DEFAULT ;
	}
	else
	{
		p_plugin_ctx->iov_send_timeout = IOV_SEND_TIMEOUT_DEFAULT ;
	}
	INFOLOGC( "iov_send_timeout[%d]" , p_plugin_ctx->iov_send_timeout )
	
	/* ���ò������������ */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

static int CheckAndConnectForwardSocket( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct OutputPluginContext *p_plugin_ctx , int forward_session_index )
{
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			i ;
	
	int			nret = 0 ;
	
	DEBUGLOGC( "forward_session_index[%d]" , forward_session_index )
	
	while(1)
	{
		for( i = 0 ; i < p_plugin_ctx->forward_session_count ; i++ )
		{
			if( forward_session_index >= 0 )
			{
				p_forward_session = p_plugin_ctx->forward_session_array + forward_session_index ;
			}
			else
			{
				p_plugin_ctx->forward_session_index++;
				if( p_plugin_ctx->forward_session_index >= p_plugin_ctx->forward_session_count )
					p_plugin_ctx->forward_session_index = 0 ;
				DEBUGLOGC( "p_plugin_ctx->forward_session_index[%d]" , p_plugin_ctx->forward_session_index )
				p_plugin_ctx->p_forward_session = p_forward_session = p_plugin_ctx->forward_session_array + p_plugin_ctx->forward_session_index ;
			}
			
			if( forward_session_index < 0 )
			{
				if( p_forward_session->enable_timestamp > 0 )
				{
					if( time(NULL) < p_forward_session->enable_timestamp )
						continue;
					else
						p_forward_session->enable_timestamp = 0 ;
				}
				
				if( p_forward_session->sock >= 0 )
					return 0;
			}
			
			/* �����׽��� */
			p_forward_session->sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
			if( p_forward_session->sock == -1 )
			{
				ERRORLOGC( "socket failed , errno[%d]" , errno )
				return -1;
			}
			
			/* �����׽���ѡ�� */
			{
				int	onoff = 1 ;
				setsockopt( p_forward_session->sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
			}
			
			/* ���ӵ�����������˿� */
			nret = connect( p_forward_session->sock , (struct sockaddr *) & (p_forward_session->addr) , sizeof(struct sockaddr) ) ;
			if( nret == -1 )
			{
				ERRORLOGC( "connect[%s:%d] failed , errno[%d]" , p_forward_session->ip , p_forward_session->port , errno )
				close( p_forward_session->sock ); p_forward_session->sock = -1 ;
				if( forward_session_index < 0 )
					p_forward_session->enable_timestamp = time(NULL) + p_plugin_ctx->disable_timeout ;
			}
			else
			{
				INFOLOGC( "connect[%s:%d] ok , sock[%d]" , p_forward_session->ip , p_forward_session->port , p_forward_session->sock )
				/* �������������� */
				AddOutputPluginEvent( p_env , p_logpipe_output_plugin , p_forward_session->sock );
				
				/* ���÷Ƕ���ģʽ */
				{
					int	opts ;
					opts = fcntl( p_forward_session->sock , F_GETFL );
					opts |= O_NONBLOCK ;
					fcntl( p_forward_session->sock , F_SETFL , opts );
				}
				
				return 0;
			}
		}
		
		sleep(p_plugin_ctx->iov_connect_interval);
	}
}

funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				i ;
	struct ForwardSession		*p_forward_session = NULL ;
	
	int				nret = 0 ;
	
	/* ��ʼ����������ڲ����� */
	
	/* �������жԶ�logpipe������ */
	for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < p_plugin_ctx->forward_session_count ; i++ , p_forward_session++ )
	{
		memset( & (p_forward_session->addr) , 0x00 , sizeof(struct sockaddr_in) );
		p_forward_session->addr.sin_family = AF_INET ;
		if( p_forward_session->ip[0] == '\0' )
			p_forward_session->addr.sin_addr.s_addr = INADDR_ANY ;
		else
			p_forward_session->addr.sin_addr.s_addr = inet_addr(p_forward_session->ip) ;
		p_forward_session->addr.sin_port = htons( (unsigned short)(p_forward_session->port) );
		
		/* ���ӷ���� */
		p_forward_session->sock = -1 ;
		nret = CheckAndConnectForwardSocket( p_env , p_logpipe_output_plugin , p_plugin_ctx , i ) ;
		if( nret )
		{
			ERRORLOGC( "CheckAndConnectForwardSocket failed[%d]" , nret )
			return -1;
		}
	}
	
	p_plugin_ctx->forward_session_index = -1 ;
	
	/* ������ӣ��������ʧЧ������ */
	nret = CheckAndConnectForwardSocket( p_env , p_logpipe_output_plugin , p_plugin_ctx , -1 ) ;
	if( nret )
	{
		ERRORLOGC( "CheckAndConnectForwardSocket failed[%d]" , nret )
		return nret;
	}
	
	return 0;
}

funcOnOutputPluginEvent OnOutputPluginEvent;
int OnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				i ;
	int				j ;
	struct ForwardSession		*p_forward_session = NULL ;
	struct pollfd			fds[ IP_PORT_MAXCNT ] ;
	int				fds_count ;
	
	int				nret = 0 ;
	
	DEBUGLOGC( "OnOutputPluginEvent" )
	
	/* ������жԶ�logpipe�����������쳣�¼� */
	memset( fds , 0x00 , sizeof(fds) );
	fds_count = 0 ;
	for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < p_plugin_ctx->forward_session_count ; i++ , p_forward_session++ )
	{
		if( p_forward_session->sock < 0 )
			continue;
		
		fds[fds_count].fd = p_forward_session->sock ;
		fds[fds_count].events = POLLIN|POLLERR|POLLHUP ;
		fds[fds_count].revents = 0 ;
		fds_count++;
		DEBUGLOGC( "add fd[%d] to poll fds" , p_forward_session->sock )
	}
	
	nret = poll( fds , fds_count , 0 ) ;
	if( nret > 0 )
	{
		for( j = 0 ; j < fds_count ; j++ )
		{
			for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < p_plugin_ctx->forward_session_count ; i++ , p_forward_session++ )
			{
				if( p_forward_session->sock < 0 )
					continue;
				
				if( p_forward_session->sock != fds[j].fd )
					continue;
				
				if( fds[j].revents & POLLIN )
				{
					char	buf[ 4096 ] ;
					int	len ;
					
					len = read( p_forward_session->sock , buf , sizeof(buf) ) ;
					if( len == 0 )
					{
						ERRORLOGC( "read forward sock[%d] return[%d]" , p_forward_session->sock , len )
						fds[j].revents |= POLLHUP ;
					}
					else if( len == -1 )
					{
						ERRORLOGC( "read forward sock[%d] return[%d]" , p_forward_session->sock , len )
						fds[j].revents |= POLLERR ;
					}
					else
					{
						WARNLOGC( "read forward sock[%d] return[%d]" , p_forward_session->sock , len )
					}
				}
				
				if( ( fds[j].revents & POLLERR ) || ( fds[j].revents & POLLHUP ) )
				{
					ERRORLOGC( "POLLERR or POLLHUP on forward sock[%d] hited" , p_forward_session->sock )
					
					/* �ر����� */
					DeleteOutputPluginEvent( p_env , p_logpipe_output_plugin , p_forward_session->sock );
					ERRORLOGC( "remote socket closed , close forward sock[%d]" , p_forward_session->sock )
					close( p_forward_session->sock ); p_forward_session->sock = -1 ;
				}
			}
		}
	}
	
	return 0;
}

funcBeforeWriteOutputPlugin BeforeWriteOutputPlugin ;
int BeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	/* �����ļ���ָ�� */
	p_plugin_ctx->filename = filename ;
	
	return 0;
}

/* ���н��������� */
static int SendLineBuffer( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct OutputPluginContext *p_plugin_ctx , char *line , uint64_t line_len , uint64_t line_add , unsigned char flush_send_buffer )
{
	char		mainfilename[ PATH_MAX + 1 ] ;
	char		*p = NULL ;
	char		(*p_tail_array)[ MAX_TAIL_BUF + 1 ] = NULL ;
	int		*p_tail_len = NULL ;
	struct iovec	*p_iov_array = NULL ;
	int		*p_iov_count = NULL ;
	int		send_timeout = 0 ;
	int		send_elapse = 0 ;
	int		len ;
	
	int		nret = 0 ;
	
	DEBUGLOGC( "send [%d][%.*s]" , line_len , line_len , line )
	
	/* ���β�� */
	p_tail_array = p_plugin_ctx->tail_array + p_plugin_ctx->iov_count  ;
	p_tail_len = p_plugin_ctx->tail_len + p_plugin_ctx->iov_count ;
	if( p_plugin_ctx->key )
	{
		(*p_tail_len) = snprintf( (*p_tail_array) , MAX_TAIL_BUF , "[key=%s][file=%s/%s][byteoffset=%"PRIu64"]\n" , p_plugin_ctx->key , p_plugin_ctx->path , p_plugin_ctx->filename , p_plugin_ctx->file_line+line_add ) ;
	}
	else
	{
		memset( mainfilename , 0x00 , sizeof(mainfilename) );
		strncpy( mainfilename , p_plugin_ctx->filename , sizeof(mainfilename)-1 );
		p = strrchr( mainfilename , '.' ) ;
		if( p )
			(*p) = '\0' ;
		p = strchr( mainfilename , '_' ) ;
		if( p )
		{
			p = strchr( p+1 , '_' ) ;
			if( p )
				(*p) = '\0' ;
		}
		(*p_tail_len) = snprintf( (*p_tail_array) , MAX_TAIL_BUF , "[key=%s][file=%s/%s][byteoffset=%"PRIu64"]\n" , mainfilename , p_plugin_ctx->path , p_plugin_ctx->filename , p_plugin_ctx->file_line+line_add ) ;
	}
	
	/* �������ݿ鵽TCP */
	INFOLOGC( "IOV-BUF LOG[%d][%.*s] TAIL[%d][%.*s]" , line_len , MIN(line_len,MAX_OUTPUT_BUF) , line , (*p_tail_len) , (*p_tail_len)-1 , (*p_tail_array) )
	DEBUGHEXLOGC( line , line_len , NULL )
	DEBUGHEXLOGC( (*p_tail_array) , (*p_tail_len) , NULL )
	
	p_plugin_ctx->iov_array[p_plugin_ctx->iov_count].iov_base = line ;
	p_plugin_ctx->iov_array[p_plugin_ctx->iov_count].iov_len = line_len ;
	p_plugin_ctx->iov_count++;
	p_plugin_ctx->iov_total_line_len += line_len ;
	
	if( p_plugin_ctx->iov_count >= p_plugin_ctx->iov_send_count )
	{
		ERRORLOGC( "p_plugin_ctx->iov_count[%d] invalid at this position" , p_plugin_ctx->iov_count )
		p_plugin_ctx->iov_count = 0 ;
		p_plugin_ctx->iov_total_line_len = 0 ;
		p_plugin_ctx->iov_total_tail_len = 0 ;
		return 1;
	}
	
	p_plugin_ctx->iov_array[p_plugin_ctx->iov_count].iov_base = (*p_tail_array) ;
	p_plugin_ctx->iov_array[p_plugin_ctx->iov_count].iov_len = (*p_tail_len) ;
	p_plugin_ctx->iov_count++;
	p_plugin_ctx->iov_total_tail_len += (*p_tail_len) ;
	
	if( p_plugin_ctx->iov_count >= p_plugin_ctx->iov_send_count || flush_send_buffer )
	{
		char	*last_log_base_bak = p_plugin_ctx->iov_array[p_plugin_ctx->iov_count-2].iov_base ;
		int	last_log_len_bak = p_plugin_ctx->iov_array[p_plugin_ctx->iov_count-2].iov_len ;
		char	*last_tail_base_bak = p_plugin_ctx->iov_array[p_plugin_ctx->iov_count-1].iov_base ;
		int	last_tail_len_bak = p_plugin_ctx->iov_array[p_plugin_ctx->iov_count-1].iov_len ;
		int	iov_count_bak = p_plugin_ctx->iov_count ;
		
		nret = CheckAndConnectForwardSocket( p_env , p_logpipe_output_plugin , p_plugin_ctx , -1 ) ;
		if( nret )
		{
			ERRORLOGC( "CheckAndConnectForwardSocket failed[%d]" , nret )
			p_plugin_ctx->iov_count = 0 ;
			p_plugin_ctx->iov_total_line_len = 0 ;
			p_plugin_ctx->iov_total_tail_len = 0 ;
			return nret;
		}
		
		p_iov_array = p_plugin_ctx->iov_array ;
		p_iov_count = & (p_plugin_ctx->iov_count) ;
		send_timeout = p_plugin_ctx->iov_send_timeout*1000000 ;
		send_elapse = 0 ;
		len = writev3( p_plugin_ctx->p_forward_session->sock , & p_iov_array , p_iov_count , p_plugin_ctx->iov_total_line_len+p_plugin_ctx->iov_total_tail_len , & send_timeout , & send_elapse ) ;
		if( len < 0 || p_iov_array || (*p_iov_count) > 0 )
		{
			if( p_iov_array || (*p_iov_count) > 0 )
			{
				ERRORLOGC( "IOV-SEND[%s:%d][%d] [%d]l[%d]lB[%d]tB timeout , errno[%d] , ll[%d]B[%.100s] lt[%d]B[%.*s]"
					, p_plugin_ctx->p_forward_session->ip , p_plugin_ctx->p_forward_session->port , p_plugin_ctx->p_forward_session->sock
					, iov_count_bak/2 , p_plugin_ctx->iov_total_line_len , p_plugin_ctx->iov_total_tail_len
					, errno
					, last_log_len_bak , last_log_base_bak
					, last_tail_len_bak , last_tail_len_bak-1 , last_tail_base_bak )
			}
			else
			{
				ERRORLOGC( "IOV-SEND[%s:%d][%d] [%d]l[%d]lB[%d]tB failed[%d] , errno[%d] , ll[%d]B[%.100s] lt[%d]B[%.*s]"
					, p_plugin_ctx->p_forward_session->ip , p_plugin_ctx->p_forward_session->port , p_plugin_ctx->p_forward_session->sock
					, iov_count_bak/2 , p_plugin_ctx->iov_total_line_len , p_plugin_ctx->iov_total_tail_len
					, len , errno
					, last_log_len_bak , last_log_base_bak
					, last_tail_len_bak , last_tail_len_bak-1 , last_tail_base_bak )
			}
			close( p_plugin_ctx->p_forward_session->sock ); p_plugin_ctx->p_forward_session->sock = -1 ;
			
			p_plugin_ctx->iov_count = 0 ;
			p_plugin_ctx->iov_total_line_len = 0 ;
			p_plugin_ctx->iov_total_tail_len = 0 ;
			return 1;
		}
		else
		{
			NOTICELOGC( "IOV-SEND[%s:%d][%d] [%d]l[%d]lB[%d]tB ok , DTV[%d.%06d] ll[%d]B[%.*s] lt[%d]B[%.*s]"
				, p_plugin_ctx->p_forward_session->ip , p_plugin_ctx->p_forward_session->port , p_plugin_ctx->p_forward_session->sock
				, iov_count_bak/2 , p_plugin_ctx->iov_total_line_len , p_plugin_ctx->iov_total_tail_len
				, send_elapse/1000000 , send_elapse%1000000
				, last_log_len_bak , last_log_len_bak , last_log_base_bak
				, last_tail_len_bak , last_tail_len_bak-1 , last_tail_base_bak )
			p_plugin_ctx->iov_count = 0 ;
			p_plugin_ctx->iov_total_line_len = 0 ;
			p_plugin_ctx->iov_total_tail_len = 0 ;
		}
	}
	
	return 0;
}

/* ���ݿ�ϲ������������� */
static int CombineToParseBuffer( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct OutputPluginContext *p_plugin_ctx , char *block_buf , uint64_t block_len )
{
	char		*p_line = NULL ;
	char		*p_newline = NULL ;
	char		*p_next_newline = NULL ;
	uint64_t	line_len ;
	uint64_t	remain_len ;
	uint64_t	line_add ;
	
	int		nret = 0 ;
	
	INFOLOGC( "before combine , [%d][%.*s]" , p_plugin_ctx->parse_buflen , MIN(p_plugin_ctx->parse_buflen,MAX_OUTPUT_BUF) , p_plugin_ctx->parse_buffer )
	INFOLOGC( "block_buf [%d][%.*s]" , block_len , MIN(block_len,MAX_OUTPUT_BUF) , block_buf )
	
	/* �����������+��ǰ���ݿ�ŵ��½��������� */
	if( p_plugin_ctx->parse_buflen + block_len <= sizeof(p_plugin_ctx->parse_buffer)-1 )
	{
		memcpy( p_plugin_ctx->parse_buffer+p_plugin_ctx->parse_buflen , block_buf , block_len );
		p_plugin_ctx->parse_buflen += block_len ;
	}
	else
	{
		/* ��ǿ�ư��������ݶ������ */
		nret = SendLineBuffer( p_env , p_logpipe_output_plugin , p_plugin_ctx , p_plugin_ctx->parse_buffer , p_plugin_ctx->parse_buflen , 0 , 1 ) ;
		if( nret < 0 )
		{
			ERRORLOGC( "SendLineBuffer failed[%d]" , nret )
			return nret;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "SendLineBuffer return[%d]" , nret )
		}
		else
		{
			DEBUGLOGC( "SendLineBuffer ok" )
		}
		
		memcpy( p_plugin_ctx->parse_buffer , block_buf , block_len );
		p_plugin_ctx->parse_buflen = block_len ;
	}
	
	/* �ѽ�������������Ч�ж������ */
	remain_len = p_plugin_ctx->parse_buflen ;
	line_add = 0 ;
	
	p_line = p_plugin_ctx->parse_buffer ;
	p_newline = memchr( p_line , '\n' , remain_len ) ;
	while( p_newline )
	{
		line_add++;
		
		line_len = p_newline - p_line ;
		remain_len -= line_len+1 ;
		(*p_newline) = '\0' ;
		p_next_newline = memchr( p_newline+1 , '\n' , remain_len ) ;
		nret = SendLineBuffer( p_env , p_logpipe_output_plugin , p_plugin_ctx , p_line , line_len , line_add , p_next_newline?0:1 ) ;
		if( nret < 0 )
		{
			ERRORLOGC( "SendLineBuffer failed[%d]" , nret )
			return nret;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "SendLineBuffer return[%d]" , nret )
		}
		else
		{
			DEBUGLOGC( "SendLineBuffer ok" )
		}
		
		p_line = p_newline+1 ;
		p_newline = p_next_newline ;
	}
	if( line_add )
	{
		memmove( p_plugin_ctx->parse_buffer , p_line , remain_len );
		p_plugin_ctx->parse_buflen = remain_len ;
	}
	
	INFOLOGC( "after combine , [%d][%.*s]" , p_plugin_ctx->parse_buflen , MIN(p_plugin_ctx->parse_buflen,MAX_OUTPUT_BUF) , p_plugin_ctx->parse_buffer )
	
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t block_len , char *block_buf )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	/* ������Ϣ */
	p_plugin_ctx->file_line = file_line ;
	p_plugin_ctx->block_len = block_len ;
	p_plugin_ctx->block_buf = block_buf ;
	
	/* ���ݿ�ϲ������������� */
	nret = CombineToParseBuffer( p_env , p_logpipe_output_plugin , p_plugin_ctx , block_buf , block_len ) ;
	if( nret < 0 )
	{
		ERRORLOGC( "CombineToParseBuffer failed[%d]" , nret )
		return nret;
	}
	else if( nret > 0 )
	{
		WARNLOGC( "CombineToParseBuffer return[%d]" , nret )
	}
	else
	{
		DEBUGLOGC( "CombineToParseBuffer ok" )
	}
	
	return 0;
}

funcAfterWriteOutputPlugin AfterWriteOutputPlugin ;
int AfterWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	return 0;
}

funcCleanOutputPluginContext CleanOutputPluginContext ;
int CleanOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	if( p_plugin_ctx->p_forward_session->sock >= 0 )
	{
		INFOLOGC( "close forward sock[%d]" , p_plugin_ctx->p_forward_session->sock )
		close( p_plugin_ctx->p_forward_session->sock ); p_plugin_ctx->p_forward_session->sock = -1 ;
	}
	
	return 0;
}

funcUnloadOutputPluginConfig UnloadOutputPluginConfig ;
int UnloadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void **pp_context )
{
	struct OutputPluginContext	**pp_plugin_ctx = (struct OutputPluginContext **)pp_context ;
	
	/* �ͷ��ڴ��Դ�Ų�������� */
	free( (*pp_plugin_ctx) ); (*pp_plugin_ctx) = NULL ;
	
	return 0;
}

