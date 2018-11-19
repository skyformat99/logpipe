/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

/* cmd for testing
ps -f -u $USER | grep "logpipe -f" | awk '{if($3==1)print $2}' | xargs kill
*/

char __LOGPIPE_VERSION_1_0_8[] = "1.0.8" ;
char *__LOGPIPE_VERSION = __LOGPIPE_VERSION_1_0_8 ;

/* ��ʾ�汾�� */
static void version()
{
	printf( "logpipe v%s build %s %s\n" , __LOGPIPE_VERSION , __DATE__ , __TIME__ );
	return;
}

/* ��ʾ�������﷨ */
static void usage()
{
	printf( "USAGE : logpipe -v\n" );
	printf( "        logpipe -f (config_file) [ --no-daemon ] [ --start-once-for-env \"(key) (value)\" ]\n" );
	printf( "NOTICE : shell : ulimit -n 65536\n" );
	printf( "NOTICE : add to sysctl.conf : fs.inotify.max_user_watches=99999999\n" );
        printf( "                              fs.inotify.max_queued_events=99999999\n" );
	return;
}

#define START_ONCE_FOR_ENV		"--start-once-for-env"

/* ���������в��� */
static int ParseCommandParameters( struct LogpipeEnv *p_env , int argc , char *argv[] )
{
	int		c ;
	
	int		nret = 0 ;
	
	for( c = 1 ; c < argc ; c++ )
	{
		if( STRCMP( argv[c] , == , "-v" ) )
		{
			version();
			exit(0);
		}
		else if( STRCMP( argv[c] , == , "-f" ) && c + 1 < argc )
		{
			strncpy( p_env->config_path_filename , argv[c+1] , sizeof(p_env->config_path_filename)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , START_ONCE_FOR_ENV ) && c + 1 < argc )
		{
			char	key[ 256 + 1 ] ;
			char	value[ 256 + 1 ] ;
			
			memset( key , 0x00 , sizeof(key) );
			memset( value , 0x00 , sizeof(value) );
			sscanf( argv[c+1] , "%s%s" , key , value );
			nret = AddPluginConfigItem( & (p_env->start_once_for_plugin_config_items) , key , strlen(key) , value , strlen(value) ) ;
			if( nret )
			{
				ERRORLOGC( "AddPluginConfigItem [%s][%s] failed" , key , value )
				return -1;
			}
			c++;
		}
		else if( STRCMP( argv[c] , == , "--no-daemon" ) )
		{
			p_env->no_daemon = 1 ;
		}
		else
		{
			printf( "ERROR : invalid command parameter '%s'\n" , argv[c] );
			usage();
			exit(1);
		}
	}
	
	return 0;
}

/* ��ں��� */
int main( int argc , char *argv[] )
{
	struct LogpipeEnv	*p_env = NULL ;
	
	int			nret = 0 ;
	
	/* ���ñ�׼����޻��� */
	setbuf( stdout , NULL );
	
	if( argc == 1 )
	{
		usage();
		exit(0);
	}
	
	/* ������־������������Ļ�� */
	SetLogcFile( "#stdout" );
	SetLogcLevel( LOGCLEVEL_DEBUG );
	
	/* �����ڴ�������ṹ */
	p_env = (struct LogpipeEnv *)malloc( sizeof(struct LogpipeEnv) ) ;
	if( p_env == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return 1;
	}
	memset( p_env , 0x00 , sizeof(struct LogpipeEnv) );
	
	/* ��ʼ�������ṹ */
	INIT_LIST_HEAD( & (p_env->start_once_for_plugin_config_items.this_node) );
	p_env->epoll_fd = -1 ;
	INIT_LIST_HEAD( & (p_env->logpipe_input_plugins_list.this_node) );
	INIT_LIST_HEAD( & (p_env->logpipe_output_plugins_list.this_node) );
	
	/* ���������в��� */
	nret = ParseCommandParameters( p_env , argc , argv ) ;
	if( nret )
		return -nret;
	
	/* װ�������ļ� */
	nret = LoadConfig( p_env ) ;
	if( nret )
		return -nret;
	
	if( list_empty( & (p_env->logpipe_input_plugins_list.this_node) ) )
	{
		ERRORLOGC( "no inputs" )
		return 1;
	}
	/*
	if( list_empty( & (p_env->logpipe_output_plugins_list.this_node) ) )
	{
		ERRORLOGC( "no outputs" )
		return 1;
	}
	*/
	
	/* ���븸���̴��� */
	if( p_env->no_daemon )
	{
		umask( 0 ) ;
		chdir( "/tmp" );
		nret = _monitor( (void*)p_env ) ;
	}
	else
	{
		/* ���ػ����̷�ʽ */
		nret = BindDaemonServer( & _monitor , (void*)p_env , 1 ) ;
	}
	
	/* ж������ */
	UnloadConfig( p_env );
	
	/* �ͷŻ����ṹ */
	free( p_env );
	
	return -nret;
}

