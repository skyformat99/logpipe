/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

/* ÿ�����ɽ���epoll�¼����� */
#define MAX_EPOLL_EVENTS	10000

/* �ӽ��������� */
int worker( struct LogpipeEnv *p_env )
{
	time_t			tt ;
	struct tm		stime ;
	struct tm		old_stime ;
	
	int			quit_flag ;
	
	struct epoll_event	event ;
	struct epoll_event	events[ MAX_EPOLL_EVENTS ] ;
	int			epoll_nfds ;
	int			timeout ;
	int			i ;
	struct epoll_event	*p_event = NULL ;
	
	int			nret = 0 ;
	
	/* ���TERM�źŻص��������� */
	signal( SIGTERM , SIG_DFL );
	
	/* �����ӽ�����־�ļ��� */
	time( & tt );
	memset( & old_stime , 0x00 , sizeof(struct tm) );
	localtime_r( & tt , & old_stime );
	SetLogcFile( "%s.%d" , p_env->log_file , old_stime.tm_hour );
	SetLogcLevel( p_env->log_level );
	
	/* �����¼����� */
	p_env->epoll_fd = epoll_create( 1024 ) ;
	if( p_env->epoll_fd == -1 )
	{
		ERRORLOGC( "epoll_create failed , errno[%d]" , errno )
		return -1;
	}
	else
	{
		INFOLOGC( "epoll_create ok , epoll_fd[%d]" , p_env->epoll_fd )
	}
	
	/* ��ʼ��������� */
	nret = InitEnvironment( p_env ) ;
	if( nret )
	{
		ERRORLOGC( "InitEnvironment failed[%d]" , nret )
		return -1;
	}
	else
	{
		INFOLOGC( "InitEnvironment ok" )
	}
	
	/* �ܵ������ּ���epoll */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = p_env->quit_pipe ;
	nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_env->quit_pipe[0] , & event ) ;
	if( nret == -1 )
	{
		ERRORLOGC( "epoll_ctl[%d] add quit pipe fd[%d] failed , errno[%d]" , p_env->epoll_fd , p_env->quit_pipe[0] , errno )
		return -1;
	}
	else
	{
		INFOLOGC( "epoll_ctl[%d] add quit pipe fd[%d] ok" , p_env->epoll_fd , p_env->quit_pipe[0] )
	}
	
