meta:
	ADDON_NAME = ofxRakNet
	ADDON_DESCRIPTION = RakNet networking for openFrameworks, built on SLikeNet (maintained RakNet 4 fork) with security fixes
	ADDON_AUTHOR = Dan Rosser
	ADDON_TAGS = "networking" "raknet" "slikenet" "udp" "multiplayer"
	ADDON_URL = https://github.com/danoli3/ofxRakNet

common:
	# Only libs/slikenet/include: SLikeNet includes its headers as "slikenet/x.h",
	# and some of them (assert.h, string.h, time.h, alloca.h) would shadow the
	# system headers if libs/slikenet/include/slikenet were on the include path.
	ADDON_INCLUDES = libs/slikenet/include
	ADDON_INCLUDES += src
	ADDON_INCLUDES_EXCLUDE = libs/slikenet/include/slikenet
	ADDON_INCLUDES_EXCLUDE += libs/slikenet/include/slikenet/%

	# Windows: ws2_32 is linked from ofxRakNet.cpp (#pragma comment), MSYS2 links it already.
	# No ADDON_DEFINES: projectGenerator 0.105 writes them into Xcode projects as
	# [[null], ...], which breaks the build.
