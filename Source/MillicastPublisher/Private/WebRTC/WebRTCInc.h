// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START

#if PLATFORM_WINDOWS

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"

// C4582: constructor is not implicitly called in "api/rtcerror.h", treated as an error by UnrealEngine
// C6319: Use of the comma-operator in a tested expression causes the left argument to be ignored when it has no side-effects.
// C6323: Use of arithmetic operator on Boolean type(s).
#pragma warning(push)
#pragma warning(disable: 4582 4583 6319 6323)

#endif // PLATFORM_WINDOWS

#include "api/rtp_receiver_interface.h"
#include "api/media_types.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/create_peerconnection_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_encoder_factory_template.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_sink_interface.h"

#include "media/base/adapted_video_track_source.h"

#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/video_capture/video_capture.h"

#include "pc/session_description.h"
#include "pc/video_track_source.h"

#include "rtc_base/thread.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_adapter.h"

// because WebRTC uses STL
#include <string>
#include <memory>

#if PLATFORM_WINDOWS
#pragma warning(pop)

#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"

#else

#ifdef PF_MAX
#undef PF_MAX
#endif

#endif //PLATFORM_WINDOWS

THIRD_PARTY_INCLUDES_END