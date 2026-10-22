/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  RakNet License.txt file in the licenses directory of this source tree. An additional grant 
 *  of patent rights can be found in the RakNet Patents.txt file in the same directory.
 *
 */

// USER EDITABLE FILE

// ofxRakNet: openFrameworks ships OpenSSL on every platform, so TCPInterface,
// HTTPConnection(2) and Rackspace can use TLS (TCPInterface::StartSSLClient).
#ifndef OPEN_SSL_CLIENT_SUPPORT
#define OPEN_SSL_CLIENT_SUPPORT 1
#endif
