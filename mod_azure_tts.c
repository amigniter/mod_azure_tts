/* 
 *
 * mod_azure_tts.c -- Microsoft Azure text to speech
 *
 */
#include "mod_azure_tts.h"
#include "azure_glue.h"

#include <unistd.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_azure_tts_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_azure_tts_shutdown);
SWITCH_MODULE_DEFINITION(mod_azure_tts, mod_azure_tts_load, mod_azure_tts_shutdown, NULL);

static void responseHandler(switch_speech_handle_t *sh, const char* eventName, char* json) {
    azure_t *azure = (azure_t *) sh->private_info;
    assert(azure != NULL);
    if(azure->session_uuid_str != NULL) {
        switch_event_t *event;
        switch_core_session_t *session = NULL;
        session = switch_core_session_locate(azure->session_uuid_str);
        if(!session)
            return;
        switch_channel_t *channel = switch_core_session_get_channel(session);
        if (json)
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
                              "responseHandler: sending event payload: %s.\n", json);
        switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, eventName);
        switch_channel_event_set_data(channel, event);
        if (json) switch_event_add_body(event, "%s", json);
        switch_event_fire(&event);
        switch_core_session_rwunlock(session);
    }
}

static switch_status_t speech_open(switch_speech_handle_t *sh, const char *voice_name, int rate, int channels, switch_speech_flag_t *flags)
{
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char outfile[512] = "";
	azure_t *azure = switch_core_alloc(sh->memory_pool, sizeof(*azure));

    azure->voice_name = switch_core_strdup(sh->memory_pool, voice_name);
    azure->rate = rate;

    azure->responseHandler = responseHandler;

	/* Construct temporary file name with a new UUID */
	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);
	switch_snprintf(outfile, sizeof(outfile), "%s%s%s.tmp.wav", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, uuid_str);
    azure->file = switch_core_strdup(sh->memory_pool, outfile);

    azure->fh = (switch_file_handle_t *) switch_core_alloc(sh->memory_pool, sizeof(switch_file_handle_t));

	sh->private_info = azure;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "speech_open - created file %s for name %s, rate %d\n",
                      azure->file, azure->voice_name, rate);

	return azure_speech_open();
}

static switch_status_t speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	azure_t *azure = (azure_t *) sh->private_info;
	assert(azure != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "speech_close - closing file %s\n", azure->file);
	if (switch_test_flag(azure->fh, SWITCH_FILE_OPEN)) {
		switch_core_file_close(azure->fh);
	}

	unlink(azure->file);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{
	azure_t *azure = (azure_t *) sh->private_info;
    assert(azure != NULL);

    if(!azure->azure_region || !azure->azure_key) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "speech_feed_tts: no authorization credentials provided!\n");
        return SWITCH_STATUS_FALSE;
    }

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "speech_feed_tts\n");

	if (SWITCH_STATUS_SUCCESS != azure_speech_feed_tts(sh, text)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_file_open(azure->fh, azure->file, 0,	//number_of_channels,
                              azure->rate,	//samples_per_second,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open file: %s\n", azure->file);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

static void speech_flush_tts(switch_speech_handle_t *sh)
{
    azure_t *azure = (azure_t *) sh->private_info;
	assert(azure != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "speech_flush_tts\n");

	if (azure->fh != NULL && azure->fh->file_interface != NULL) {
		switch_core_file_close(azure->fh);
	}
}

static switch_status_t speech_read_tts(switch_speech_handle_t *sh, void *data, size_t *datalen, switch_speech_flag_t *flags)
{
    azure_t *azure = (azure_t *) sh->private_info;
	size_t my_datalen = *datalen / 2;

	assert(azure != NULL);

	if (azure->fh->file_interface == (void *)0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "file [%s] has already been closed\n", azure->file);
		unlink(azure->file);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_file_read(azure->fh, data, &my_datalen) != SWITCH_STATUS_SUCCESS) {
		switch_core_file_close(azure->fh);
		unlink(azure->file);
		return SWITCH_STATUS_FALSE;
	}
	*datalen = my_datalen * 2;
	if (datalen == 0) {
		switch_core_file_close(azure->fh);
		unlink(azure->file);
		return SWITCH_STATUS_BREAK;
	}
	return SWITCH_STATUS_SUCCESS;

}

static void text_param_tts(switch_speech_handle_t *sh, char *param, const char *val)
{
    azure_t *azure = (azure_t *) sh->private_info;
    assert(azure != NULL);
    if(!strcmp(param, "AZURE_SUBSCRIPTION_KEY")) {
        azure->azure_key = switch_core_strdup(sh->memory_pool, val);
    }
    if(!strcmp(param, "AZURE_REGION")) {
        azure->azure_region = switch_core_strdup(sh->memory_pool, val);
    }
    if(!strcmp(param, "UUID") && val[0] != '\0') {
        azure->session_uuid_str= switch_core_strdup(sh->memory_pool, val);
    }
    /*
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PARAM (%s) is %s \n", param, val);
    */
}

static void numeric_param_tts(switch_speech_handle_t *sh, char *param, int val)
{
}

static void float_param_tts(switch_speech_handle_t *sh, char *param, double val)
{
}

SWITCH_MODULE_LOAD_FUNCTION(mod_azure_tts_load)
{
	switch_speech_interface_t *speech_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	speech_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SPEECH_INTERFACE);
	speech_interface->interface_name = "azure_tts";
	speech_interface->speech_open = speech_open;
	speech_interface->speech_close = speech_close;
	speech_interface->speech_feed_tts = speech_feed_tts;
	speech_interface->speech_read_tts = speech_read_tts;
	speech_interface->speech_flush_tts = speech_flush_tts;
	speech_interface->speech_text_param_tts = text_param_tts;
	speech_interface->speech_numeric_param_tts = numeric_param_tts;
	speech_interface->speech_float_param_tts = float_param_tts;

	return azure_speech_load();
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_azure_tts_shutdown)
{
	return SWITCH_STATUS_UNLOAD;
}
