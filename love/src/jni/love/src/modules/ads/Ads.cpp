/**
 * Created by bio1712 for love2d
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

// LOVE
#include "common/config.h"
#include "Ads.h"

#if defined(LOVE_MACOSX)
#include <CoreServices/CoreServices.h>
#elif defined(LOVE_IOS)
#include "common/ios.h"
#elif defined(LOVE_LINUX) || defined(LOVE_ANDROID)
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#elif defined(LOVE_WINDOWS)
#include "common/utf8.h"
#include <shlobj.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif
#if defined(LOVE_ANDROID)
#include "common/android.h"
#elif defined(LOVE_LINUX)
#include <spawn.h>
#endif


namespace love
{
namespace ads
{
	
		
Ads::Ads()
{
	
}
	
void Ads::test() const {
	printf("ADS_TEST\n");
}

void Ads::createBanner(const char *adID, const char *position) {
	if (hasBannerBeenCreated)
	{
		printf("Banner has already been created!");
		return;
	}
	
	love::android::createBanner(adID, position);
	hasBannerBeenCreated = true;
}
	

void Ads::hideBanner()
{
	love::android::hideBanner();
}
	
void Ads::showBanner()
{
	love::android::showBanner();
}
	
void Ads::requestInterstitial(const char *adID) {
	
	love::android::requestInterstitial(adID);
}
	
void Ads::showInterstitial()
{
	love::android::showInterstitial();
}

bool Ads::isInterstitialLoaded()
{
	return love::android::isInterstitialLoaded();
}
	
void Ads::requestRewardedAd(const char *adID) {
	
	love::android::requestRewardedAd(adID);
}
	
bool Ads::isRewardedAdLoaded()
{
	return love::android::isRewardedAdLoaded();
		
}
	
void Ads::showRewardedAd()
{
	love::android::showRewardedAd();
}

std::string Ads::getDeviceLanguage()
{
	return love::android::getDeviceLanguage();
}
	
//Private functions for callbacks
	
bool Ads::coreInterstitialError()
{ //Interstitial has failed to load
	return love::android::coreInterstitialError();
}
	
bool Ads::coreInterstitialClosed()
{ //Interstitial has been closed by user
	return love::android::coreInterstitialClosed();
}
	
bool Ads::coreRewardedAdError()
{ //Video has failed to load
	return love::android::coreRewardedAdError();
}
	
bool Ads::coreRewardedAdDidFinish()
{ //Video has finished playing
	return love::android::coreRewardedAdDidFinish();
}
	
std::string Ads::coreGetRewardType()
{ //Get reward type
	return love::android::coreGetRewardType();
}
	
double Ads::coreGetRewardQuantity()
{ //Get reward qty
	return love::android::coreGetRewardQuantity();
}
	
bool Ads::coreRewardedAdDidStop()
{ //Ad stopped by user
	return love::android::coreRewardedAdDidStop();
}

void Ads::changeEUConsent()
{ //Change EU consent
	return love::android::changeEUConsent();
}
		
} // ads
} // love
