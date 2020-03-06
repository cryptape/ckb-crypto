//
// Created by Maellys on 2020/3/5.
//

#include <ctime>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "bignum.h"
#include "sha256.h"


#define MAXPRIMECOUNT 1000 // max value for count
unsigned int nSmallPrimes[MAXPRIMECOUNT][2]; // С��������
unsigned int nPrimeCount = 0; // Ѱ�������Ĵ���
CBigNumString strMod, strPubKey, strPriKey; // ��ʼ�������ַ�����
// Ѱ��С����
void MakeSmallPrimes()
{
    unsigned int n;
    unsigned int j;
    nPrimeCount = 3;


    // ���ȶ��弸��С����
    nSmallPrimes[0][0] = 2;
    nSmallPrimes[1][0] = 3;
    nSmallPrimes[2][0] = 5;
    nSmallPrimes[0][1] = 4;
    nSmallPrimes[1][1] = 9;
    nSmallPrimes[2][1] = 25;
    // ��ʼѰ������
    for (n=7; nPrimeCount < MAXPRIMECOUNT; n+=2)
    {
        for (j=0; nSmallPrimes[j][1] < n; j++)
        {
            if (j>= nPrimeCount) // ������ڵ�ǰ�������ţ��򷵻�
            {
                return;
            }
            if (n % nSmallPrimes[j][0]==0)  // ����������������˳�ѭ��
            {
                break;
            }
        }
        // �ҵ���������������
        if (nSmallPrimes[j][1] > n)
        {
            nSmallPrimes[nPrimeCount][0] = n;
            nSmallPrimes[nPrimeCount++][1] = n*n;
        }
    }
}

// �����������
CBigNum GenerateBigRandomNumber(unsigned short nBytes)
{
    CBigNum Result=0U; // ��ʼ������
    int i;
    clock_t ctStart;
    unsigned long ctr=0;

    // ����ʱ����
    clock_t ctInterval = CLOCKS_PER_SEC / 50 + 1;

    for (i=0; i<nBytes*2; i++)
    {
        ctStart = clock();
        // �ȵ�����ʱ�����ٿ�ʼ
        while (clock() - ctStart < ctInterval)
            ctr++;

        ctr = (ctr % 33) & 0xF;

        Result <<= 4U; // ��������4λ
        Result |= ctr; // ��������
    }
    putchar('\n');
    return Result; // ���ش���
}

CBigNum FindABigPrime(unsigned short nBytes)
{
    CBigNum nBig, nBig2;
    DWORD j;
    DWORD nTestCount = 0;
    DWORD nLehmanCount = 0;
    clock_t ctStartTime = clock(); // ��¼��ʼʱ��
    DWORD nOffset=0;
    bool bPrime=false; // ������־λ

    // ��ʼѰ�Ҷ�Ӧλ���Ĵ�����
    for (nBig = GenerateBigRandomNumber(nBytes) | 1U; !bPrime; nBig+=2U, nOffset+=2)
    {
        nTestCount++;
        for (j=0; j<nPrimeCount; j++)
        {
            // ������Ǵ����������˳�ѭ��
            if (nBig % nSmallPrimes[j][0] == 0)
            {
                break;
            }
        }

        if (j<nPrimeCount)
            continue;
        nLehmanCount++;
        nBig2 = (nBig - 1U) / 2U;
        // ����һЩ���������
        DWORD arnLehmanPrimes[] = { 89, 5179, 25981, 25439, 25013, 25667, 27397 };
        // ��ʼ��������
        CBigNum LehmanResults[sizeof(arnLehmanPrimes) / sizeof(arnLehmanPrimes[0])];
        nBig2 = nBig - 1U;
        bPrime = true;
        for (j=0; j<sizeof(arnLehmanPrimes) / sizeof(arnLehmanPrimes[0]); j++)
        {
            // ��ʼ���ɴ�����
            LehmanResults[j] =
                    CBigNum(arnLehmanPrimes[j]).PowMod(nBig2, nBig, CLOCKS_PER_SEC);
            if (LehmanResults[j] == nBig2)
            {
            }
            else if (LehmanResults[j] == 1U)
            {
            }
            else // ���Ǵ�����
            {
                bPrime = false;
                break;
            }
        }
        // �ҵ�������
        if (bPrime)
        {
            break;
        }
    }
    return nBig; // ���ش�����
}

// ���ɹ�Կ��˽Կ n,e,d
void GenKeyPair(CBigNum &PublicMod, CBigNum &PublicKey, CBigNum &PrivateKey, CBigNum &P, CBigNum &Q, unsigned int nByteCount)
{
    if (0U==(P | Q))
    {
        P=FindABigPrime(nByteCount); // ����nByteCountλ�Ĵ�����
        Q=FindABigPrime(nByteCount); // ����nByteCountλ�Ĵ�����
        PublicKey=GenerateBigRandomNumber(nByteCount) | 1U;
    } else {
        PublicKey |= 1U;
    }
    PrivateKey = (P-1U) * (Q-1U); // ��ʼ��˽Կ phi(n)
    while (PublicKey > PrivateKey)
        PublicKey=GenerateBigRandomNumber(nByteCount-1) | 1U;
    while(CBigNum::gcd(PublicKey,PrivateKey) != 1U)
        PublicKey+=2; // �ۼ�ֱ�����صõ���Կ e
    PrivateKey = PublicKey.Inverse(PrivateKey); // ���e����r��ģ��Ԫ�أ�����˽Կ d

    PublicMod = P*Q; // ��Կn������
}


