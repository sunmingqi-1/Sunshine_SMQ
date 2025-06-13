#include "mic_in.h"
#include <array>
#include <opus/opus.h>
#include <portaudio.h>  // 也可以改成 WASAPI 手写
#include <winsock2.h>
#include <ws2tcpip.h>
// #pragma comment( lib, "ws2_32.lib" )
#ifdef _MSC_VER  // 仅 MSVC 需要
#pragma comment( lib, "ws2_32.lib" )
#endif
static int pa_index_by_name( const char* needle )
{
    int n = Pa_GetDeviceCount();
    for ( int i = 0; i < n; ++i )
    {
        const PaDeviceInfo* inf = Pa_GetDeviceInfo( i );
        if ( inf && strstr( inf->name, needle ) && inf->maxOutputChannels >= 1 )
            return i;
    }
    return paNoDevice;
}

int mic_in::start( ctx_t& ctx, uint16_t port, const char* dev )
{
    if ( ctx.th.joinable() )
        return -1;
    ctx.stop = false;
    ctx.th   = std::thread( [ =, &ctx ] {
        // --- Winsock init ---
        WSADATA wsa;
        WSAStartup( MAKEWORD( 2, 2 ), &wsa );
        SOCKET      s = socket( AF_INET, SOCK_DGRAM, 0 );
        sockaddr_in addr{ AF_INET, htons( port ), INADDR_ANY };
        bind( s, ( sockaddr* )&addr, sizeof addr );

        // --- Opus decoder ---
        int                               err;
        OpusDecoder*                      dec = opus_decoder_create( 48000, 1, &err );
        std::array< unsigned char, 1500 > buf;
        std::array< short, 960 >          pcm;

        // --- PortAudio sink ---
        Pa_Initialize();
        PaStream*          stream   = nullptr;
        int                devIndex = pa_index_by_name( dev ? dev : "CABLE Input" );
        PaStreamParameters out;
        memset( &out, 0, sizeof out );
        out.device           = devIndex;
        out.channelCount     = 1;
        out.sampleFormat     = paInt16;
        out.suggestedLatency = 0.02;
        Pa_OpenStream( &stream, nullptr, &out, 48000, 960, paNoFlag, nullptr, nullptr );
        Pa_StartStream( stream );

        while ( !ctx.stop.load() )
        {
            int len = recv( s, ( char* )buf.data(), buf.size(), 0 );
            if ( len <= 0 )
                continue;
            int sam = opus_decode( dec, buf.data(), len, pcm.data(), 960, 0 );
            if ( sam > 0 )
                Pa_WriteStream( stream, pcm.data(), sam );
        }
        // --- cleanup ---
        Pa_StopStream( stream );
        Pa_CloseStream( stream );
        Pa_Terminate();
        opus_decoder_destroy( dec );
        closesocket( s );
        WSACleanup();
    } );
    return 0;
}
void mic_in::stop( ctx_t& ctx )
{
    if ( !ctx.th.joinable() )
        return;
    ctx.stop = true;
    ctx.th.join();
}