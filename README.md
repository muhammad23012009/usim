# USim

USim is an Ubuntu Touch app to manage eSIMs.

## Features
USim support the following features:

* Installing profiles with a QR code or by entering the string manually
* Enabling/disabling profiles
* Switching profiles
* Formatting eUICC memory
* Advanced eSIM profile information

## Usage
Using the app is as simple as installing it, doing operations on the eUICC and finally, restarting ofono with the restart icon in the top bar. Restarting ofono is required before closing the app to resume cellular, otherwise you will not be able to use the eSIM.

### TODO
* Implement a better way to show a busy dialog to user with states and errors (current method is too brittle)
* Eventually modify ofono to use it for logical channels instead of using gbinder
* An icon (duh)
* Improve the UI overall (it's quite bland)
* Clean up error handling in LpacWorker
* and more I forgot