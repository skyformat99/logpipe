/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_LOGPIPE_IN_
#define _H_LOGPIPE_IN_

#ifdef __cplusplus
extern "C" {
#endif

#include "logpipe_api.h"

#include "list.h"
#include "rbtree.h"

#include "LOGC.h"

extern char *__LOGPIPE_VERSION ;

/* ������ýṹ */
struct LogpipePluginConfigItem
{
	char			*key ;
	char			*value ;
	
	struct list_head	this_node ;
} ;

/* ������� */
#define LOGPIPE_PLUGIN_TYPE_INPUT	'I'
#define LOGPIPE_PLUGIN_TYPE_OUTPUT	'O'

/* ��������ṹ�������Ȳ�ѯ�������ʱʹ�� */
struct LogpipePlugin
{
	unsigned char			type ; /* ������� */
} ;

/* �����������ṹ */
struct LogpipeInputPlugin
{
	unsigned char			type ; /* ������� */
	
	struct LogpipePluginConfigItem	plugin_config_items ; /* �Զ������ò��� */
	
	char				so_filename[ PATH_MAX + 1 ] ; /* ����ļ��� */
	char				so_path_filename[ PATH_MAX + 1 ] ; /* ���·���ļ��� */
	void				*so_handler ; /* ����򿪾�� */
	funcLoadInputPluginConfig	*pfuncLoadInputPluginConfig ; /* ���������ļ�ʱװ�ز�� */
	funcInitInputPluginContext	*pfuncInitInputPluginContext ; /* ������ѭ��ǰ��ʼ����� */
	funcOnInputPluginIdle		*pfuncOnInputPluginIdle ; /* ����ʱ�ĵ��� */
	funcOnInputPluginEvent		*pfuncOnInputPluginEvent ; /* ���������¼�ʱ�ĵ��� */
	funcBeforeReadInputPlugin	*pfuncBeforeReadInputPlugin ; /* ��ȡһ�����ݿ�ǰ */
	funcReadInputPlugin		*pfuncReadInputPlugin ; /* ��ȡһ�����ݿ� */
	funcAfterReadInputPlugin	*pfuncAfterReadInputPlugin ; /* ��ȡһ�����ݿ�� */
	funcCleanInputPluginContext	*pfuncCleanInputPluginContext ; /* ������ѭ���������� */
	funcUnloadInputPluginConfig	*pfuncUnloadInputPluginConfig ; /* �˳�ǰж�ز�� */
	int				fd ;
	void				*context ; /* ���ʵ�������� */
	
	struct list_head		this_node ;
} ;

/* �����������ṹ */
struct LogpipeOutputPlugin
{
	unsigned char			type ; /* ������� */
	
	struct LogpipePluginConfigItem	plugin_config_items ; /* �Զ������ò��� */
	
	char				so_filename[ PATH_MAX + 1 ] ; /* ����ļ��� */
	char				so_path_filename[ PATH_MAX + 1 ] ; /* ���·���ļ��� */
	void				*so_handler ; /* ����򿪾�� */
	funcLoadOutputPluginConfig	*pfuncLoadOutputPluginConfig ; /* ���������ļ�ʱװ�ز�� */
	funcInitOutputPluginContext	*pfuncInitOutputPluginContext ; /* ������ѭ��ǰ��ʼ����� */
	funcOnOutputPluginIdle		*pfuncOnOutputPluginIdle ; /* ����ʱ�ĵ��� */
	funcOnOutputPluginEvent		*pfuncOnOutputPluginEvent ; /* �����������¼�ʱ���� */
	funcBeforeWriteOutputPlugin	*pfuncBeforeWriteOutputPlugin ; /* ��д��һ�����ݿ�ǰ */
	funcWriteOutputPlugin		*pfuncWriteOutputPlugin ; /* д��һ�����ݿ� */
	funcAfterWriteOutputPlugin	*pfuncAfterWriteOutputPlugin ; /* ��д��һ�����ݿ�� */
	funcCleanOutputPluginContext	*pfuncCleanOutputPluginContext ; /* ������ѭ���������� */
	funcUnloadOutputPluginConfig	*pfuncUnloadOutputPluginConfig ; /* �˳�ǰж�ز�� */
	int				fd ;
	void				*context ; /* ���ʵ�������� */
	
	struct list_head		this_node ;
} ;

/* �����ṹ */
struct LogpipeEnv
{
	char				config_path_filename[ PATH_MAX + 1 ] ; /* �����ļ��� */
	int				no_daemon ; /* �Ƿ�ת��Ϊ�ػ��������� */
	
	char				log_file[ PATH_MAX + 1 ] ; /* logpipe��־�ļ��� */
	int				log_level ; /* logpipe��־�ȼ� */
	struct LogpipePluginConfigItem	start_once_for_plugin_config_items ; /* ����ʱֻ������һ�ε����ã�������ʱ�������д�����־ */
	
	char				hostname[ HOST_NAME_MAX + 1 ] ;
	struct passwd			*pwd ;
	
#if 0 /* ��ϧinotify��֧�������ܵ� */
	char				logpipe_fifo_path_filename[ PATH_MAX + 1 ] ; /* �ڲ�״̬��������ܵ�·���ļ��� */
	int				logpipe_fifo_inotify_fd ; /* �ڲ�״̬��������ܵ��ļ�����¼������� */
	int				logpipe_fifo_inotify_wd ; /* �ڲ�״̬��������ܵ��ļ�����¼���������� */
#endif
	
	int				epoll_fd ; /* epoll�¼����� */
	
	struct LogpipeInputPlugin	logpipe_input_plugins_list ; /* ���������� */
	struct LogpipeOutputPlugin	logpipe_output_plugins_list ; /* ���������� */
	struct LogpipeInputPlugin	*p_block_input_plugin ; /* ����epoll�¼����߶�����ĳһ�������Ķ���ģʽ��ָ�������������ڵ� */
	unsigned char			idle_processing_flag ;
	
	int				quit_pipe[2] ; /* ���ӽ�������ܵ� */
} ;

/* �������� */
int WriteEntireFile( char *pathfilename , char *file_content , int file_len );
char *StrdupEntireFile( char *pathfilename , int *p_file_len );
int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv , int close_flag );

/* ������ú��� */
int AddPluginConfigItem( struct LogpipePluginConfigItem *config , char *key , int key_len , char *value , int value_len );
int DuplicatePluginConfigItems( struct LogpipePluginConfigItem *dst , struct LogpipePluginConfigItem *src );
void RemovePluginConfigItemsFrom( struct LogpipePluginConfigItem *config , struct LogpipePluginConfigItem *from );
void RemoveAllPluginConfigItems( struct LogpipePluginConfigItem *config );

/* ���� */
int LoadConfig( struct LogpipeEnv *p_env );
void UnloadConfig( struct LogpipeEnv *p_env );
void UnloadInputPluginSession( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin );
void UnloadOutputPluginSession( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin );

/* ���� */
int InitEnvironment( struct LogpipeEnv *p_env );
void CleanEnvironment( struct LogpipeEnv *p_env );

/* �ڲ�״̬����ܵ� */
int CreateLogpipeFifo( struct LogpipeEnv *p_env );
int ProcessLogpipeFifoEvents( struct LogpipeEnv *p_env );

/* ������� */
int monitor( struct LogpipeEnv *p_env );
int _monitor( void *pv );

/* �������� */
int worker( struct LogpipeEnv *p_env );

/* �����¼����� */
int ProcessOnIdle( struct LogpipeEnv *p_env );

#ifdef __cplusplus
}
#endif

#endif

