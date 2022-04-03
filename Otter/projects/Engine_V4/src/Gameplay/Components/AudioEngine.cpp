#include "AudioEngine.h"
#include "fmod_errors.h"
#include "fmod_studio_common.h"
#include "fmod_studio.hpp"
#include <iostream>
#include "ToneFire.h"
#include "fmod_studio_common.h"


ToneFire::FMODStudio AudioEngine::studio;
ToneFire::StudioSound AudioEngine::audio;

int AudioEngine::ErrorCheck(FMOD_RESULT result)
{
	if (result != FMOD_OK)
	{
		std::cout << "FMOD ERROR:" << FMOD_ErrorString(result) << std::endl;

#ifdef _DEBUG
		__debugbreak();
#endif // _DEBUG
		return 1;
	}
	return 0;
}

void AudioEngine::studioinit()
{


}

void AudioEngine::studioupdate()
{
	studio.Update();
}

void AudioEngine::shutdown()
{
	ErrorCheck(pSystem->close());
	ErrorCheck(pSystem->release());
}


void AudioEngine::loadSound(const std::string& soundName, const std::string& filename, bool b3d, bool bLooping, bool bStream)
{
	//checks if already loaded
	auto foundElement = sounds.find(soundName);
	if (foundElement != sounds.end())
	{
		return;
	}

	FMOD_MODE mode = FMOD_DEFAULT;
	mode |= (b3d) ? FMOD_3D : FMOD_2D;
	mode |= (bLooping) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
	mode |= (bStream) ? FMOD_CREATESTREAM : FMOD_CREATECOMPRESSEDSAMPLE;

	FMOD::Sound* LoadedSound;
	ErrorCheck(pSystem->createSound(filename.c_str(), mode, nullptr, &LoadedSound));

	if (LoadedSound != nullptr)
	{
		sounds[soundName] = LoadedSound;
	}
}



void AudioEngine::loadBanks()
{
	studio.LoadBank("Master.bank");
	studio.LoadBank("Master.strings.bank");
	studio.LoadBank("Background_Music.bank");
	studio.LoadBank("Enemy_Sounds.bank");
}

void AudioEngine::unloadSound(const std::string& soundName)
{
	auto foundElement = sounds.find(soundName);
	if (foundElement != sounds.end())
	{
		ErrorCheck(foundElement->second->release());
		sounds.erase(foundElement);
	}
}

void AudioEngine::loadEvents()
{
	audio.LoadEvent("event:/Daytime Song");
	audio.LoadEvent("event:/Goblin");
	audio.LoadEvent("event:/Growls");
	
}

void AudioEngine::playEvents(const std::string& eventname)
{
	audio.PlayEvent(eventname);
}

void AudioEngine::stopEvents(const std::string& eventname)
{
	audio.StopEvent(eventname);
}

void AudioEngine::EventPosChanges(const std::string& eventname, float x, float y, float z)
{
	audio.SetEventPosition(eventname, FMOD_VECTOR{ x, y, z });
}

void AudioEngine::EventParamChanges(const std::string& eventname, std::string& paramname, float x, float y)
{
	audio.SetEventParameter(eventname, paramname, x);
}

void AudioEngine::playSoundByName(const std::string& soundName)
{
	pSystem->playSound(sounds[soundName], nullptr, false, nullptr);
}