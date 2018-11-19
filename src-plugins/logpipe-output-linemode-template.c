#include "logpipe_api.h"

int	__LOGPIPE_OUTPUT_LINEMODE_TEMPLATE_VERSION_0_1_0 = 1 ;

/* ��������ṹ */
#define PARSE_BUFFER_SIZE		LOGPIPE_BLOCK_BUFSIZE*2

#define MAX_OUTPUT_BUF			256

struct OutputPluginContext
{
	char			*key ;
	
	char			*filename ;
	uint64_t		file_line ;
	uint64_t		block_len ;
	char			*block_buf ;
	
	char			parse_buffer[ PARSE_BUFFER_SIZE + 1 ] ; /* ���������� */
	int			parse_buflen ;
} ;

funcLoadOutputPluginConfig LoadOutputPluginConfig ;
int LoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct OutputPluginContext	*p_plugin_ctx = NULL ;
	
	/* �����ڴ��Դ�Ų�������� */
	p_plugin_ctx = (struct OutputPluginContext *)malloc( sizeof(struct OutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
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
	/* ����ֽ�õ���־�� */
	NOTICELOGC( "send flush_flag[%d] filename[%s]line_no[%d] line[%d][%.*s]"
		, flush_send_buffer
		, p_plugin_ctx->filename , p_plugin_ctx->file_line+line_add
		, line_len , line_len , line )
	
	
	
	
	
	
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

