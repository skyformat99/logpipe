/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

#if 0

/* �����ڲ�״̬��������ܵ� */
int CreateLogpipeFifo( struct LogpipeEnv *p_env )
{
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	/* �����ڲ�״̬��������ܵ� */
	nret = mkfifo( p_env->logpipe_fifo_path_filename , 00777 ) ;
	if( nret == -1 )
	{
		INFOLOGC( "mkfifo[%s] failed[%d]" , p_env->logpipe_fifo_path_filename , nret )
	}
	else
	{
		INFOLOGC( "mkfifo[%s] ok" , p_env->logpipe_fifo_path_filename )
	}
	
	/* �����ڲ�״̬��������ܵ��ļ���������� */
	p_env->logpipe_fifo_inotify_fd = inotify_init() ;
	if( p_env->logpipe_fifo_inotify_fd == -1 )
	{
		ERRORLOGC( "inotify_init failed , errno[%d]" , errno )
		return -1;
	}
	
	/* ע���ڲ�״̬��������ܵ��ļ����¼� */
	p_env->logpipe_fifo_inotify_wd = inotify_add_watch( p_env->logpipe_fifo_inotify_fd , p_env->logpipe_fifo_path_filename , (uint32_t)(IN_OPEN|IN_DELETE_SELF|IN_MOVE_SELF|IN_Q_OVERFLOW) );
	if( p_env->logpipe_fifo_inotify_wd == -1 )
	{
		ERRORLOGC( "inotify_add_watch[%s] failed , errno[%d]" , p_env->logpipe_fifo_path_filename , errno )
		return -1;
	}
	else
	{
		INFOLOGC( "inotify_add_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_env->logpipe_fifo_path_filename , p_env->logpipe_fifo_inotify_fd , p_env->logpipe_fifo_inotify_wd )
	}
	
	/* �ڲ�״̬��������ܵ������ּ���epoll */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = & (p_env->logpipe_fifo_inotify_fd) ;
	nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_env->logpipe_fifo_inotify_fd , & event ) ;
	if( nret == -1 )
	{
		ERRORLOGC( "epoll_ctl[%d] add logpipe_fifo fd[%d] failed , errno[%d]" , p_env->epoll_fd , p_env->logpipe_fifo_inotify_fd , errno )
		return -1;
	}
	else
	{
		INFOLOGC( "epoll_ctl[%d] add logpipe_fifo fd[%d] ok" , p_env->epoll_fd , p_env->logpipe_fifo_inotify_fd )
	}
	
	return 0;
}

/* �����ڲ�״̬��������ܵ��¼� */
int ProcessLogpipeFifoEvents( struct LogpipeEnv *p_env )
{
	char			inotify_read_buffer[ 1024 + 1 ] ;
	long			inotify_read_buflen ;
	struct inotify_event	*p_inotify_event = NULL ;
	struct inotify_event	*p_overflow_inotify_event = NULL ;
	
	int			nret = 0 ;
	
	/* ���ļ��仯�¼� */
	DEBUGLOGC( "read logpipe_fifo_inotify[%d] ..." , p_env->logpipe_fifo_inotify_fd )
	memset( inotify_read_buffer , 0x00 , sizeof(inotify_read_buffer) );
	inotify_read_buflen = read( p_env->logpipe_fifo_inotify_fd , inotify_read_buffer , sizeof(inotify_read_buffer)-1 ) ;
	if( inotify_read_buflen == -1 )
	{
		FATALLOGC( "read logpipe_fifo_inotify[%d] failed , errno[%d]" , p_env->logpipe_fifo_inotify_fd , errno )
		return -1;
	}
	else
	{
		INFOLOGC( "read logpipe_fifo_inotify[%d] ok , [%d]bytes" , p_env->logpipe_fifo_inotify_fd , inotify_read_buflen )
	}
	
	p_inotify_event = (struct inotify_event *)(inotify_read_buffer) ;
	p_overflow_inotify_event = (struct inotify_event *)(inotify_read_buffer+inotify_read_buflen) ;
	while( p_inotify_event < p_overflow_inotify_event )
	{
		if( p_inotify_event->mask & IN_DELETE_SELF || p_inotify_event->mask & IN_MOVE_SELF )
		{
			INFOLOGC( "INOTIFY_EVENT IN_DELETE_SELF or IN_MOVED_SELF , wd[%d] mask[0x%X] cookie[%d] len[%d] name[%.*s]" , p_inotify_event->wd , p_inotify_event->mask , p_inotify_event->cookie , p_inotify_event->len , p_inotify_event->len , p_inotify_event->name )
			
			/* ����ļ���ض��� */
			inotify_rm_watch( p_env->logpipe_fifo_inotify_fd , p_env->logpipe_fifo_inotify_wd );
			
			/* ɾ���ļ���������� */
			epoll_ctl( p_env->epoll_fd , EPOLL_CTL_DEL , p_env->logpipe_fifo_inotify_fd , NULL );
			close( p_env->logpipe_fifo_inotify_fd );
			
			/* �����ڲ�״̬��������ܵ� */
			memset( p_env->logpipe_fifo_path_filename , 0x00 , sizeof(p_env->logpipe_fifo_path_filename) );
			snprintf( p_env->logpipe_fifo_path_filename , sizeof(p_env->logpipe_fifo_path_filename)-1 , "%s/etc/logpipe.fifo" , getenv("HOME") );
			nret = CreateLogpipeFifo( p_env ) ;
			if( nret )
			{
				ERRORLOGC( "CreateLogpipeFifo failed[%d]" , nret )
				return -1;
			}
			else
			{
				INFOLOGC( "CreateLogpipeFifo ok" )
			}
		}
		else if( p_inotify_event->mask & IN_OPEN )
		{
			INFOLOGC( "INOTIFY_EVENT IN_OPEN , wd[%d] mask[0x%X] cookie[%d] len[%d] name[%.*s]" , p_inotify_event->wd , p_inotify_event->mask , p_inotify_event->cookie , p_inotify_event->len , p_inotify_event->len , p_inotify_event->name )
			
			FILE	*fp = NULL ;
			
			fp = fopen( p_env->logpipe_fifo_path_filename , "w" ) ;
			if( fp )
			{
				fprintf( fp , "logpipe v%s build %s %s\n" , __LOGPIPE_VERSION , __DATE__ , __TIME__ );
				fprintf( fp , "-----------------------------------\n" );
				fprintf( fp , "      config_path_filename[%s]\n" , p_env->config_path_filename );
				fprintf( fp , "                  log_file[%s]\n" , p_env->log_file );
				fprintf( fp , "logpipe_fifo_path_filename[%s]\n" , p_env->logpipe_fifo_path_filename );
				
				fputc( EOF , fp );
				fclose( fp );
			}
		}
		else
		{
			ERRORLOGC( "unknow file inotify event mask[0x%X]" , p_inotify_event->mask )
		}
		
		p_inotify_event = (struct inotify_event *)( (char*)p_inotify_event + sizeof(struct inotify_event) + p_inotify_event->len ) ;
	}
	
	return 0;
}

#endif
