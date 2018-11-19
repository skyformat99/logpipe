#include "logpipe_api.h"

int	__LOGPIPE_OUTPUT_FILE_VERSION_0_2_0 = 1 ;

/* �����ļ���Ϣ�ṹ */
struct TraceFile
{
	char		filename[ PATH_MAX + 1 ] ;
	char		path_filename[ PATH_MAX + 1 ] ;
	int		fd ;
	
	struct rb_node	path_filename_rbnode ;
} ;

struct OutputPluginContext
{
	char			*path ;
	char			*uncompress_algorithm ;
	uint64_t		rotate_size ;
	char			exec_after_rotating_buffer[ PATH_MAX * 3 ] ;
	char			*exec_after_rotating ;
	
	struct rb_root		path_filename_rbtree ;
	struct TraceFile	*p_trace_file ;
} ;

LINK_RBTREENODE_STRING( LinkTraceFilePathFilenameTreeNode , struct OutputPluginContext , path_filename_rbtree , struct TraceFile , path_filename_rbnode , path_filename )
QUERY_RBTREENODE_STRING( QueryTraceFilePathFilenameTreeNode , struct OutputPluginContext , path_filename_rbtree , struct TraceFile , path_filename_rbnode , path_filename )
UNLINK_RBTREENODE( UnlinkTraceFilePathFilenameTreeNode , struct OutputPluginContext , path_filename_rbtree , struct TraceFile , path_filename_rbnode )

void FreeTraceFile( void *pv )
{
	struct TraceFile      *p_trace_file = (struct TraceFile *) pv ;
	
	if( p_trace_file )
	{
		free( p_trace_file ); p_trace_file = NULL ;
	}
	
	return;
}
DESTROY_RBTREE( DestroyTraceFileTree , struct OutputPluginContext , path_filename_rbtree , struct TraceFile , path_filename_rbnode , FreeTraceFile )

funcLoadOutputPluginConfig LoadOutputPluginConfig ;
int LoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct OutputPluginContext	*p_plugin_ctx = NULL ;
	char				*p = NULL ;
	
	/* �����ڴ��Դ�Ų�������� */
	p_plugin_ctx = (struct OutputPluginContext *)malloc( sizeof(struct OutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
	p_plugin_ctx->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	INFOLOGC( "path[%s]" , p_plugin_ctx->path )
	if( p_plugin_ctx->path == NULL || p_plugin_ctx->path[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'path'" )
		return -1;
	}
	
	p_plugin_ctx->uncompress_algorithm = QueryPluginConfigItem( p_plugin_config_items , "uncompress_algorithm" ) ;
	if( p_plugin_ctx->uncompress_algorithm )
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			;
		}
		else
		{
			ERRORLOGC( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm )
			return -1;
		}
	}
	INFOLOGC( "uncompress_algorithm[%s]" , p_plugin_ctx->uncompress_algorithm )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "rotate_size" ) ;
	if( p )
		p_plugin_ctx->rotate_size = size64_atou64(p) ;
	else
		p_plugin_ctx->rotate_size = 0 ;
	INFOLOGC( "rotate_size[%"PRIu64"]" , p_plugin_ctx->rotate_size )
	if( p_plugin_ctx->rotate_size == UINT64_MAX )
	{
		ERRORLOGC( "rotate_size[%"PRIu64"] invalid" , p_plugin_ctx->rotate_size )
		return -1;
	}
	
	p = QueryPluginConfigItem( p_plugin_config_items , "exec_after_rotating" ) ;
	if( p )
	{
		int		buffer_len = 0 ;
		int		remain_len = sizeof(p_plugin_ctx->exec_after_rotating_buffer)-1 ;
		
		memset( p_plugin_ctx->exec_after_rotating_buffer , 0x00 , sizeof(p_plugin_ctx->exec_after_rotating_buffer) );
		JSONUNESCAPE_FOLD( p , strlen(p) , p_plugin_ctx->exec_after_rotating_buffer , buffer_len , remain_len )
		if( buffer_len == -1 )
		{
			ERRORLOGC( "p[%s] invalid" , p )
			return -1;
		}
		
		p_plugin_ctx->exec_after_rotating = p_plugin_ctx->exec_after_rotating_buffer ;
	}
	INFOLOGC( "exec_after_rotating[%s]" , p_plugin_ctx->exec_after_rotating )
	
	/* ���ò������������ */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	return 0;
}

funcOnOutputPluginEvent OnOutputPluginEvent;
int OnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	return 0;
}

