#pragma once
#include "common_defs.hpp"
#ifdef SSL_ON
namespace chai
{

inline ssl::context getSslServerContext(const std::string& sslPemFile,
                                        const std::string& privatekey,
                                        const char* trustStorePath)
{
    ssl::context sslContext(boost::asio::ssl::context::tls_server);
    sslContext.set_options(boost::asio::ssl::context::default_workarounds |
                           boost::asio::ssl::context::no_sslv2 |
                           boost::asio::ssl::context::no_sslv3 |
                           boost::asio::ssl::context::single_dh_use |
                           boost::asio::ssl::context::no_tlsv1 |
                           boost::asio::ssl::context::no_tlsv1_1);

    // BIG WARNING: This needs to stay disabled, as there will always be
    // unauthenticated endpoints
    // mSslContext->set_verify_mode(boost::asio::ssl::verify_peer);

    SSL_CTX_set_options(sslContext.native_handle(), SSL_OP_NO_RENEGOTIATION);

    sslContext.add_verify_path(trustStorePath);

    sslContext.use_certificate_file(sslPemFile, boost::asio::ssl::context::pem);
    sslContext.use_private_key_file(privatekey, boost::asio::ssl::context::pem);

    // Set up EC curves to auto (boost asio doesn't have a method for this)
    // There is a pull request to add this.  Once this is included in an asio
    // drop, use the right way
    // http://stackoverflow.com/questions/18929049/boost-asio-with-ecdsa-certificate-issue
    if (SSL_CTX_set_ecdh_auto(sslContext.native_handle(), 1) != 1)
    {
        // std::cout << "Error setting tmp ecdh list\n";
    }

    // Mozilla intermediate cipher suites v5.7
    // Sourced from: https://ssl-config.mozilla.org/guidelines/5.7.json
    const char* mozillaIntermediate = "ECDHE-ECDSA-AES128-GCM-SHA256:"
                                      "ECDHE-RSA-AES128-GCM-SHA256:"
                                      "ECDHE-ECDSA-AES256-GCM-SHA384:"
                                      "ECDHE-RSA-AES256-GCM-SHA384:"
                                      "ECDHE-ECDSA-CHACHA20-POLY1305:"
                                      "ECDHE-RSA-CHACHA20-POLY1305:"
                                      "DHE-RSA-AES128-GCM-SHA256:"
                                      "DHE-RSA-AES256-GCM-SHA384:"
                                      "DHE-RSA-CHACHA20-POLY1305";

    if (SSL_CTX_set_cipher_list(sslContext.native_handle(),
                                mozillaIntermediate) != 1)
    {
        // std::cout << "Error setting cipher list\n";
    }
    return sslContext;
}
inline ssl::context getSslServerContext(const std::string& sslPemFile,
                                        const char* trustStorePath)
{
    getSslServerContext(sslPemFile, sslPemFile, trustStorePath);
}
} // namespace chai
#endif
