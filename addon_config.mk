meta:
	ADDON_NAME = ofxRakNet
	ADDON_DESCRIPTION = RakNet networking for openFrameworks, built on MafiaNet (maintained RakNet / SLikeNet fork)
	ADDON_AUTHOR = Dan Rosser
	ADDON_TAGS = "networking" "raknet" "mafianet" "udp" "multiplayer"
	ADDON_URL = https://github.com/danoli3/ofxRakNet

common:
	# Only libs/mafianet/include: MafiaNet includes its headers as "mafianet/x.h",
	# and some of them (assert.h, string.h, time.h, alloca.h) would shadow the
	# system headers if libs/mafianet/include/mafianet were on the include path.
	ADDON_INCLUDES = libs/mafianet/include
	ADDON_INCLUDES += src
	ADDON_INCLUDES_EXCLUDE = libs/mafianet/include/mafianet
	ADDON_INCLUDES_EXCLUDE += libs/mafianet/include/mafianet/%

	# Windows: ws2_32 is linked from ofxRakNet.cpp (#pragma comment), MSYS2 links it already.
	# MafiaNet's voice chat (voice.cpp, needs Opus and RNNoise) isn't included.
	# No ADDON_DEFINES: projectGenerator 0.105 writes them into Xcode projects as
	# [[null], ...], which breaks the build.
