# mod_azure_tts

A FreeSWITCH module that allows Microsoft Azure API to be used as a tts provider.

### About
It integrates into the FreeSWITCH TTS interface and adds a new engine of **azure_tts** to the core. The new engine can be used with mod_dptools **speak** command as a native engine in the same way `speak` command is used, by setting `tts_engine` to *azure_tts* and `tts_voice` to one of the supported [Azure voices](https://learn.microsoft.com/en-us/azure/cognitive-services/speech-service/language-support?tabs=stt#text-to-speech). Text-to-speak can be a plain text or SSML.

## Installation

Module requires libfreeswitch-dev package and [AzureSpeechSDK](https://github.com/Azure-Samples/cognitive-services-speech-sdk/blob/master/quickstart/cpp/linux/from-microphone/README.md) for linux. Make sure to have `build-essential` and `cmake` installed as well.

On a FreeSWITCH running machine install `pkg-config` and `libfreeswitch-dev`:
```
sudo apt-get install build-essential cmake pkg-config libfreeswitch-dev
```

### Azure Speech SDK

SpeechSDK only needs to be extracted to a common location. We'll use that path later when building the module as AZURE_PATH cmake argument.
Install prerequisites for azure sdk:
```
sudo apt-get install libssl-dev libasound2 wget
```
Note that OpenSSL library (libssl-dev) should be version 1.x. The Speech SDK doesn't support OpenSSL ver 3.x

```
cd /usr/local
sudo mkdir -p SpeechSDK-Linux
sudo wget -O SpeechSDK-Linux.tar.gz https://aka.ms/csspeech/linuxbinary
sudo tar --strip 1 -xzf SpeechSDK-Linux.tar.gz -C SpeechSDK-Linux
sudo rm -r SpeechSDK-Linux.tar.gz
```

### Build
In mod_azure_tts folder
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DAZURE_PATH=/usr/local/SpeechSDK-Linux ..
make
sudo make install
```
*AZURE_PATH* cmake variable should point to the location where the SpeechSDK libraries are installed.

### Loading

Module can be loaded from **fs_cli**
```
load mod_azure_tts
```
or it can be added to *autoload_configs/modules.conf.xml* for automatic loading
```
<load module="mod_azure_tts"/>
```

## Usage

A dialplan example. The same principle applies when used with ESL (set channel variables and call application *speak*)
```
<action application="set" data="tts_engine=azure_tts"/>
<action application="set" data="tts_voice=en-US-JennyNeural"/>
```

Initialize TTS engine. Both variables are ***required***

 - `tts_engine` set to azure_tts
 - `tts_voice` preferred speech [voice](https://learn.microsoft.com/en-us/azure/cognitive-services/speech-service/language-support?tabs=stt#text-to-speech)

If SSML is being used, it allows setting the voice(s) from SSML and it has the priority. Nevertheless `tts_voice` is still required by the TTS interface and needs to be set.

```
<action application="speak" data="{AZURE_SUBSCRIPTION_KEY=theKey,AZURE_REGION=theRegion}this is a text to speak" />
```

Send the **speak** command (call an app) and pass the parameters to it, which is a text to speak.
In front of the text, within curly brackets, we send comma separated arguments:
- AZURE_SUBSCRIPTION_KEY your subsciption key
- AZURE_REGION region
- UUID (optional)
  - unique identifier of FreeSWITCH channel

If `UUID` is provided events will be sent for the particular channel, otherwise no events are fired.

### Events
An optional feature of this module is that it can generate associated events to an application.  The format of the JSON text frames and the associated events are described below.

##### Freeswitch event generated
**Name**: mod_azure_tts::json
**Body**: JSON string

##### JSON event on successful audio synthesizing
When audio is being successfully synthesized, an event will be launched and will look like this:
```json
{
  "result": "success",
  "details": [
    {
      "AudioData": 37046,
      "AudioDuration": 2312
    }
  ]
}
```
The result is `success` and the `details` an array:
- `AudioData` - (int) size in bytes
- `AudioDuration` - (int) duration in ms

##### JSON event on error response
An example of error event:
```
{
  "result": "error",
  "details": [
    {
      "ErrorCode": 1,
      "ErrorDetails": "WebSocket upgrade failed: Authentication error (401). Please check subscription information and region name. USP state: 2. Received audio size: 0 bytes.",
      "Reason": 1
    }
  ]
}
```
The result is `error` and the `details` an array:
- `ErrorCode` - (int)
- `Reason` - (int)
- `ErrorDetails` - (string) description of error response