/*
 * Copyright (C) 2011, 2025 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IPCUtilities.h"

#include <sys/socket.h>
#include <wtf/UniStdExtras.h>

namespace IPC {

#if OS(DARWIN)
// The default buffer sizes of AF_UNIX datagram sockets on Darwin (2048 bytes for sending
// and 4096 for receiving, see net.local.dgram.maxdgram) are too small for IPC messages,
// which are up to 4096 bytes without counting attachments: sendmsg() fails with EMSGSIZE
// and the message is dropped.
static constexpr int datagramSocketBufferSize = 256 * 1024;
#endif

SocketPair createPlatformConnection(int socketType, unsigned options)
{
    std::array<int, 2> sockets;

#if OS(DARWIN)
    // SOCK_SEQPACKET is defined on Darwin, but socketpair() does not support it for AF_UNIX
    // (it fails with EPROTONOSUPPORT). Fall back to SOCK_DGRAM like ConnectionUnix does.
    if (socketType == SOCK_SEQPACKET)
        socketType = SOCK_DGRAM;
#endif

#if OS(LINUX)
    if ((options & SetCloexecOnServer) || (options & SetCloexecOnClient)) {
        RELEASE_ASSERT(socketpair(AF_UNIX, socketType | SOCK_CLOEXEC, 0, sockets.data()) != -1);

        if (!(options & SetCloexecOnServer))
            RELEASE_ASSERT(unsetCloseOnExec(sockets[1]));
        if (!(options & SetCloexecOnClient))
            RELEASE_ASSERT(unsetCloseOnExec(sockets[0]));

        return { { sockets[0], UnixFileDescriptor::Adopt }, { sockets[1], UnixFileDescriptor::Adopt } };
    }
#endif

    RELEASE_ASSERT(socketpair(AF_UNIX, socketType, 0, sockets.data()) != -1);

#if OS(DARWIN)
    if (socketType == SOCK_DGRAM) {
        for (int socket : sockets) {
            RELEASE_ASSERT(!setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &datagramSocketBufferSize, sizeof(datagramSocketBufferSize)));
            RELEASE_ASSERT(!setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &datagramSocketBufferSize, sizeof(datagramSocketBufferSize)));
        }
    }
#endif

    if (options & SetCloexecOnServer)
        RELEASE_ASSERT(setCloseOnExec(sockets[1]));
    if (options & SetCloexecOnClient)
        RELEASE_ASSERT(setCloseOnExec(sockets[0]));

    return { { sockets[0], UnixFileDescriptor::Adopt }, { sockets[1], UnixFileDescriptor::Adopt } };
}

} // namespace IPC