// �����ض���ʽ�Ĺ�Կ��˽Կ n,b,a
void GenerateKeys(CBigNumString &PublicMod, CBigNumString &PublicKey, CBigNumString &PrivateKey, unsigned short nBytes)
{
    CBigNum PubMod, PubKey, PriKey, PriP, PriQ;
    MakeSmallPrimes();
    GenKeyPair(PubMod, PubKey, PriKey, PriP, PriQ, nBytes); // ���ɹ�Կ��˽Կ
    // ����Կ��˽Կת����16������ʽ
    PublicMod = PubMod.ToHexString();
    PublicKey = PubKey.ToHexString();
    PrivateKey = PriKey.ToHexString();
}


// RSA���ܺ��� E(x)=x^b mod n
void RSAEncrypt(char *publickey,char *publicmod, char *output, unsigned int *outputlen, char *input, unsigned int inputlen)
{
    CBigNum Transform;
    CBigNum PubMod, PubKey;
    CBigNumString strTransform;
    // ����Կת���ɴ���
    PubMod = CBigNum::FromHexString(publicmod);
    PubKey = CBigNum::FromHexString(publickey);

    // ת�����������
    Transform = Transform.FromByteString(input);
    // ʹ��RSA��������
    Transform = Transform.PowMod(PubKey,PubMod);
    // ������ת����16���Ƶ��ַ�
    strTransform = Transform.ToHexString();

    // ������ĳ���
    *outputlen = strlen((const char*)strTransform)+1;
    // �������
    memcpy(output,(const char*)strTransform,(*outputlen)+1);
}

// RSA���� D(y)=y^a mod n
void RSADecrypt(char *output, unsigned int *outputlen, char *input, unsigned int inputlen)
{
    CBigNum Transform;
    CBigNum PubMod,PriKey;
    CBigNumString strTransform;
    // ��˽Կת���ɴ���
    PubMod = CBigNum::FromHexString((const char*)strMod);
    PriKey = CBigNum::FromHexString((const char*)strPriKey);
    // ת�����������
    Transform = Transform.FromHexString(input);
    // ʹ��RSA�����Ľ��н���
    Transform = Transform.PowMod(PriKey,PubMod);
    // ��������ת�����ֽ��ַ���
    strTransform = Transform.ToByteString();

    // ��������ĳ���
    *outputlen = strlen((const char*)strTransform)+1;
    // ���������
    memcpy(output,(const char*)strTransform,(*outputlen)+1);


}
/*// RSA ǩ�� s=E(H(m))
void RSASign(char *publickey,char *publicmod, char *sig, unsigned int *siglen, char *input)
{
    uint8_t digest[300];
    char digest_str[300];
    // ����ժҪ
    sha256(input,digest);
    hexdigest(digest,digest_str);
    printf("Digest:\n");
    printf("%s\n\n\n",digest_str);
    // ʹ�� RSA ������ϢժҪ���õ�ǩ��
    RSAEncrypt(publickey,publicmod,sig,siglen,digest_str, sizeof(digest_str));
    printf("Signature:\n");
    printf("%s\n\n\n",sig);
}
// RSA ��ǩ
void RSAVerify(char * decrypt,char * sig, unsigned int siglen)
{
    uint8_t new_digest[300];
    char new_digest_str[300];
    char dec_sig[300];
    unsigned int dec_sig_len;
    // �Խ����ļ���ժҪ
    sha256(decrypt,new_digest);
    hexdigest(new_digest,new_digest_str);
    printf("Recalculated digest:\n");
    printf("%s\n\n\n",new_digest_str);
    // ����ǩ���ָ�ԭժҪ
    RSADecrypt(dec_sig,&dec_sig_len,sig,siglen);
    printf("Recovered digest:\n");
    printf("%s\n\n\n",dec_sig);
    if(strcmp(dec_sig,new_digest_str)==0){
        printf("Signature verified!\n\n\n");
    }else{
        printf("Signature not verified!\n\n\n");
    }
}
 */
int main(int argc, char* argv[])
{
    char pubkey[300]; // ��Կ
    char pubmod[300]; // ��Կ
    char encrypt_text[300]; // ���������
    char decrypt_text[300]; // ����Ľ�����
    //char signature[300];// ǩ��

    unsigned int encrypt_len;
    unsigned int decrypt_len;
    //unsigned int signature_len;
    char plain_text[50];
    strcpy(plain_text,"A message less than 32 chars~");
    // ������Կ��˽Կ
    GenerateKeys(strMod,strPubKey,strPriKey,16);
    // ���ù�Կ�ַ���
    memcpy(pubkey,(const char*)strPubKey,strlen((const char*)strPubKey)+1);
    memcpy(pubmod,(const char*)strMod,strlen((const char*)strMod)+1);

    RSAEncrypt(pubkey,pubmod,encrypt_text,&encrypt_len,plain_text,sizeof(plain_text));
    RSADecrypt(decrypt_text,&decrypt_len,encrypt_text,encrypt_len);


    printf("public key n :  %s\n",pubmod);
    printf("public key e :  %s\n",pubkey);
    printf("Plain text:\n");
    printf("%s\n\n\n",plain_text);
    printf("cipher:\n");
    printf("%s\n\n\n",encrypt_text);

    //RSASign(pubkey,pubmod,signature,&signature_len,plain_text);

    //RSAVerify(decrypt_text,signature,signature_len);
    printf("output for decryption:\n");
    printf("%s\n\n\n",decrypt_text);
    return 0;
}



