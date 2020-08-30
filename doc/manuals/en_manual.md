# StormCandle User Manual
Thank you for ~~buying~~ building the StormCandle.

To help you enjoy your StormCandle, please read the following document carefully and follow its instructions.

## Understanding your StormCandle

Your StormCandle will show you the current air pressure as an intuitive color gradient.

It "remembers" the highest and lowes pressures it has ever experienced. At pressures in the centre between these values, it will glow warm white. 
At higher pressures, more and more red light is added, at lower pressures blue light. Since air pressure changes with altitude, 
you should perform a reset after any elevation change of more than one floor.

**Note:** If you operate the device in a roughly airtight environment such as a well-sealed house or appartment, 
you may accidentially change the air pressure when operating a strong range hood or other outward-facing ventilator. 
This may accidentially set a minimal pressure value far beneath the natural range and thus distort the displayed result. In this case, please perform a reset as explained below.

## Setup
To use the device, place it in a convenient location where you can see it and connect the USB plug to any USB power source (wall charger / USB power bank / computer). 
After first setup, you should perform a reset as described below.

### Resetting min / max values

#### Revision 2 (with reset button)
To perform a reset, first disconnect the device from power. Hold down the reset button (you may need to remove a cover at the bottom of the device to acces the button). While holding, connect the device to power and wait. 

#### Revision 2b (no reset button)
To perform a reset, first disconnect the device from power. Turn over the device and connect the device to power.

![Rev2b reset process](rev2b_reset.png?raw=true "Rev2b reset process")

The device will blink three times red (1 second) and once green (2 seconds). 
At the end of the green light, the device will be reset and start flickering white, blue and red.

## Technical information
Power consumption: ca. 50 mA @ 5 V

Source code: https://github.com/BitScout/StormCandleRev2
