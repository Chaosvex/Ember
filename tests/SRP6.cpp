/*
 * Copyright (c) 2014 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>
#include <srp6/Server.h>
#include <srp6/Client.h>
#include <srp6/Generator.h>
#include <botan/bigint.h>
#include <array>
#include <memory>
#include <string>
#include <string_view>

using namespace ember;

class srp6SessionTest : public ::testing::Test {
public:
	virtual void SetUp() {
		identifier_ = "CHAOSVEX";
		password_ = "ABC";
		gen_ = std::make_unique<srp6::Generator>(srp6::Generator::Group::_256_BIT);
		srp6::generate_salt(salt_);
		verifier_ = srp6::generate_verifier(identifier_, password_, *gen_, salt_, srp6::Compliance::GAME);
		server_ = std::make_unique<srp6::Server>(*gen_, verifier_);
		client_ = std::make_unique<srp6::Client>(identifier_, password_, *gen_);
	}

	virtual void TearDown() {}

	std::string identifier_, password_;
	Botan::BigInt verifier_;
	std::array<std::uint8_t, 32> salt_{};
	std::unique_ptr<srp6::Generator> gen_;
	std::unique_ptr<srp6::Server> server_;
	std::unique_ptr<srp6::Client> client_;
};

TEST(srp6a, RFC5054_TestVectors) {
	std::string identifier { "alice" };
	std::string password { "password123" };
	const auto salt = Botan::BigInt::encode(Botan::BigInt(("0xBEB25379D1A8581EB5A727673A2441EE")));
	srp6::Generator gen(srp6::Generator::Group::_1024_BIT);
	
	Botan::BigInt expected_k("0x7556AA045AEF2CDD07ABAF0F665C3E818913186F");
	Botan::BigInt k = srp6::detail::compute_k(gen.generator(), gen.prime());
	ASSERT_EQ(expected_k, k) << "K was calculated incorrectly!";

	Botan::BigInt expected_x("0x94B7555AABE9127CC58CCF4993DB6CF84D16C124");
	Botan::BigInt x = srp6::detail::compute_x(identifier, password, salt, srp6::Compliance::RFC5054);
	ASSERT_EQ(expected_x, x) << "x was calculated incorrectly!";

	Botan::BigInt expected_v("0x7E273DE8696FFC4F4E337D05B4B375BEB0DDE1569E8FA00A9886D8129BADA1F1822"
	                         "223CA1A605B530E379BA4729FDC59F105B4787E5186F5C671085A1447B52A48CF1970"
	                         "B4FB6F8400BBF4CEBFBB168152E08AB5EA53D15C1AFF87B2B9DA6E04E058AD51CC72B"
	                         "FC9033B564E26480D78E955A5E29E7AB245DB2BE315E2099AFB");
	Botan::BigInt v = srp6::generate_verifier(identifier, password, gen, salt, srp6::Compliance::RFC5054);
	ASSERT_EQ(expected_v, v) << "v was calculated incorrectly!";

	Botan::BigInt test_a("0x60975527035CF2AD1989806F0407210BC81EDC04E2762A56AFD529DDDA2D4393");
	Botan::BigInt test_b("0xE487CB59D31AC550471E81F00F6928E01DDA08E974A004F49E61F5D105284D20");

	srp6::Client client(identifier, password, gen, test_a, true);
	srp6::Server server(gen, v, test_b, true);

	Botan::BigInt expected_A("0x61D5E490F6F1B79547B0704C436F523DD0E560F0C64115BB72557EC4"
	                         "4352E8903211C04692272D8B2D1A5358A2CF1B6E0BFCF99F921530EC"
	                         "8E39356179EAE45E42BA92AEACED825171E1E8B9AF6D9C03E1327F44"
	                         "BE087EF06530E69F66615261EEF54073CA11CF5858F0EDFDFE15EFEA"
	                         "B349EF5D76988A3672FAC47B0769447B");
	ASSERT_EQ(expected_A, client.public_ephemeral())
		<< "Client's public ephemeral did not match expected value!";

	Botan::BigInt expected_B("0xBD0C61512C692C0CB6D041FA01BB152D4916A1E77AF46AE105393011"
	                         "BAF38964DC46A0670DD125B95A981652236F99D9B681CBF87837EC99"
	                         "6C6DA04453728610D0C6DDB58B318885D7D82C7F8DEB75CE7BD4FBAA"
	                         "37089E6F9C6059F388838E7A00030B331EB76840910440B1B27AAEAE"
	                         "EB4012B7D7665238A8E3FB004B117B58");
	ASSERT_EQ(expected_B, server.public_ephemeral())
		<< "Server's public ephemeral did not match expected value!";

	Botan::BigInt expected_u("0xCE38B9593487DA98554ED47D70A7AE5F462EF019");
	Botan::BigInt u = srp6::detail::scrambler(expected_A, expected_B, gen.prime().bytes(),
	                                         srp6::Compliance::RFC5054);
	ASSERT_EQ(expected_u, u) << "Scrambling parameter did not match";

	Botan::BigInt expected_key("0xB0DC82BABCF30674AE450C0287745E7990A3381F63B387AAF271A10D"
	                           "233861E359B48220F7C4693C9AE12B0A6F67809F0876E2D013800D6C"
	                           "41BB59B6D5979B5C00A172B4A2A5903A0BDCAF8A709585EB2AFAFA8F"
	                           "3499B200210DCC1F10EB33943CD67FC88A2F39A4BE5BEC4EC0A3212D"
	                           "C346D7E474B29EDE8A469FFECA686E5A");

	const auto& c_sess_key = client.session_key(expected_B, salt, srp6::Compliance::RFC5054).t;
	const auto& s_sess_key = server.session_key(expected_A, srp6::Compliance::RFC5054).t;

	EXPECT_EQ(expected_key, Botan::BigInt::decode(c_sess_key.data(), c_sess_key.size()))
		<< "Client key did not match expected value!";
	EXPECT_EQ(expected_key, Botan::BigInt::decode(s_sess_key.data(), s_sess_key.size()))
		<< "Server key did not match expected value!";
}

TEST_F(srp6SessionTest, SelfAuthentication) {
	Botan::BigInt A = client_->public_ephemeral();
	Botan::BigInt B = server_->public_ephemeral();

	srp6::SessionKey s_key = server_->session_key(A);
	srp6::SessionKey c_key = client_->session_key(B, salt_);

	Botan::BigInt c_proof = client_->generate_proof(c_key, B, salt_);
	Botan::BigInt s_proof = server_->generate_proof(s_key, A, c_proof);

	Botan::BigInt expected_c_proof = srp6::generate_client_proof(identifier_, s_key, gen_->prime(),
	                                                            gen_->generator(), A, B, salt_);
	Botan::BigInt expected_s_proof = srp6::generate_server_proof(A, c_proof, c_key, gen_->prime().bytes());

	EXPECT_EQ(expected_c_proof, c_proof) << "Server could not verify client proof!";
	EXPECT_EQ(expected_s_proof, s_proof) << "Client could not verify server proof!";
}

/* 
 * Simulates an actual authentication session by seeding the server with
 * the parameters that were used for an actual successful login
 */