#if 0
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
#endif
	
	/* ������ѭ�� */
	quit_flag = 0 ;
	while( quit_flag == 0 )
	{
		/* �����ǰʱ������һ�β�һ�£����л���־�ļ� */
		time( & tt );
		memset( & stime , 0x00 , sizeof(struct tm) );
		localtime_r( & tt , & stime );
		if( stime.tm_hour != old_stime.tm_hour )
		{
			SetLogcFile( "%s.%d" , p_env->log_file , stime.tm_hour );
			unlink( GetLogcFilePtr() );
			memcpy( & old_stime , & stime , sizeof(struct tm) );
		}
		
		/* �ȴ�epoll�¼�������1�볬ʱ */
		INFOLOGC( "epoll_wait[%d] ..." , p_env->epoll_fd )
		memset( events , 0x00 , sizeof(events) );
		if( p_env->idle_processing_flag )
			timeout = 1000 ;
		else
			timeout = -1 ;
		epoll_nfds = epoll_wait( p_env->epoll_fd , events , MAX_EPOLL_EVENTS , timeout ) ;
		if( epoll_nfds == -1 )
		{
			if( errno == EINTR )
			{
				INFOLOGC( "epoll_wait[%d] interrupted" , p_env->epoll_fd )
			}
			else
			{
				ERRORLOGC( "epoll_wait[%d] failed , errno[%d]" , p_env->epoll_fd , errno )
				return -1;
			}
		}
		else if( epoll_nfds == 0 )
		{
			nret = ProcessOnIdle( p_env ) ;
			if( nret )
			{
				ERRORLOGC( "ProcessOnIdle failed[%d] , errno[%d]" , nret , errno )
				return -1;
			}
			else
			{
				DEBUGLOGC( "ProcessOnIdle ok" )
			}
		}
		else
		{
			INFOLOGC( "epoll_wait[%d] return[%d]events" , p_env->epoll_fd , epoll_nfds )
		}
		
		/* ѭ����������epoll�¼� */
		for( i = 0 , p_event = events ; i < epoll_nfds ; i++ , p_event++ )
		{
			/* ����Ǹ��ӽ�������ܵ� */
			if( p_event->data.ptr == p_env->quit_pipe )
			{
				DEBUGLOGC( "p_event->data.ptr[%p] quit_pipe" , p_event->data.ptr )
				quit_flag = 1 ;
			}
#if 0
			else if( p_event->data.ptr == & (p_env->logpipe_fifo_inotify_fd) )
			{
				/* �����ڲ�״̬��������ܵ��¼� */
				nret = ProcessLogpipeFifoEvents( p_env ) ;
				if( nret )
				{
					ERRORLOGC( "ProcessLogpipeFifoEvents failed[%d]\n" , nret )
					return -1;
				}
				else
				{
					DEBUGLOGC( "ProcessLogpipeFifoEvents ok\n" )
				}
			}
#endif
			else
			{
				struct LogpipePlugin	*p_logpipe_plugin = NULL ;
				
				p_logpipe_plugin = (struct LogpipePlugin *)(p_event->data.ptr) ;
				/* ����������� */
				if( p_logpipe_plugin->type == LOGPIPE_PLUGIN_TYPE_INPUT )
				{
					struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
					char				so_filename[ sizeof(((struct LogpipeInputPlugin *)0)->so_filename) ] ;
					
					DEBUGLOGC( "p_event->data.ptr[%p] p_logpipe_input_plugin" , p_event->data.ptr )
					
					p_logpipe_input_plugin = (struct LogpipeInputPlugin *)(p_event->data.ptr) ;
					strcpy( so_filename , p_logpipe_input_plugin->so_filename );
					
					/* �ɶ��¼� */
					if( p_event->events & EPOLLIN )
					{
						/* �����������¼��ص����� */
						nret = p_logpipe_input_plugin->pfuncOnInputPluginEvent( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context ) ;
						if( nret < 0 )
						{
							FATALLOGC( "[%s]->pfuncOnInputPluginEvent failed[%d]" , so_filename , nret )
							return -1;
						}
						else if( nret > 0 )
						{
							WARNLOGC( "[%s]->pfuncOnInputPluginEvent return[%d]" , so_filename , nret )
						}
						else
						{
							DEBUGLOGC( "[%s]->pfuncOnInputPluginEvent ok" , so_filename )
						}
					}
					/* �����¼� */
					else
					{
						FATALLOGC( "[%s]->pfuncOnInputPluginEvent unknow event[0x%X]" , so_filename , p_event->events )
						return -1;
					}
				}
				/* ����������� */
				else if( p_logpipe_plugin->type == LOGPIPE_PLUGIN_TYPE_OUTPUT )
				{
					struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
					char				so_filename[ sizeof(((struct LogpipeOutputPlugin *)0)->so_filename) ] ;
					
					DEBUGLOGC( "p_event->data.ptr[%p] p_logpipe_output_plugin" , p_event->data.ptr )
					
					p_logpipe_output_plugin = (struct LogpipeOutputPlugin *)(p_event->data.ptr) ;
					strcpy( so_filename , p_logpipe_output_plugin->so_filename );
					
					/* �ɶ��¼� */
					if( p_event->events & EPOLLIN )
					{
						/* �����������¼��ص����� */
						nret = p_logpipe_output_plugin->pfuncOnOutputPluginEvent( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
						if( nret < 0 )
						{
							FATALLOGC( "[%s]->pfuncOnOutputPluginEvent failed[%d]" , so_filename , nret )
							return -1;
						}
						else if( nret > 0 )
						{
							WARNLOGC( "[%s]->pfuncOnOutputPluginEvent return[%d]" , so_filename , nret )
						}
						else
						{
							DEBUGLOGC( "[%s]->pfuncOnOutputPluginEvent ok" , so_filename )
						}
					}
					/* �����¼� */
					else
					{
						FATALLOGC( "[%s]->pfuncOnOutputPluginEvent unknow event[0x%X]" , so_filename , p_event->events )
						return -1;
					}
				}
				else
				{
					FATALLOGC( "unknow plugin[%p] type[%c]" , p_event->data.ptr , p_logpipe_plugin->type )
					return -1;
				}
				
			}
		}
	}
	
	/* ���������� */
	CleanEnvironment( p_env );
	INFOLOGC( "CleanEnvironment" )
	
	/* �ر��¼����� */
	INFOLOGC( " close epoll_fd[%d]" , p_env->epoll_fd )
	close( p_env->epoll_fd );
	
	return 0;
}

