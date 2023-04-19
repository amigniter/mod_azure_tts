#ifndef __MOD_AZURE_TTS_H__
#define __MOD_AZURE_TTS_H__

#include <switch.h>

#define EVENT_JSON "mod_azure_tts::json"

typedef void (*responseHandler_t)(switch_speech_handle_t *sh, const char* eventName, char* json);

struct azure_data {
    char *voice_name;
    int rate;
    char *file;
    char *azure_key;
    char *azure_region;
    char *session_uuid_str;
    responseHandler_t responseHandler;
    switch_file_handle_t *fh;
};

typedef struct azure_data azure_t;

#endif