TEST_F(srp6SessionTest, GameAuthentication) {
	// Server's secret value, client's public value, client proof, server proof
	Botan::BigInt b("18593985542940560649451045851874319089347482848983190581196134045699448046190");
	Botan::BigInt A("59852229564408135463856204462249479723343699701058170755060257585995770179058");
	Botan::BigInt M1("1198251478626595859038225880380336340559256984824");
	Botan::BigInt M2("859932068100996518188190846072995264590638975226");

	// User values from the database
	Botan::BigInt salt("0xF4C7DBCA7138DA48D9B7BE55C0C76B1145AF67340CF7A6718D452A563E12A19C");
	Botan::BigInt verifier("0x37A75AE5BCF38899C75D28688C78434CB690657B5D8D77463668B83D0062A186");

	// Start server
	srp6::Generator gen(srp6::Generator::Group::_256_BIT);
	srp6::Server server(gen, verifier, b);

	srp6::SessionKey key = server.session_key(A);
	Botan::BigInt M1_S = srp6::generate_client_proof("CHAOSVEX", key, gen.prime(), gen.generator(), A,
	                                                server.public_ephemeral(), Botan::BigInt::encode(salt));
	Botan::BigInt M2_S = server.generate_proof(key, A, M1);

	EXPECT_EQ(M1, M1_S) << "Server's calculated client proof did not match the replayed proof!";
	EXPECT_EQ(M2, M2_S) << "Server's proof did not match the replayed proof!";
}

TEST_F(srp6SessionTest, ServerZeroEphemeral) {
	EXPECT_THROW(server_->session_key(0), srp6::exception)
		<< "Public ephemeral key should never be zero!";
}

TEST_F(srp6SessionTest, ServerNegativeEphemeral) {
	EXPECT_THROW(server_->session_key(Botan::BigInt("-10")), srp6::exception)
		<< "Public ephemeral key should never be negative!";
}

