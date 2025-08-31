//
//  WiiAudioEngine_Private.cpp
//  Wii audio engine
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioDefines.h>

#include "WiiAudioEngine.hpp"

//
// Handles volume changes.
//
IOReturn WiiAudioEngine::handleVolumeChange(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue) {
  if (newValue > kWiiMaxVolume) {
    newValue = kWiiMaxVolume;
  }
  if (newValue < kWiiMinVolume) {
    newValue = kWiiMinVolume;
  }

  _currentVolume = newValue;
  WIIDBGLOG("Volume changed to %d", _currentVolume);
  return kIOReturnSuccess;
}

//
// Handles mute changes.
//
IOReturn WiiAudioEngine::handleMuteChange(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue) {
  _currentMute = newValue;
  WIIDBGLOG("Mute changed to %d", _currentMute);
  return kIOReturnSuccess;
}

//
// Creates the volume logarithmic table for volume adjustments.
// Table borrowed from https://github.com/ekarlo/eqMac2/blob/master/eqMac2Driver/eqMac2DriverEngine.cpp.
//
void WiiAudioEngine::createVolumeLogTable(void) {
  _logTable[0]  = 1.0E-4;
	_logTable[1]  = 1.09749875E-4;
	_logTable[2]  = 1.2045036E-4;
	_logTable[3]  = 1.3219411E-4;
	_logTable[4]  = 1.4508287E-4;
	_logTable[5]  = 1.5922828E-4;
	_logTable[6]  = 1.7475284E-4;
	_logTable[7]  = 1.9179103E-4;
	_logTable[8]  = 2.1049041E-4;
	_logTable[9]  = 2.3101296E-4;
	_logTable[10] = 2.5353645E-4;
	_logTable[11] = 2.7825593E-4;
	_logTable[12] = 3.0538556E-4;
	_logTable[13] = 3.3516026E-4;
	_logTable[14] = 3.67838E-4;
	_logTable[15] = 4.0370174E-4;
	_logTable[16] = 4.4306213E-4;
	_logTable[17] = 4.8626016E-4;
	_logTable[18] = 5.336699E-4;
	_logTable[19] = 5.857021E-4;
	_logTable[20] = 6.4280734E-4;
	_logTable[21] = 7.054802E-4;
	_logTable[22] = 7.742637E-4;
	_logTable[23] = 8.4975344E-4;
	_logTable[24] = 9.326034E-4;
	_logTable[25] = 0.0010235311;
	_logTable[26] = 0.001123324;
	_logTable[27] = 0.0012328468;
	_logTable[28] = 0.0013530478;
	_logTable[29] = 0.0014849682;
	_logTable[30] = 0.0016297508;
	_logTable[31] = 0.0017886495;
	_logTable[32] = 0.0019630406;
	_logTable[33] = 0.0021544348;
	_logTable[34] = 0.0023644895;
	_logTable[35] = 0.0025950242;
	_logTable[36] = 0.002848036;
	_logTable[37] = 0.0031257158;
	_logTable[38] = 0.0034304692;
	_logTable[39] = 0.0037649358;
	_logTable[40] = 0.0041320124;
	_logTable[41] = 0.0045348783;
	_logTable[42] = 0.0049770237;
	_logTable[43] = 0.005462277;
	_logTable[44] = 0.0059948424;
	_logTable[45] = 0.006579332;
	_logTable[46] = 0.007220809;
	_logTable[47] = 0.007924829;
	_logTable[48] = 0.00869749;
	_logTable[49] = 0.009545485;
	_logTable[50] = 0.010476157;
	_logTable[51] = 0.01149757;
	_logTable[52] = 0.012618569;
	_logTable[53] = 0.013848864;
	_logTable[54] = 0.015199111;
	_logTable[55] = 0.016681006;
	_logTable[56] = 0.018307382;
	_logTable[57] = 0.02009233;
	_logTable[58] = 0.022051308;
	_logTable[59] = 0.024201283;
	_logTable[60] = 0.026560878;
	_logTable[61] = 0.02915053;
	_logTable[62] = 0.03199267;
	_logTable[63] = 0.03511192;
	_logTable[64] = 0.038535286;
	_logTable[65] = 0.042292427;
	_logTable[66] = 0.046415888;
	_logTable[67] = 0.05094138;
	_logTable[68] = 0.055908103;
	_logTable[69] = 0.061359074;
	_logTable[70] = 0.06734151;
	_logTable[71] = 0.07390722;
	_logTable[72] = 0.081113085;
	_logTable[73] = 0.08902151;
	_logTable[74] = 0.097701;
	_logTable[75] = 0.10722672;
	_logTable[76] = 0.1176812;
	_logTable[77] = 0.12915497;
	_logTable[78] = 0.14174742;
	_logTable[79] = 0.15556762;
	_logTable[80] = 0.17073527;
	_logTable[81] = 0.18738174;
	_logTable[82] = 0.20565122;
	_logTable[83] = 0.22570197;
	_logTable[84] = 0.24770764;
	_logTable[85] = 0.2718588;
	_logTable[86] = 0.29836473;
	_logTable[87] = 0.32745492;
	_logTable[88] = 0.35938138;
	_logTable[89] = 0.3944206;
	_logTable[90] = 0.43287614;
	_logTable[91] = 0.47508103;
	_logTable[92] = 0.5214008;
	_logTable[93] = 0.5722368;
	_logTable[94] = 0.62802917;
	_logTable[95] = 0.6892612;
	_logTable[96] = 0.75646335;
	_logTable[97] = 0.83021754;
	_logTable[98] = 0.91116273;
	_logTable[99] = 1.0;
}

//
// Creates audio controls for the engine.
//
IOReturn WiiAudioEngine::createControls(void) {
  IOAudioControl  *control;

  //
  // Create volume control.
  //
  control = IOAudioLevelControl::createVolumeControl(kWiiMaxVolume, kWiiMinVolume, kWiiMaxVolume, (-40 << 16) + (32768), 0,
                                                     kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll,
                                                     0, kIOAudioControlUsageOutput);
  if (control == NULL) {
    return kIOReturnNoMemory;
  }

  control->setValueChangeHandler(OSMemberFunctionCast(IOAudioControl::IntValueChangeHandler, this, &WiiAudioEngine::handleVolumeChange), this);
  addDefaultAudioControl(control);
  control->release();

  //
  // Create mute control.
  //
  control = IOAudioToggleControl::createMuteControl(false, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageOutput);
	if (control == NULL) {
    return kIOReturnNoMemory;
  }

	control->setValueChangeHandler(OSMemberFunctionCast(IOAudioControl::IntValueChangeHandler, this, &WiiAudioEngine::handleMuteChange), this);
	addDefaultAudioControl(control);
	control->release();

  return kIOReturnSuccess;
}
