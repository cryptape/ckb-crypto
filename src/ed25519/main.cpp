

#include <botan/hex.h>
//#include <iostream>
#include <botan/ed25519.h>


int main()
{
    // Generate Ed255119 private key
    //Botan::AutoSeeded_RNG rng;

    std::vector<uint8_t> m_public;
    //Botan::secure_vector<uint8_t> m_private;

    //const Botan::secure_vector<uint8_t> seed = rng.random_vec(32);
    m_public.resize(32);
    //m_private.resize(64);
    //Botan::ed25519_gen_keypair(m_public.data(), m_private.data(), seed.data());

    // sign msg
    std::string text("This is a tasty burger!");
    std::vector<uint8_t> m_msg(text.data(),text.data()+text.length());

    //Botan::secure_vector<uint8_t> sig(64);
    std::vector<uint8_t> sig(64);


    // Sign the message
    //Botan::ed25519_sign(sig.data(),m_msg.data(),m_msg.size(),m_private.data(), nullptr,0);

    /*
    for (int i = 0; i < sig.size(); i++) {
        printf("0x%02x,", sig[i]);
    }
    printf("\n\n");

    for (int j = 0; j < m_public.size(); j++) {
        printf("0x%02x,",m_public[j]);
    }
    */

    uint8_t sig_arr[] =
            {0xdf,0x1c,0x33,0x3a,0x10,0xa3,0x7b,0xf6,0x4b,0x25,0xbf,0x08,0x60,0x9f,0x97,0xc0,0xdf,0x48,0x8d,0x46,
             0xd9,0x64,0xfd,0xc3,0x77,0x8a,0xc0,0x84,0xfb,0xab,0xfd,0x04,0x88,0x8f,0xcf,0x59,0x3d,0xad,0x24,0xa7,
             0x74,0x49,0x18,0xb6,0x32,0xe9,0xb4,0x71,0x84,0x9b,0xec,0x54,0x21,0x74,0xec,0x2c,0x6d,0xc7,0x0c,0x76,
             0x3f,0xe6,0xf9,0x0d};

    uint8_t pub_arr[] =
            {0xdf,0x97,0xf1,0x52,0x96,0x85,0x50,0xfc,0xd8,0x6e,0x04,0x36,0x31,0x46,0xb1,0xd8,0xb9,0x05,0x17,0x27,
             0x37,0x32,0x00,0x16,0xff,0xe1,0xc6,0x74,0x90,0x50,0x92,0x07};

    for (int i = 0; i < sig.size(); ++i) {
        sig[i] = sig_arr[i];
    }

    for (int j = 0; j < m_public.size(); ++j) {
        m_public[j] = pub_arr[j];
    }

    bool judge = Botan::ed25519_verify(m_msg.data(),m_msg.size(),sig.data(),m_public.data(),nullptr,0);
    //std::cout << judge << std::endl;
    if (judge)
    {
        return 0;
    }
    return -1;
}