TEST_F(srp6SessionTest, ClientZeroEphemeral) {
	EXPECT_THROW(client_->session_key(0, salt_), srp6::exception)
		<< "Public ephemeral key should never be zero!";
}

TEST_F(srp6SessionTest, ClientNegativeEphemeral) {
	EXPECT_THROW(client_->session_key(Botan::BigInt("-10"), salt_), srp6::exception)
		<< "Public ephemeral key should never be negative!";
}

TEST(srp6Regressions, SaltZeroPad_ComputeX) {
	std::string_view username { "alice" };
	std::string_view password { "password123" };
	Botan::BigInt salt("0xBEB25379D1A8581EB5A727673A2441EE");
	srp6::Generator gen(srp6::Generator::Group::_1024_BIT);

	Botan::BigInt expected_x("0x7E5250F2CB894FD9703611318C387A773FD52C09");
	Botan::BigInt x = srp6::detail::compute_x(username, password, Botan::BigInt::encode(salt), srp6::Compliance::GAME);
	ASSERT_EQ(expected_x, x) << "x was calculated incorrectly!";
}

TEST(srp6Regressions, SaltZeroPad_GenerateUser) {
	std::string_view username { "alice" };
	std::string_view password { "password123" };
	Botan::BigInt salt("0xBEB25379D1A8581EB5A727673A2441EE");

	auto gen = srp6::Generator(srp6::Generator::Group::_256_BIT);
	auto verifier = srp6::generate_verifier(username, password, gen, Botan::BigInt::encode(salt), srp6::Compliance::GAME);
	
	Botan::BigInt expected_v("0x399CF53C149F220F4AA88F7F2F6CA9CB6E4C44EA5240AC0F65601F392F32A16A");
	ASSERT_EQ(expected_v, verifier) << "Verifier was calculated incorrectly!";
}

TEST(srp6Regressions, NPad_GenerateClientProof) {
	const Botan::BigInt g(7);
	const Botan::BigInt prime("0x894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
	const Botan::BigInt a("0x52DFA6644066547BD7360AD2A23AE91DB544FADB8F4DCA86B4184481102E4089");
	const Botan::BigInt b("0x809C1BC78BDB3873D286FDADF38D1524348C9CA5AB63E7793EF6A7944C5A8D");
	Botan::BigInt session_val("0x42C6518D6F338C050717427B18F7C6B6131C968B0CFC20C43AAAD61625F286DA55E24BF6A2CBDC79");
	srp6::KeyType kt(session_val.bytes(), boost::container::default_init);
	session_val.binary_encode(kt.data(), kt.size());
	const srp6::SessionKey key(std::move(kt));

	const std::array<std::uint8_t, 32> salt {
		0x40, 0x1A, 0x08, 0x7D, 0x89, 0x73, 0x9D, 0xD9, 0xE4, 0x2F, 0x1E, 0x7E, 0x41, 0x65, 0xFD, 0xA4,
		0x21, 0x41, 0xF4, 0xFD, 0x4A, 0xD3, 0x2D, 0x03, 0xC1, 0xF2, 0x07, 0x66, 0x88, 0x06, 0xE5, 0x41
	};

	const auto c_proof = srp6::generate_client_proof("TEST", key, prime, g, a, b, salt);
	const Botan::BigInt expected_cproof("0xF9C97B36A797001F7D31CC0EB3E741B8B216B564");

	ASSERT_EQ(expected_cproof, c_proof) << "Client proof was calculated incorrectly!";
}

TEST(srp6Regressions, SPad_VerifyKey) {
	const srp6::Generator gen(srp6::Generator::Group::_256_BIT);
	const Botan::BigInt v("0x570B18E774242FAC149DB63458E8BA7C67C8CCD18F8C1B2779848703523AF502");
	const Botan::BigInt c_pub_key("0x3AD9948BCEE582A3BFCEABC895B22FB3F0208E5D444D07CAA580CE24B1DEFC70"); // A
	const Botan::BigInt s_priv_key("0x3865DD04A190926F04B241820503B53F8BF21B2C161FB2FE038B662943936A53"); // b
	srp6::Server server(gen, v, s_priv_key);
	const auto key = server.session_key(c_pub_key);
	const Botan::BigInt sbytes(key.t.data(), key.t.size());
	const Botan::BigInt correct_key("0xEE57F5996D4EEDFFDE38EE79492AB4A5E57CD25C3CE98B035D4BA9A7E05D56C0DAF0F30D9797C216");
	EXPECT_EQ(correct_key, sbytes) << "Computed key incorrectly";
}