/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

/* QUIT�źű�־ */
static sig_atomic_t		g_QUIT_flag = 0 ;

/* �źŴ���ص����� */
static void sig_set_flag( int sig_no )
{
	/* ���յ���ͬ�ź����ò�ͬ��ȫ�ֱ�־���Ӻ��������д��� */
	if( sig_no == SIGTERM )
	{
		g_QUIT_flag = 1 ; /* �˳� */
	}
	
	return;
}

/* ����ֻ����һ�ε����ò�����ͨ�������������� */
static void SetStartOnceEnv( struct LogpipePluginConfigItem *start_once )
{
	struct LogpipePluginConfigItem	*item = NULL ;
	
	list_for_each_entry( item , & (start_once->this_node) , struct LogpipePluginConfigItem , this_node )
	{
		setenv( item->key , item->value , 1 );
	}
	
	return;
}

/* ���ֻ����һ�ε����ò��� */
static void UnsetStartOnceEnv( struct LogpipePluginConfigItem *start_once )
{
	struct LogpipePluginConfigItem	*item = NULL ;
	
	list_for_each_entry( item , & (start_once->this_node) , struct LogpipePluginConfigItem , this_node )
	{
		unsetenv( item->key );
	}
	
	return;
}

/* ������������ */
int monitor( struct LogpipeEnv *p_env )
{
	struct sigaction	act ;
	
	pid_t			pid , pid2 ;
	int			status ;
	
	time_t			tt ;
	struct tm		stime ;
	struct tm		old_stime ;
	
	int			nret = 0 ;
	
	/* �����ź� */
	if( ! p_env->no_daemon )
	{
		signal( SIGINT , SIG_IGN );
	}
	signal( SIGCLD , SIG_DFL );
	signal( SIGCHLD , SIG_DFL );
	signal( SIGPIPE , SIG_IGN );
	act.sa_handler = & sig_set_flag ;
	sigemptyset( & (act.sa_mask) );
	act.sa_flags = 0 ;
	sigaction( SIGTERM , & act , NULL );
	act.sa_flags = SA_RESTART ;
	signal( SIGCLD , SIG_DFL );
	
	/* ����ֻ����һ�ε����ò��� */
	SetStartOnceEnv( & (p_env->start_once_for_plugin_config_items) );
	
	/* �����ʼʱ�� */
	time( & tt );
	memset( & old_stime , 0x00 , sizeof(struct tm) );
	localtime_r( & tt , & old_stime );
	
	while( g_QUIT_flag == 0 )
	{
		/* �������ӽ�������ܵ� */
		nret = pipe( p_env->quit_pipe ) ;
		if( nret == -1 )
		{
			FATALLOGC( "pipe failed , errno[%d]" , errno )
			return -1;
		}
		
		/* ������������ */
		pid = fork() ;
		if( pid == -1 )
		{
			FATALLOGC( "fork failed , errno[%d]" , errno )
			return -1;
		}
		else if( pid == 0 )
		{
			close( p_env->quit_pipe[1] );
			INFOLOGC( "child : [%ld] fork [%ld]" , getppid() , getpid() )
			exit(-worker(p_env));
		}
		else
		{
			close( p_env->quit_pipe[0] );
			INFOLOGC( "parent : [%ld] fork [%ld]" , getpid() , pid )
			/* ���ֻ����һ�ε����ò��� */
			UnsetStartOnceEnv( & (p_env->start_once_for_plugin_config_items) );
		}
		
_GOTO_WAITPID :
		
		/* �����ȴ��������̽��� */
		DEBUGLOGC( "waitpid ..." )
		pid2 = waitpid( pid , & status , 0 );
		
		/* �����ǰʱ������һ�β�һ�£����л���־�ļ� */
		time( & tt );
		memset( & stime , 0x00 , sizeof(struct tm) );
		localtime_r( & tt , & stime );
		if( stime.tm_hour != old_stime.tm_hour )
		{
			SetLogcFile( "%s.%d" , p_env->log_file , stime.tm_hour );
			memcpy( & old_stime , & stime , sizeof(struct tm) );
		}
		
		if( pid2 == -1 )
		{
			if( errno == EINTR )
			{
				/* ������˳��ź��жϣ��˳� */
				if( g_QUIT_flag )
				{
					if( p_env->quit_pipe[1] >= 0 )
					{
						close( p_env->quit_pipe[1] ); p_env->quit_pipe[1] = -1 ;
					}
					else
					{
						kill( pid , SIGKILL );
					}
					goto _GOTO_WAITPID;
				}
			}
			
			FATALLOGC( "waitpid failed , errno[%d]" , errno )
			return -1;
		}
		else if( pid2 != pid )
		{
			FATALLOGC( "unexpect other child[%d]" , pid2 )
		}
		
		/* ��鹤�������Ƿ��������� */
		if( WEXITSTATUS(status) == 0 && WIFSIGNALED(status) == 0 && WTERMSIG(status) == 0 )
		{
			INFOLOGC( "waitpid[%d] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d]" , pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) )
		}
		else
		{
			FATALLOGC( "waitpid[%d] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d]" , pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) )
		}
		
		if( p_env->quit_pipe[1] >= 0 )
		{
			close( p_env->quit_pipe[1] ); p_env->quit_pipe[1] = -1 ;
		}
		
		sleep(1);
		
		/* ���´�������ܵ��������������� */
	}
	
	return 0;
}

/* ��������ں��� */
int _monitor( void *pv )
{
	struct LogpipeEnv	*p_env = (struct LogpipeEnv *)pv ;
	time_t			tt ;
	struct tm		stime ;
	
	int			nret = 0 ;
	
	/* ���ø�������־�ļ��� */
	time( & tt );
	memset( & stime , 0x00 , sizeof(struct tm) );
	localtime_r( & tt , & stime );
	SetLogcFile( "%s.%d" , p_env->log_file , stime.tm_hour );
	SetLogcLevel( p_env->log_level );
	
	INFOLOGC( "--- monitor begin ---------" )
	
	nret = monitor( p_env ) ;
	
	INFOLOGC( "--- monitor end ---------" )
	
	return nret;
}