/* ���ļ� */
static int OpenFile( struct TraceFile *p_trace_file )
{
	p_trace_file->fd = open( p_trace_file->path_filename , O_CREAT|O_WRONLY|O_APPEND , 00777 ) ;
	if( p_trace_file->fd == -1 )
	{
		ERRORLOGC( "open file[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		return 1;
	}
	else
	{
		INFOLOGC( "open file[%s] ok" , p_trace_file->path_filename )
	}
	
	return 0;
}

/* �ر��ļ� */
static void CloseFile( struct TraceFile *p_trace_file )
{
	if( p_trace_file->fd >= 0 )
	{
		INFOLOGC( "close file[%s]" , p_trace_file->path_filename )
		close( p_trace_file->fd ); p_trace_file->fd = -1 ;
	}
	
	return;
}

/* �ļ���Сת������ */
static int RotatingFile( struct OutputPluginContext *p_plugin_ctx )
{
	struct TraceFile	*p_trace_file = p_plugin_ctx->p_trace_file ;
	struct timeval		tv ;
	struct tm		tm ;
	char			old_filename[ PATH_MAX + 1 ] ;
	char			new_filename[ PATH_MAX + 1 ] ;
	
	int			nret = 0 ;
	
	/* �ر��ļ� */
	CloseFile( p_trace_file );
	
	/* ���� */
	snprintf( old_filename , sizeof(old_filename)-1 , "%s/%s" , p_plugin_ctx->path , p_trace_file->filename );
	gettimeofday( & tv , NULL );
	memset( & tm , 0x00 , sizeof(struct tm) );
	localtime_r( & (tv.tv_sec) , & tm );
	memset( new_filename , 0x00 , sizeof(new_filename) );
	snprintf( new_filename , sizeof(new_filename)-1 , "%s/_%s-%04d%02d%02d_%02d%02d%02d_%06ld" , p_plugin_ctx->path , p_trace_file->filename , tm.tm_year+1900 , tm.tm_mon+1 , tm.tm_mday , tm.tm_hour , tm.tm_min , tm.tm_sec , tv.tv_usec );
	
	setenv( "LOGPIPE_ROTATING_PATHNAME" , p_plugin_ctx->path , 1 );
	setenv( "LOGPIPE_ROTATING_NEW_FILENAME" , new_filename , 1 );
	
	nret = rename( old_filename , new_filename ) ;
	
	if( p_plugin_ctx->exec_after_rotating )
	{
		system( p_plugin_ctx->exec_after_rotating );
	}
	
	if( nret )
	{
		FATALLOGC( "rename [%s] to [%s] failed , errno[%d]" , old_filename , new_filename , errno )
		return -1;
	}
	else
	{
		INFOLOGC( "rename [%s] to [%s] ok" , old_filename , new_filename )
	}
	
	/* ���ļ� */
	OpenFile( p_trace_file );
	
	return 0;
}

funcBeforeWriteOutputPlugin BeforeWriteOutputPlugin ;
int BeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	struct TraceFile		trace_file ;
	struct TraceFile		*p_trace_file = NULL ;
	int				len ;
	
	int				nret = 0 ;
	
	/* ��ѯ�ļ�������Ϣ���� */
	memset( & trace_file , 0x00 , sizeof(struct TraceFile) );
	snprintf( trace_file.path_filename , sizeof(trace_file.path_filename)-1 , "%s/%.*s" , p_plugin_ctx->path , filename_len , filename );
	p_trace_file = QueryTraceFilePathFilenameTreeNode( p_plugin_ctx , & trace_file ) ;
	if( p_trace_file )
	{
		/* ����ļ���ɾ���ˣ�����ļ����ٽṹ���� */
		nret = access( p_trace_file->path_filename , F_OK ) ;
		if( nret == - 1 )
		{
			INFOLOGC( "access[%.*s] failed , errno[%d]" , filename_len , filename , errno )
			UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
			CloseFile( p_trace_file );
			FreeTraceFile( p_trace_file );
			
			p_trace_file = NULL ;
		}
		else
		{
			p_plugin_ctx->p_trace_file = p_trace_file ;
		}
	}
	if( p_trace_file == NULL )
	{
		/* �����ڴ��Դ���ļ����ٽṹ */
		p_trace_file = (struct TraceFile *)malloc( sizeof(struct TraceFile) ) ;
		if( p_trace_file == NULL )
		{
			ERRORLOGC( "malloc failed , errno[%d]" , errno )
			return 1;
		}
		memset( p_trace_file , 0x00 , sizeof(struct TraceFile) );
		p_trace_file->fd = -1 ;
		
		/* ����ļ����ٽṹ */
		if( filename_len > sizeof(p_trace_file->filename)-1 )
		{
			ERRORLOGC( "filename[%.*s] overflow" , filename_len , filename )
			FreeTraceFile( p_trace_file );
			return 1;
		}
		strncpy( p_trace_file->filename , filename , filename_len );
		len = snprintf( p_trace_file->path_filename , sizeof(p_trace_file->path_filename)-1 , "%s/%.*s" , p_plugin_ctx->path , filename_len , filename ) ;
		if( SNPRINTF_OVERFLOW(len,sizeof(p_trace_file->path_filename) ) )
		{
			ERRORLOGC( "filename[%.*s] overflow" , filename_len , filename )
			FreeTraceFile( p_trace_file );
			return 1;
		}
		
		/* ���ļ� */
		nret = OpenFile( p_trace_file ) ;
		if( nret )
		{
			ERRORLOGC( "open[%s] failed[%d]" , p_trace_file->path_filename , nret )
			FreeTraceFile( p_trace_file );
			return 1;
		}
		else
		{
			INFOLOGC( "open[%s] ok" , p_trace_file->path_filename )
		}
		
		LinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
		
		p_plugin_ctx->p_trace_file = p_trace_file ;
	}
	
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t block_len , char *block_buf )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	struct TraceFile		*p_trace_file = p_plugin_ctx->p_trace_file ;
	
	int				len ;
	
	int				nret = 0 ;
	
	/* ���δ���ý�ѹ */
	if( p_plugin_ctx->uncompress_algorithm == NULL )
	{
		len = writen( p_trace_file->fd , block_buf , block_len ) ;
		if( len == -1 )
		{
			ERRORLOGC( "write block data to file[%d][%s] failed , errno[%d]" , p_trace_file->fd , p_trace_file->path_filename , errno )
			UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
			CloseFile( p_trace_file );
			FreeTraceFile( p_trace_file );
			return 1;
		}
		else
		{
			INFOLOGC( "write block data to file[%d][%s] ok , [%d]bytes [%.100s...]" , p_trace_file->fd , p_trace_file->path_filename , block_len , block_buf )
			DEBUGHEXLOGC( block_buf , len , NULL )
		}
	}
	/* ��������˽�ѹ */
	else
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			char			block_out_buf[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ;
			uint64_t		block_out_len ;
			
			memset( block_out_buf , 0x00 , sizeof(block_out_buf) );
			nret = UncompressInputPluginData( p_plugin_ctx->uncompress_algorithm , block_buf , block_len , block_out_buf , & block_out_len ) ;
			if( nret )
			{
				ERRORLOGC( "UncompressInputPluginData failed[%d]" , nret )
				UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
				CloseFile( p_trace_file );
				FreeTraceFile( p_trace_file );
				return -1;
			}
			else
			{
				DEBUGLOGC( "UncompressInputPluginData ok" )
			}
			
			len = writen( p_trace_file->fd , block_out_buf , block_out_len ) ;
			if( len == -1 )
			{
				ERRORLOGC( "write uncompress block data to file[%d][%s] failed , errno[%d]" , p_trace_file->fd , p_trace_file->path_filename , errno )
				UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
				CloseFile( p_trace_file );
				FreeTraceFile( p_trace_file );
				return 1;
			}
			else
			{
				INFOLOGC( "write uncompress block data to file[%d][%s] ok , [%"PRIu64"]bytes [%.100s...]" , p_trace_file->fd , p_trace_file->path_filename , block_out_len , block_buf )
				DEBUGHEXLOGC( block_out_buf , len , NULL )
			}
		}
		else
		{
			ERRORLOGC( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm )
			return -1;
		}
	}
	
	/* ��������ļ���Сת�� */
	if( p_plugin_ctx->rotate_size > 0 )
	{
		struct stat		file_stat ;
		
		memset( & file_stat , 0x00 , sizeof(struct stat) );
		nret = fstat( p_trace_file->fd , & file_stat ) ;
		if( nret == -1 )
		{
			ERRORLOGC( "fstat[%s] failed[%d] , errno[%d]" , p_trace_file->path_filename , nret , errno )
			UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
			CloseFile( p_trace_file );
			FreeTraceFile( p_trace_file );
		}
		else
		{
			/* ��������ļ�ת����С */
			if( file_stat.st_size >= p_plugin_ctx->rotate_size )
			{
				INFOLOGC( "file_stat.st_size[%"PRIu64"] > p_plugin_ctx->rotate_size[%"PRIu64"]" , file_stat.st_size , p_plugin_ctx->rotate_size )
				
				/* �ļ���Сת������ */
				nret = RotatingFile( p_plugin_ctx ) ;
				if( nret )
				{
					UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
					CloseFile( p_trace_file );
					FreeTraceFile( p_trace_file );
				}
			}
		}
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
	
	DestroyTraceFileTree( p_plugin_ctx );
	
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

