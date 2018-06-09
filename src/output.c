/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

/* 激活一轮从输入插件读，写到所有输出插件流程处理 */
int WriteAllOutputPlugins( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , uint16_t filename_len , char *filename )
{
	struct LogpipeOutputPlugin	*p_travel_logpipe_output_plugin = NULL ;
	
	uint32_t			file_offset ;
	uint32_t			file_line ;
	char				block_buf[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ;
	uint32_t			block_len ;
	struct timeval			tv_begin ;
	struct timeval			tv_end ;
	struct timeval			tv_diff ;
	
	int				nret = 0 ;
	int				nret2 = 0 ;
	
	file_offset = 0 ;
	file_line = 0 ;
	
	/* 执行输入端读前函数 */
	INFOLOG( "[%s]->pfuncBeforeReadInputPlugin ..." , p_logpipe_input_plugin->so_filename )
	if( p_logpipe_input_plugin->pfuncBeforeReadInputPlugin )
	{
		gettimeofday( & tv_begin , NULL );
		nret = p_logpipe_input_plugin->pfuncBeforeReadInputPlugin( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & file_offset , & file_line ) ;
		gettimeofday( & tv_end , NULL );
		DiffTimeval( & tv_begin , & tv_end , & tv_diff );
		if( nret < 0 )
		{
			ERRORLOG( "[%s]->pfuncBeforeReadInputPlugin failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno )
			return -1;
		}
		else if( nret > 0 )
		{
			WARNLOG( "[%s]->pfuncBeforeReadInputPlugin failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno )
			goto _GOTO_AFTER_OUTPUT;
		}
		else
		{
			INFOLOG( "[%s]->pfuncBeforeReadInputPlugin ok , ELAPSE[%ld.%06ld]" , p_logpipe_input_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
		}
	}
	
	/* 执行所有输出端写前函数 */
	list_for_each_entry( p_travel_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		INFOLOG( "[%s]->pfuncBeforeWriteOutputPlugin ..." , p_travel_logpipe_output_plugin->so_filename )
		if( p_travel_logpipe_output_plugin->pfuncBeforeWriteOutputPlugin )
		{
			gettimeofday( & tv_begin , NULL );
			nret = p_travel_logpipe_output_plugin->pfuncBeforeWriteOutputPlugin( p_env , p_travel_logpipe_output_plugin , p_travel_logpipe_output_plugin->context , filename_len , filename ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret < 0 )
			{
				ERRORLOG( "[%s]->pfuncBeforeWriteOutputPlugin failed , errno[%d]" , p_travel_logpipe_output_plugin->so_filename , errno )
				return -1;
			}
			else if( nret > 0 )
			{
				WARNLOG( "[%s]->pfuncBeforeWriteOutputPlugin failed , errno[%d]" , p_travel_logpipe_output_plugin->so_filename , errno )
				goto _GOTO_AFTER_OUTPUT;
			}
			else
			{
				INFOLOG( "[%s]->pfuncBeforeWriteOutputPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_output_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
	}
	
	while(1)
	{
		/* 执行输入端读函数 */
		INFOLOG( "[%s]->pfuncReadInputPlugin ..." , p_logpipe_input_plugin->so_filename );
		memset( block_buf , 0x00 , sizeof(block_buf) );
		gettimeofday( & tv_begin , NULL );
		nret = p_logpipe_input_plugin->pfuncReadInputPlugin( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & file_offset , & file_line , & block_len , block_buf , sizeof(block_buf) ) ;
		gettimeofday( & tv_end , NULL );
		DiffTimeval( & tv_begin , & tv_end , & tv_diff );
		if( nret == LOGPIPE_READ_END_OF_INPUT )
		{
			INFOLOG( "[%s]->pfuncReadInputPlugin done" , p_logpipe_input_plugin->so_filename )
			nret = 0 ;
			break;
		}
		else if( nret < 0 )
		{
			ERRORLOG( "[%s]->pfuncReadInputPlugin failed[%d]" , p_logpipe_input_plugin->so_filename , nret )
			return -1;
		}
		else if( nret > 0 )
		{
			WARNLOG( "[%s]->pfuncReadInputPlugin return[%d]" , p_logpipe_input_plugin->so_filename , nret )
			goto _GOTO_AFTER_OUTPUT;
		}
		else
		{
			INFOLOG( "[%s]->pfuncReadInputPlugin ok , ELAPSE[%ld.%06ld]" , p_logpipe_input_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
		}
		
		/* 执行所有输出端写函数 */
		list_for_each_entry( p_travel_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
		{
			INFOLOG( "[%s]->pfuncWriteOutputPlugin ..." , p_travel_logpipe_output_plugin->so_filename )
			gettimeofday( & tv_begin , NULL );
			nret = p_travel_logpipe_output_plugin->pfuncWriteOutputPlugin( p_env , p_travel_logpipe_output_plugin , p_travel_logpipe_output_plugin->context , file_offset , file_line , block_len , block_buf ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret < 0 )
			{
				ERRORLOG( "[%s]->pfuncWriteOutputPlugin failed[%d]" , p_travel_logpipe_output_plugin->so_filename , nret )
				return -1;
			}
			else if( nret > 0 )
			{
				WARNLOG( "[%s]->pfuncWriteOutputPlugin return[%d]" , p_travel_logpipe_output_plugin->so_filename , nret );
				goto _GOTO_AFTER_OUTPUT;
			}
			else
			{
				INFOLOG( "[%s]->pfuncWriteOutputPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_output_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
	}
	
_GOTO_AFTER_OUTPUT :
	
	/* 执行输入端读后函数 */
	INFOLOG( "[%s]->pfuncAfterReadInputPlugin ..." , p_logpipe_input_plugin->so_filename )
	if( p_logpipe_input_plugin->pfuncAfterReadInputPlugin )
	{
		gettimeofday( & tv_begin , NULL );
		nret2 = p_logpipe_input_plugin->pfuncAfterReadInputPlugin( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & file_offset , & file_line ) ;
		gettimeofday( & tv_end , NULL );
		DiffTimeval( & tv_begin , & tv_end , & tv_diff );
		if( nret2 < 0 )
		{
			ERRORLOG( "[%s]->pfuncAfterReadInputPlugin failed[%d]" , p_logpipe_input_plugin->so_filename , nret2 )
		}
		else if( nret2 > 0 )
		{
			WARNLOG( "[%s]->pfuncAfterReadInputPlugin return[%d]" , p_logpipe_input_plugin->so_filename , nret2 )
		}
		else
		{
			INFOLOG( "[%s]->pfuncAfterReadInputPlugin ok , ELAPSE[%ld.%06ld]" , p_logpipe_input_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
		}
	}
	
	/* 执行所有输出端写后函数 */
	list_for_each_entry( p_travel_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		INFOLOG( "[%s]->pfuncAfterWriteOutputPlugin ..." , p_travel_logpipe_output_plugin->so_filename )
		if( p_travel_logpipe_output_plugin->pfuncAfterWriteOutputPlugin )
		{
			gettimeofday( & tv_begin , NULL );
			nret2 = p_travel_logpipe_output_plugin->pfuncAfterWriteOutputPlugin( p_env , p_travel_logpipe_output_plugin , p_travel_logpipe_output_plugin->context , filename_len , filename ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret2 < 0 )
			{
				ERRORLOG( "[%s]->pfuncAfterWriteOutputPlugin failed[%d]" , p_travel_logpipe_output_plugin->so_filename , nret2 )
			}
			else if( nret2 > 0 )
			{
				WARNLOG( "[%s]->pfuncAfterWriteOutputPlugin return[%d]" , p_travel_logpipe_output_plugin->so_filename , nret2 )
			}
			else
			{
				INFOLOG( "[%s]->pfuncAfterWriteOutputPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_output_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
	}
	
	return nret;
}

