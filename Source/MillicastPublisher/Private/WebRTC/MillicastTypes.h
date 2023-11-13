// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "VideoEncoderInput.h"

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
#define FVideoEncoderInputFrameType AVEncoder::FVideoEncoderInputFrame*
#define InputFrameRef InputFrame
#else
#define FVideoEncoderInputFrameType TSharedPtr<AVEncoder::FVideoEncoderInputFrame>
#define InputFrameRef InputFrame.Get()
#endif
