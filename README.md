# MillicastPublisher plugin 1.7.0 for Unreal Engine

* Supported on Windows, Linux, Mac and Android

This plugin enable to publish game audio and video content to Millicast.
You can configure your credentials and configure your game logic using unreal object, capture and publish from virtual camera.

## Supported Unreal Engine

The Unreal publisher supports Unreal engine 4.27, 5.0.3, 5.1, 5.2, 5.3

## Support codec

### Audio

* Opus (software encoding)

### Video

* VP8 (software encoding, simulcast available)
* VP9 (software encoding)
* H264 (hardware encoding, simulcast available)

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

### Installation steps for Android

Installs needed for Android:

Java JDK

* for UE 5.3.1 and above: [JDK 11](https://www.oracle.com/eg/java/technologies/javase/jdk11-archive-downloads.html)
* for UE 4.27 and below: [JDK 8](https://www.oracle.com/eg/java/technologies/javase/javase8-archive-downloads.html)

### SDK

* Install [Android Studio](https://developer.android.com/studio)
* Open the Android Studio and go to More ``Actions dropdown`` and select ``SDK Manager``
* go to SDK Tools Tab and mark checkboxes: 33.0.0, 32.0.0, 31.0.0, 30.0.3, 29.0.2
* go to: C:\Path\to\Android\Sdk\build-tools (You have Android SDK Location path at the top of the settings)
* go trough each folder and change file name ``d8`` to ``dx``
### NDK

* For UE less than 5.1: [Android NDK r21d](https://dl.google.com/android/repository/android-ndk-r21d-windows-x86_64.zip)
* For UE 5.1 and above: [Android NDK r25b](https://dl.google.com/android/repository/android-ndk-r25b-windows.zip)

After extraction, you will have to copy the ``ndk`` folder path and set it in Unreal Engine Project Settings

### Setup the Environment Variables

* ``JAVA_HOME`` 		: C:\Program Files\Java\jdk1.8.0_202
* ``ANDROID_HOME`` 		: C:\Path\to\Android\Sdk
* ``ANDROID_SDK_HOME`` 	: C:\Path\to\Android\Sdk
* ``NDK_ROOT`` 			: C:\Path\To\Android\Sdk\ndk\android-ndk-r25b
* ``NDKROOT`` 			: C:\Path\To\Android\Sdk\ndk\android-ndk-r25b

### Unreal Engine Project Settings

Navigate to the Unreal Engine and open Project Settings, go to the Android SDK tab and set the paths.

* Location of Android SDK (the directory that usually contains 'android-sdk-'):``C:/Path/To/Android/Sdk``
* Location of Android NDK (the directory that usually contains 'android-ndk-'):	``C:/Path/To/Sdk/ndk/android-ndk-r25b``
* Location of JAVA (the directory usually contains 'jdk')					  :	``C:/Path/To/Java/jdk1.8.0_202/jre``
* SDK API Level(specific version, 'latest' or matchndk'- see tooltip)		  :	``matchndk``
* NDK API Level(specific version of latest -see tooltip)					  :	``android-31``

Next go to the Android tab inside Project Settings and set the following:

* in the APK Packaging ``Press Configure`` Now and Press Accept SDK button and if button greyed out that mean it accepted before.
* in the Android Package Name type com.yourcompanyname.yourprojectname
* in the Minimum SDK Version type 21 and make sure install SDK 21 in Android Studio.
* in the Target SDK Version open Android Studio then SDK Manager and type latest version of sdk
* in Install Location choose ``Auto``
* check box Package game data inside .apk
* check box Allow large OBB files
* check box Allow patch OBB files
* check box Use ExternalFilesDir for UE4Game files
* in the App Bundles check box for Generate bundle (AAB)
* in the Build check box for support arm64

in the Destribution Signing you must have ``key.keystore`` file and to make this file follow steps:

* run Command Prompt as administrator
* run: cd C:\Path\To\Java\jdk1.8.0_202\bin
* run: ``keytool -genkey -v -keystore key.keystore -alias Mykey -keyalg RSA -keysize 2048 -validity 10000``
    * enter any password
	* repeat password
	* type ``firstandlastname``
	* type ``orgunit``
	* type ``org``
	* type ``city``
	* type ``state``
	* type ``12345`` for country code
	* type ``yes``
	* enter password again
	* repeat password

The Key will be now created, go to C:\Path\To\Java\jdk1.8.0_202\bin folder to get the file copy the ``key.keystore`` file and paste it in ``ProjectRootFolder/Build/Android/``

Back at the Project settings in the Unreal Engine editor inside ``Distribution Signing``:

* Key Strore (output of keytool, placed in <Project>/Build/Android): key.keystore
* Key Alias (-alias parameter to keytool): Mykey
* Key Store  Password(-storepass parameter to keytool)
* Key Password(leave blank to use key store password)

### Please note:

While using Plugin for Android:

* Support for OpenGL is not ready yet. Uncheck the ``Support OpenGL ES3.2`` and check ``Support Vulkan`` in the ``Platforms - Android`` section inside Project Settings.
* Disable simulicast on ``MillicastPublisher`` component in your blueprint.

## Documentation

You can find the documentation for the plugin here: [https://docs.millicast.com/docs/millicast-publisher-plugin](https://docs.millicast.com/docs/millicast-publisher-plugin)
