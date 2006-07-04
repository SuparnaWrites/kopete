#include "libeva.h"
#include "md5.h"
#include "crypt.h"
#include <arpa/inet.h>

namespace Eva {
	static const char login_16_51 [] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x29, 0xc0, 0xf8, 0xc4, 0xbe, 
		0x3b, 0xee, 0x57, 0x92, 0xd2, 0x42, 0xa6, 0xbe, 
		0x41, 0x98, 0x97, 0xb4 };
	static const char login_53_68 []= {
		0xce, 0x11, 0xd5, 0xd9, 0x97, 0x46, 0xac, 0x41, 
		0xa5, 0x01, 0xb2, 0xf5, 0xe9, 0x62, 0x8e, 0x07 };

	static const char login_94_193 []= {
		0x01, 0x40, 0x01, 0xb6, 0xfb, 0x54, 0x6e, 0x00, 
		0x10, 0x33, 0x11, 0xa3, 0xab, 0x86, 0x86, 0xff,
		0x5b, 0x90, 0x5c, 0x74, 0x5d, 0xf1, 0x47, 0xbf,
		0xcf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00 };
	static const char init_key[] = {
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };



	ByteArray header( int id, short const command, short const sequence )
	{
		// CODE DEBT: udp does not have the lenght placeholder!
		ByteArray data(13);
		data += '\0';
		data += '\0';
		data += Head;
		data += htons(Version);
		data += htons(command);
		data += htons(sequence);
		data += htonl(id);

		return data;
	}

	void setLength( ByteArray& data )
	{
		data.copyAt(0, htons(data.size()) );
	}

	int rand(void)
	{
		return 0xdead;
	}
	
	ByteArray doMd5( const ByteArray& text )
	{
		ByteArray code( Md5KeyLength );
		md5_state_t ctx;
		md5_init( &ctx );
		md5_append( &ctx, (md5_byte_t*) text.data(), text.size() );
		md5_finish( &ctx, (md5_byte_t*)code.data() );
		code.setSize( Md5KeyLength ); 
		return code;
	}

	inline void encrypt64( unsigned char* plain, unsigned char* plain_pre, 
			unsigned char* key, unsigned char* crypted, unsigned char* crypted_pre, 
			bool& isHeader )
	{
		int i;
		for( i = 0; i< 8; i++ )
			plain[i] ^= isHeader ? plain_pre[i] : crypted_pre[i];

		TEA::encipher( (unsigned int*) plain, (unsigned int*) key, 
				(unsigned int*) crypted );

		for( i = 0; i< 8; i++ )
			crypted[i] ^= plain_pre[i];

		memcpy( plain_pre, plain, 8 );
		isHeader = false;
	}

	
	// Interface for application
	// Utilities
	ByteArray loginToken( char const* buffer ) 
	{
		int length = buffer[1];
		length -= 3; // length - 1(head) - 1(length) - 1(tail) 
		ByteArray data(length);

		if( buffer[0] != LoginTokenOK )
			return data;
		data.append( buffer+2, length );
		return data;
	}

	ByteArray QQHash( const ByteArray& text )
	{
		return doMd5( doMd5( text ) );
	}

	ByteArray encrypt( const ByteArray& text, const ByteArray& key )
	{

		unsigned char 
			plain[8],         /* plain text buffer*/
			plain_pre[8],   /* plain text buffer, previous 8 bytes*/
			crypted[8],        /* crypted text*/
			crypted_pre[8];  /* crypted test, previous 8 bytes*/
	
		int pos, len, i;
		bool isHeader = true;      /* header is one byte*/
		ByteArray encoded( text.size() + 32 );
		
		pos = ( text.size() + 10 ) % 8;
		if( pos )
			pos = 8 - pos;

		// Prepare the first 8 bytes:
		plain[0] = ( rand() & 0xf8 ) | pos;
		memset( plain_pre, 0, 8 );
		memset( plain+1, rand()& 0xff, pos++ );

		// pad at most 2 bytes
		len = min( 8-pos, 2 );
		for( i = 0; i< len; i++ )
			plain[ pos++ ] = rand() & 0xff;

		for( i = 0; i< text.size(); i++ )
		{
			if( pos == 8 )
			{
				encrypt64( plain, plain_pre, (unsigned char*)key.data(), crypted, crypted_pre, isHeader );
				pos = 0;
				encoded.append( (char*)crypted, 8 );
				memcpy( crypted_pre, crypted, 8 );
			}
			else
				plain[pos++] = text.data()[i];
		}

		for( i = 0; i< 7; i++ )
		{
			if( pos == 8 )
			{
				encrypt64( plain, plain_pre, (unsigned char*)key.data(), crypted, crypted_pre, isHeader );
				encoded.append( (char*)crypted, 8 );
				break;
			}
			else
				plain[pos++] = 0;
		}

		return encoded;
	}

	// Core functions
	ByteArray requestLoginToken( int id, short const sequence )
	{
		ByteArray data(16);
		data += header(id, RequestLoginToken, sequence);
		data += '\0';
		data += Tail;
		setLength( data );

		return data;
	}

	ByteArray login( int id, short const sequence, const ByteArray& key, 
			const ByteArray& token, char const loginMode )
	{
		ByteArray login(LoginLength);
		ByteArray data(MaxPacketLength);
		ByteArray initKey( (char*)init_key, 16 );

		ByteArray nil(0);
		login += encrypt( nil, key );
		login.append( login_16_51, 36 );
		login += loginMode;
		login.append( login_53_68, 16 );
		login += (char) (token.size());
		login += token;
		login.append( login_94_193, 100 );
		memset( login.data()+login.size(), 0, login.capacity()-login.size() );

		data += header( id, Login, sequence );
		data += initKey;
		data += encrypt( login, initKey );
		data += Tail;
		setLength( data );

		initKey.release(); // static data, no need to free

		return data;
	}

		
		
}
		
		

