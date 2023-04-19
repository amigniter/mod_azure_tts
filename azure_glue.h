#ifndef __AZURE_GLUE_H__
#define __AZURE_GLUE_H__

switch_status_t azure_speech_load();
switch_status_t azure_speech_open();
switch_status_t azure_speech_feed_tts(switch_speech_handle_t *sh, char* text);

#endif