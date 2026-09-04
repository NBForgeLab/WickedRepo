// Unit tests for the wiNetwork module (wiNetwork.h / wiNetwork_Windows.cpp):
// real UDP loopback traffic, receive timeouts and socket lifecycle.
// Requires no GPU and no external network access.
#include "doctest.h"

#if defined(_WIN32) && !defined(PLATFORM_UWP)
#include "wiNetwork.h"

#include <cstring>
#include <string>

namespace
{
	constexpr uint16_t basePort = 53777;

	// A few ports in a private range, in case one is busy on the machine.
	bool bindReceiver(wiNetwork::Socket& receiver, uint16_t& boundPort)
	{
		for (uint16_t attempt = 0; attempt < 4; ++attempt)
		{
			const uint16_t port = static_cast<uint16_t>(basePort + attempt);
			if (wiNetwork::ListenPort(&receiver, port))
			{
				boundPort = port;
				return true;
			}
		}
		return false;
	}
}

TEST_CASE("UDP loopback delivers a packet from sender to receiver")
{
	wiNetwork::Socket receiver;
	wiNetwork::Socket sender;
	REQUIRE(wiNetwork::CreateSocket(&receiver));
	REQUIRE(wiNetwork::CreateSocket(&sender));

	uint16_t port = 0;
	REQUIRE(bindReceiver(receiver, port));

	const std::string payload = "gameguru max unit test";
	const wiNetwork::Connection destination{ { 127, 0, 0, 1 }, port };
	CHECK(wiNetwork::Send(&sender, &destination, payload.data(), payload.size()));

	// Wait up to 5 seconds for the datagram to arrive:
	CHECK(wiNetwork::CanReceive(&receiver, 5000000));

	wiNetwork::Connection from;
	char buffer[128] = {};
	REQUIRE(wiNetwork::Receive(&receiver, &from, buffer, payload.size()));
	CHECK(std::string(buffer, payload.size()) == payload);

	// The sender's loopback address and ephemeral port are reported back:
	CHECK(from.ipaddress[0] == 127);
	CHECK(from.ipaddress[1] == 0);
	CHECK(from.ipaddress[2] == 0);
	CHECK(from.ipaddress[3] == 1);
	CHECK(from.port != 0);
	CHECK(from.port != port); // sender uses its own ephemeral port

	CHECK(wiNetwork::Destroy(&receiver));
	CHECK(wiNetwork::Destroy(&sender));
}

TEST_CASE("CanReceive reports no data on an idle socket")
{
	wiNetwork::Socket receiver;
	REQUIRE(wiNetwork::CreateSocket(&receiver));

	uint16_t port = 0;
	REQUIRE(bindReceiver(receiver, port));

	// Brief timeout on a socket that received nothing:
	CHECK_FALSE(wiNetwork::CanReceive(&receiver, 50000));

	CHECK(wiNetwork::Destroy(&receiver));
}

TEST_CASE("Destroy invalidates the socket handle")
{
	wiNetwork::Socket sock;
	REQUIRE(wiNetwork::CreateSocket(&sock));
	REQUIRE(sock.IsValid());

	CHECK(wiNetwork::Destroy(&sock));
	CHECK_FALSE(sock.IsValid());
}

#endif // defined(_WIN32) && !defined(PLATFORM_UWP)
