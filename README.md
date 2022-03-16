# MillicastPublisher plugin for Unreal Engine 4

* Supported UE version 4.27
* Supported on Windows and Linux

This plugin enable to publish game audio and video content to Millicast.
You can configure your credentials and configure your game logic using unreal object, capture and publish from virtual camera.

## Installation

You can install the plugin from the source code.
Follow these steps : 

* create a project with the UE editor.
* Close the editor
* Go at the root of your project folder (C:\Users\User\Unreal Engine\MyProject)
* Create a new directory "Plugins" and move into it
* Clone the MillicastPublisher repository : ``git clone https://github.com/millicast/millicast-publisher-unreal-engine-plugin.git MillicastPublisher``
* Open your project with UE

It will prompt you, saying if you want to re-build MillicastPublisher plugin, say yes.
You are now in the editor and can build your game using MillicastPublisher.

Note: After you package your game, it is possible that you will get an error when launching the game :  

> "Plugin MillicastPublisher could not be load because module MillicastPublisher has not been found"

And then the game fails to launch.
That is because Unreal has excluded the plugin.
If that is the case, create an empty C++ class in your project. This will force Unreal to include the plugin. Then, re-package the game, launch it, and it should be fixed.

## Documentation

You can find the documentation for the plugin here: [https://docs.millicast.com/docs/millicast-publisher-plugin](https://docs.millicast.com/docs/millicast-publisher-plugin)
