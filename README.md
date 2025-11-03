## AntiRecoil Pico
AntiRecoil and AutoClicker Script running on embedded c using USB host/device implementation using PIO of raspberry pi pico (RP2040). Only works on the provided Pico SDK and ToolChain Versions. 
Either you can take USB_DEVICE_AND_KEYBOARD.uf2 from the build folder and paste it into raspberry pi pico on Bootsel mode or you can make your own build in vscode using Raspberry pi pico extension.

## Versions

|Pico SDK|2.2.0|
|-|-|
|ToolChain|14_2_Rel1|
|PicoTool|2.1.1|
|Cmake|3.31.5|
|Ninja|1.12.1|

## GPIO

|VCC|VBUS|
|-|-|
|D-|GP 1|
|D+|GP 0|
|Ground|GND|
