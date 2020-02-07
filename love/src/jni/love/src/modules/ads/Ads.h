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

#ifndef LOVE_ADS_H
#define LOVE_ADS_H

// LOVE
#include "common/config.h"
#include "common/Module.h"
#include "common/StringMap.h"

// stdlib
#include <string>

//SDL
#include <SDL_syswm.h>
#include "window/Window.h"

namespace love
{
namespace ads
{
		
class Ads : public Module
{
public:
			
	Ads();
	virtual ~Ads() {}
			
	// Implements Module.
	virtual ModuleType getModuleType() const { return M_ADS; }
			
	void test(void) const;
	
	void createBanner(const char *adID,const char *position);
	
	void hideBanner();
	
	void showBanner();
	
	bool isInterstitialLoaded();
	
	void requestInterstitial(const char *adID);
	
	void showInterstitial();
	
	bool isRewardedAdLoaded();
	
	void requestRewardedAd(const char *adID);
	
	void showRewardedAd();
	
	std::string getDeviceLanguage();
	
	//Private functions for callbacks
	bool coreInterstitialError();
	
	bool coreInterstitialClosed();
	
	bool coreRewardedAdError();
	
	bool coreRewardedAdDidFinish();
	
	std::string coreGetRewardType(); 
	
	double coreGetRewardQuantity();
	
	bool coreRewardedAdDidStop();
	
	void changeEUConsent();
	
private:
	bool hasBannerBeenCreated = false;
	
			
}; // Ads
	
} // ads
} // love

#endif // LOVE_ADS_H
