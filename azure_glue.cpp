#include <switch.h>
#include <string>
#include <map>

#include "mod_azure_tts.h"
#include <speechapi_cxx.h>

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Audio;
static cJSON* createJsonEvent(const std::string& result, std::map<std::string,std::string>& details);

extern "C" {
	switch_status_t azure_speech_load() {
        return SWITCH_STATUS_SUCCESS;
	}

	switch_status_t azure_speech_open() {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_status_t azure_speech_feed_tts(switch_speech_handle_t *sh, char* text) {
        auto *azure = (azure_t *) sh->private_info;

        std::shared_ptr<SpeechConfig> speechConfig;

        try {
            speechConfig = SpeechConfig::FromSubscription(azure->azure_key, azure->azure_region);
        }
        catch(std::runtime_error& e)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                              "azure_speech_feed_tts: %s\n", e.what());
            return SWITCH_STATUS_FALSE;
        }

        // The language of the voice that speaks.
        speechConfig->SetSpeechSynthesisVoiceName(azure->voice_name);
        speechConfig->SetSpeechSynthesisOutputFormat(SpeechSynthesisOutputFormat::Riff8Khz16BitMonoPcm); //Riff8Khz16BitMonoPcm(default)  Raw8Khz16BitMonoPcm


        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                          "azure_speech_feed_tts: voice-name: %s\n",
                          azure->voice_name);

        auto speechSynthesizer = SpeechSynthesizer::FromConfig(speechConfig);

        std::shared_ptr<SpeechSynthesisResult> result;

        if (strstr(text, "<speak") == text) {
            result = speechSynthesizer->SpeakSsmlAsync(text).get();
        } else {
            result = speechSynthesizer->SpeakTextAsync(text).get();
        }

        // Checks result.
        if (result->Reason == ResultReason::SynthesizingAudioCompleted)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                              "azure_speech_feed_tts: SynthesizingAudioCompleted\n");

            auto audioDuration = std::chrono::duration_cast<std::chrono::milliseconds>(result->AudioDuration).count();

            std::map<std::string,std::string> successResponse;
            successResponse.insert(std::pair<std::string, std::string>("AudioData", std::to_string(result->GetAudioData()->size())) );
            successResponse.insert(std::pair<std::string, std::string>("AudioDuration", std::to_string(audioDuration)) );

            cJSON* jsonData = createJsonEvent("success", successResponse);
            if(jsonData) {
                char* jsonString = cJSON_PrintUnformatted(jsonData);
                azure->responseHandler(sh, EVENT_JSON, jsonString);
                free(jsonString);
                cJSON_Delete(jsonData);
            }

            auto stream = AudioDataStream::FromResult(result);
            stream->SaveToWavFileAsync(azure->file).get();

            return SWITCH_STATUS_SUCCESS;
        }
        else if (result->Reason == ResultReason::Canceled)
        {
            auto cancellation = SpeechSynthesisCancellationDetails::FromResult(result);

            std::map<std::string,std::string> errorResponse;
            errorResponse.insert(std::pair<std::string, std::string>("Reason", std::to_string((int)cancellation->Reason) ) );
            errorResponse.insert(std::pair<std::string, std::string>("ErrorCode", std::to_string((int)cancellation->ErrorCode) ) );
            errorResponse.insert(std::pair<std::string, std::string>("ErrorDetails", cancellation->ErrorDetails ) );

            cJSON* jsonData = createJsonEvent("error", errorResponse);
            if(jsonData) {
                char* jsonString = cJSON_PrintUnformatted(jsonData);
                azure->responseHandler(sh, EVENT_JSON, jsonString);
                free(jsonString);
                cJSON_Delete(jsonData);
            }

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                              "azure_speech_feed_tts: Error SynthesizingAudio\n");
        }

		return SWITCH_STATUS_FALSE;
	}
}

cJSON* createJsonEvent(const std::string& result, std::map<std::string,std::string>& details) {
    cJSON* json;
    cJSON* resultJson;
    cJSON* detailsJson;
    cJSON* detail;
    json = cJSON_CreateObject();

    resultJson = cJSON_CreateString(result.c_str());
    cJSON_AddItemToObject(json, "result", resultJson);

    detailsJson = cJSON_CreateArray();
    cJSON_AddItemToObject(json, "details", detailsJson);

    if(result == "success") {
        detail = cJSON_CreateObject();
        cJSON_AddItemToArray(detailsJson, detail);

        std::map<std::string,std::string>::iterator it;
        for (it = details.begin(); it != details.end(); ++it) {
            if(it->first == "AudioData") {
                cJSON* size = cJSON_CreateNumber( stoi(it->second) );
                cJSON_AddItemToObject(detail, "AudioData", size);
            } else if(it->first == "AudioDuration") {
                cJSON* duration = cJSON_CreateNumber( stoi(it->second) );
                cJSON_AddItemToObject(detail, "AudioDuration", duration);
            }
        }
    } else if (result == "error") {
        detail = cJSON_CreateObject();
        cJSON_AddItemToArray(detailsJson, detail);
        std::map<std::string,std::string>::iterator it;
        for (it = details.begin(); it != details.end(); ++it) {
            if(it->first == "Reason") {
                cJSON* reason = cJSON_CreateNumber( stoi(it->second) );
                cJSON_AddItemToObject(detail, "Reason", reason);
            } else if(it->first == "ErrorCode") {
                cJSON* errorCode = cJSON_CreateNumber( stoi(it->second) );
                cJSON_AddItemToObject(detail, "ErrorCode", errorCode);
            } else if(it->first == "ErrorDetails") {
                cJSON* errorDetails = cJSON_CreateString( (it->second).c_str() );
                cJSON_AddItemToObject(detail, "ErrorDetails", errorDetails);
            }
        }
    }
    return json;
}
