#define MCLBN_DONT_EXPORT
#define BLS_DLL_EXPORT

#include <bls/bls.h>

#if 1
#include "mcl/impl/bn_c_impl.hpp"
#else
#if MCLBN_FP_UNIT_SIZE == 4 && MCLBN_FR_UNIT_SIZE == 4
#include <mcl/bn256.hpp>
#elif MCLBN_FP_UNIT_SIZE == 6 && MCLBN_FR_UNIT_SIZE == 6
#include <mcl/bn384.hpp>
#elif MCLBN_FP_UNIT_SIZE == 6 && MCLBN_FR_UNIT_SIZE == 4
#include <mcl/bls12_381.hpp>
#elif MCLBN_FP_UNIT_SIZE == 8 && MCLBN_FR_UNIT_SIZE == 8
#include <mcl/bn512.hpp>
#else
	#error "not supported size"
#endif
#include <mcl/lagrange.hpp>
using namespace mcl::bn;
inline Fr *cast(mclBnFr *p) { return reinterpret_cast<Fr*>(p); }
inline const Fr *cast(const mclBnFr *p) { return reinterpret_cast<const Fr*>(p); }

inline G1 *cast(mclBnG1 *p) { return reinterpret_cast<G1*>(p); }
inline const G1 *cast(const mclBnG1 *p) { return reinterpret_cast<const G1*>(p); }

inline G2 *cast(mclBnG2 *p) { return reinterpret_cast<G2*>(p); }
inline const G2 *cast(const mclBnG2 *p) { return reinterpret_cast<const G2*>(p); }

inline Fp12 *cast(mclBnGT *p) { return reinterpret_cast<Fp12*>(p); }
inline const Fp12 *cast(const mclBnGT *p) { return reinterpret_cast<const Fp12*>(p); }

inline Fp6 *cast(uint64_t *p) { return reinterpret_cast<Fp6*>(p); }
inline const Fp6 *cast(const uint64_t *p) { return reinterpret_cast<const Fp6*>(p); }
#endif

inline void Gmul(G1& z, const G1& x, const Fr& y) { G1::mul(z, x, y); }
inline void Gmul(G2& z, const G2& x, const Fr& y) { G2::mul(z, x, y); }
inline void GmulCT(G1& z, const G1& x, const Fr& y) { G1::mulCT(z, x, y); }
inline void GmulCT(G2& z, const G2& x, const Fr& y) { G2::mulCT(z, x, y); }
inline void hashAndMapToG(G1& z, const void *m, mclSize size) { hashAndMapToG1(z, m, size); }
inline void hashAndMapToG(G2& z, const void *m, mclSize size) { hashAndMapToG2(z, m, size); }

/*
	BLS signature
	e : G1 x G2 -> GT
	Q in G2 ; fixed global parameter
	H : {str} -> G1
	s : secret key
	sQ ; public key
	s H(m) ; signature of m
	verify ; e(sQ, H(m)) = e(Q, s H(m))

	swap G1 and G2 if BLS_SWAP_G is defined
	@note the current implementation does not support precomputed miller loop
*/
/*
	ETH2 spec requires the original G2cofactor
	original cofactor = mulByCofactorBLS12fast * adj
	adj H(m) ; original version
	H(m) ; fast version
	PublicKey = s P
	Signature = s (adj H(m)) ; slow
	          = (s adj) H(m) ; fast
	Verify original ; e(P, s adj H(m)) == e(s P, adj H(m))
	fast version    ; e((1/adj) P, s H(m)) == e(s P, H(m))
*/

static int g_curveType;
#ifdef BLS_SWAP_G
typedef G2 G;
static G1 g_P;
static int g_adjInvIdx;
static G1 g_PadjInv[2]; // 0:g_P, 1:g_P * adj
#ifdef BLS_ETH
static bool g_newEth2;
#endif
inline const G1& getBasePoint() { return g_P; }
inline const G1& getBasePointAdjInv() { return g_PadjInv[g_adjInvIdx]; } // for only BLS12-381
#else
typedef G1 G;
static G2 g_Q;
const size_t maxQcoeffN = 128;
static mcl::FixedArray<Fp6, maxQcoeffN> g_Qcoeff; // precomputed Q
inline const G2& getBasePoint() { return g_Q; }
inline const G2& getBasePointAdjInv() { return getBasePoint(); } // same
inline const mcl::FixedArray<Fp6, maxQcoeffN>& getQcoeff() { return g_Qcoeff; }
#endif

int blsSetETHmode(int mode)
{
#ifdef BLS_ETH
	if (g_curveType != MCL_BLS12_381) return -1;
	switch (mode) {
	case BLS_ETH_MODE_OLD:
		g_newEth2 = false;
		g_adjInvIdx = 1;
		mclBn_setMapToMode(MCL_MAP_TO_MODE_ETH2);
		break;
	case BLS_ETH_MODE_DRAFT_05:
		g_newEth2 = true;
		g_adjInvIdx = 0;
		mclBn_setMapToMode(MCL_MAP_TO_MODE_HASH_TO_CURVE_05);
		break;
	case BLS_ETH_MODE_DRAFT_06:
		g_newEth2 = true;
		g_adjInvIdx = 0;
		mclBn_setMapToMode(MCL_MAP_TO_MODE_HASH_TO_CURVE_06);
		break;
	default:
		return -1;
	}
	return 0;
#else
	(void)mode;
	return -1;
#endif
}

int blsInit(int curve, int compiledTimeVar)
{
	if (compiledTimeVar != MCLBN_COMPILED_TIME_VAR) {
		return -(compiledTimeVar + (MCLBN_COMPILED_TIME_VAR * 1000));
	}
	const mcl::CurveParam& cp = mcl::getCurveParam(curve);
	bool b;
	initPairing(&b, cp);
	if (!b) return -1;
	g_curveType = curve;

#ifdef BLS_SWAP_G
	#ifdef BLS_ETH
	g_newEth2 = false;
	if (curve == MCL_BLS12_381) {
		mclBn_setETHserialization(1);
		g_P.setStr(&b, "1 3685416753713387016781088315183077757961620795782546409894578378688607592378376318836054947676345821548104185464507 1339506544944476473020471379941921221584933875938349620426543736416511423956333506472724655353366534992391756441569", 10);
		mclBn_setMapToMode(MCL_MAP_TO_MODE_ETH2);
		g_PadjInv[0] = g_P;
		G1::mul(g_PadjInv[1], g_P, mcl::bn::getG2cofactorAdjInv());
		g_adjInvIdx = 1;
	} else
	#endif
	{
		mapToG1(&b, g_P, 1);
		g_PadjInv[0] = g_P;
		g_adjInvIdx = 0;
	}
#else

	if (curve == MCL_BN254) {
		const char *Qx_BN254 = "11ccb44e77ac2c5dc32a6009594dbe331ec85a61290d6bbac8cc7ebb2dceb128 f204a14bbdac4a05be9a25176de827f2e60085668becdd4fc5fa914c9ee0d9a";
		const char *Qy_BN254 = "7c13d8487903ee3c1c5ea327a3a52b6cc74796b1760d5ba20ed802624ed19c8 8f9642bbaacb73d8c89492528f58932f2de9ac3e80c7b0e41f1a84f1c40182";
		g_Q.x.setStr(&b, Qx_BN254, 16);
		g_Q.y.setStr(&b, Qy_BN254, 16);
		g_Q.z = 1;
	} else {
		mapToG2(&b, g_Q, 1);
	}
	if (!b) return -100;
#if MCL_SIZEOF_UNIT == 8
	if (curve == MCL_BN254) {
		#include "./qcoeff-bn254.hpp"
		g_Qcoeff.resize(BN::param.precomputedQcoeffSize);
		assert(g_Qcoeff.size() == CYBOZU_NUM_OF_ARRAY(QcoeffTblBN254));
		for (size_t i = 0; i < g_Qcoeff.size(); i++) {
			Fp6& x6 = g_Qcoeff[i];
			for (size_t j = 0; j < 6; j++) {
				Fp& x = x6.getFp0()[j];
				mcl::fp::Unit *p = const_cast<mcl::fp::Unit*>(x.getUnit());
				for (size_t k = 0; k < 4; k++) {
					p[k] = QcoeffTblBN254[i][j][k];
				}
			}
		}
	} else
#endif
	{
		precomputeG2(&b, g_Qcoeff, getBasePoint());
	}
#endif
	if (!b) return -101;
	return 0;
}

void blsSetETHserialization(int ETHserialization)
{
	mclBn_setETHserialization(ETHserialization);
}

#ifdef __EMSCRIPTEN__
extern "C" BLS_DLL_API void *blsMalloc(size_t n)
{
	return malloc(n);
}
extern "C" BLS_DLL_API void blsFree(void *p)
{
	free(p);
}
#endif

static inline const mclBnG1 *cast(const G1* x) { return (const mclBnG1*)x; }
static inline const mclBnG2 *cast(const G2* x) { return (const mclBnG2*)x; }

void blsIdSetInt(blsId *id, int x)
{
	*cast(&id->v) = x;
}

int blsSecretKeySetLittleEndian(blsSecretKey *sec, const void *buf, mclSize bufSize)
{
	cast(&sec->v)->setArrayMask((const char *)buf, bufSize);
	return 0;
}
int blsSecretKeySetLittleEndianMod(blsSecretKey *sec, const void *buf, mclSize bufSize)
{
	bool b;
	cast(&sec->v)->setArray(&b, (const char *)buf, bufSize, mcl::fp::Mod);
	return b ? 0 : -1;
}

void blsGetPublicKey(blsPublicKey *pub, const blsSecretKey *sec)
{
	Gmul(*cast(&pub->v), getBasePoint(), *cast(&sec->v));
}

int blsHashToSignature(blsSignature *sig, const void *buf, mclSize bufSize)
{
	hashAndMapToG(*cast(&sig->v), buf, bufSize);
	return 0;
}

void blsSign(blsSignature *sig, const blsSecretKey *sec, const void *m, mclSize size)
{
	blsHashToSignature(sig, m, size);
	Fr s = *cast(&sec->v);
#ifdef BLS_ETH
	if (g_curveType == MCL_BLS12_381 && !g_newEth2) {
		s *= mcl::bn::getG2cofactorAdj();
	}
#endif
	GmulCT(*cast(&sig->v), *cast(&sig->v), s);
}

#ifdef BLS_SWAP_G
/*
	e(P, sHm) == e(sP, Hm)
	<=> finalExp(ML(P, sHm) * e(-sP, Hm)) == 1
*/
bool isEqualTwoPairings(const G2& sHm, const G1& sP, const G2& Hm)
{
	GT e;
	G1 v1[2];
	G2 v2[2] = { sHm, Hm };
	v1[0] = getBasePointAdjInv();
	G1::neg(v1[1], sP);
	millerLoopVec(e, v1, v2, 2);
	finalExp(e, e);
	return e.isOne();
}
#else
/*
	e(P1, Q1) == e(P2, Q2)
	<=> finalExp(ML(P1, Q1)) == finalExp(ML(P2, Q2))
	<=> finalExp(ML(P1, Q1) / ML(P2, Q2)) == 1
	<=> finalExp(ML(P1, Q1) * ML(-P2, Q2)) == 1
	Q1 is precomputed
*/
bool isEqualTwoPairings(const G1& P1, const Fp6* Q1coeff, const G1& P2, const G2& Q2)
{
	GT e;
	precomputedMillerLoop2mixed(e, P2, Q2, -P1, Q1coeff);
	finalExp(e, e);
	return e.isOne();
}
#endif

int blsVerify(const blsSignature *sig, const blsPublicKey *pub, const void *m, mclSize size)
{
	G Hm;
	hashAndMapToG(Hm, m, size);
#ifdef BLS_SWAP_G
	return isEqualTwoPairings(*cast(&sig->v), *cast(&pub->v), Hm);
#else
	/*
		e(sHm, Q) = e(Hm, sQ)
		e(sig, Q) = e(Hm, pub)
	*/
	return isEqualTwoPairings(*cast(&sig->v), getQcoeff().data(), Hm, *cast(&pub->v));
#endif
}

void blsAggregateSignature(blsSignature *aggSig, const blsSignature *sigVec, mclSize n)
{
	if (n == 0) {
		memset(aggSig, 0, sizeof(*aggSig));
		return;
	}
	*aggSig = sigVec[0];
	for (mclSize i = 1; i < n; i++) {
		blsSignatureAdd(aggSig, &sigVec[i]);
	}
}

void blsAggregatePublicKey(blsPublicKey *aggPub, const blsPublicKey *pubVec, mclSize n)
{
	if (n == 0) {
		memset(aggPub, 0, sizeof(*aggPub));
		return;
	}
	*aggPub = pubVec[0];
	for (mclSize i = 1; i < n; i++) {
		blsPublicKeyAdd(aggPub, &pubVec[i]);
	}
}

int blsFastAggregateVerify(const blsSignature *sig, const blsPublicKey *pubVec, mclSize n, const void *msg, mclSize msgSize)
{
	if (n == 0) return 0;
	blsPublicKey aggPub;
	blsAggregatePublicKey(&aggPub, pubVec, n);
	return blsVerify(sig, &aggPub, msg, msgSize);
}

int blsAggregateVerifyNoCheck(const blsSignature *sig, const blsPublicKey *pubVec, const void *msgVec, mclSize msgSize, mclSize n)
{
#ifdef BLS_ETH
	if (n == 0) return 0;
#if 1 // 1.1 times faster
	GT e1;
	const char *msg = (const char*)msgVec;
	const size_t N = 16;
	G1 g1Vec[N];
	G2 g2Vec[N];
	size_t start = 1; // 1 if first else 0

	g1Vec[0] = getBasePoint();
	G2::neg(g2Vec[0], *cast(&sig->v));
	while (n > 0) {
		size_t m = N - start;
		if (n < m) m = n;
		for (size_t i = 0; i < m; i++) {
			g1Vec[i + start] = *cast(&pubVec[i].v);
			hashAndMapToG(g2Vec[i + start], &msg[i * msgSize], msgSize);
		}
		if (start) {
			millerLoopVec(e1, g1Vec, g2Vec, m + start);
			start = 0;
		} else {
			GT e2;
			millerLoopVec(e2, g1Vec, g2Vec, m);
			e1 *= e2;
		}
		pubVec += m;
		msg += m * msgSize;
		n -= m;
	}
	BN::finalExp(e1, e1);
	return e1.isOne();
#else
	const char *p = (const char *)msgVec;
	GT s(1), t;
	for (mclSize i = 0; i < n; i++) {
		G2 Q;
		hashAndMapToG(Q, &p[msgSize * i], msgSize);
		millerLoop(t, *cast(&pubVec[i].v), Q);
		s *= t;
	}
	millerLoop(t, -getBasePoint(), *cast(&sig->v));
	s *= t;
	finalExp(s, s);
	return s.isOne() ? 1 : 0;
#endif
#else
	(void)sig;
	(void)pubVec;
	(void)msgVec;
	(void)msgSize;
	(void)n;
	return 0;
#endif
}

mclSize blsIdSerialize(void *buf, mclSize maxBufSize, const blsId *id)
{
	return cast(&id->v)->serialize(buf, maxBufSize);
}

mclSize blsSecretKeySerialize(void *buf, mclSize maxBufSize, const blsSecretKey *sec)
{
	return cast(&sec->v)->serialize(buf, maxBufSize);
}

mclSize blsPublicKeySerialize(void *buf, mclSize maxBufSize, const blsPublicKey *pub)
{
	return cast(&pub->v)->serialize(buf, maxBufSize);
}

mclSize blsSignatureSerialize(void *buf, mclSize maxBufSize, const blsSignature *sig)
{
	return cast(&sig->v)->serialize(buf, maxBufSize);
}

mclSize blsIdDeserialize(blsId *id, const void *buf, mclSize bufSize)
{
	return cast(&id->v)->deserialize(buf, bufSize);
}

mclSize blsSecretKeyDeserialize(blsSecretKey *sec, const void *buf, mclSize bufSize)
{
	return cast(&sec->v)->deserialize(buf, bufSize);
}

mclSize blsPublicKeyDeserialize(blsPublicKey *pub, const void *buf, mclSize bufSize)
{
	return cast(&pub->v)->deserialize(buf, bufSize);
}

mclSize blsSignatureDeserialize(blsSignature *sig, const void *buf, mclSize bufSize)
{
	return cast(&sig->v)->deserialize(buf, bufSize);
}

int blsIdIsEqual(const blsId *lhs, const blsId *rhs)
{
	return *cast(&lhs->v) == *cast(&rhs->v);
}

int blsSecretKeyIsEqual(const blsSecretKey *lhs, const blsSecretKey *rhs)
{
	return *cast(&lhs->v) == *cast(&rhs->v);
}

int blsPublicKeyIsEqual(const blsPublicKey *lhs, const blsPublicKey *rhs)
{
	return *cast(&lhs->v) == *cast(&rhs->v);
}

int blsSignatureIsEqual(const blsSignature *lhs, const blsSignature *rhs)
{
	return *cast(&lhs->v) == *cast(&rhs->v);
}

int blsSecretKeyShare(blsSecretKey *sec, const blsSecretKey* msk, mclSize k, const blsId *id)
{
	bool b;
	mcl::evaluatePolynomial(&b, *cast(&sec->v), cast(&msk->v), k, *cast(&id->v));
	return b ? 0 : -1;
}

int blsPublicKeyShare(blsPublicKey *pub, const blsPublicKey *mpk, mclSize k, const blsId *id)
{
	bool b;
	mcl::evaluatePolynomial(&b, *cast(&pub->v), cast(&mpk->v), k, *cast(&id->v));
	return b ? 0 : -1;
}

int blsSecretKeyRecover(blsSecretKey *sec, const blsSecretKey *secVec, const blsId *idVec, mclSize n)
{
	bool b;
	mcl::LagrangeInterpolation(&b, *cast(&sec->v), cast(&idVec->v), cast(&secVec->v), n);
	return b ? 0 : -1;
}

int blsPublicKeyRecover(blsPublicKey *pub, const blsPublicKey *pubVec, const blsId *idVec, mclSize n)
{
	bool b;
	mcl::LagrangeInterpolation(&b, *cast(&pub->v), cast(&idVec->v), cast(&pubVec->v), n);
	return b ? 0 : -1;
}

int blsSignatureRecover(blsSignature *sig, const blsSignature *sigVec, const blsId *idVec, mclSize n)
{
	bool b;
	mcl::LagrangeInterpolation(&b, *cast(&sig->v), cast(&idVec->v), cast(&sigVec->v), n);
	return b ? 0 : -1;
}

void blsSecretKeyAdd(blsSecretKey *sec, const blsSecretKey *rhs)
{
	*cast(&sec->v) += *cast(&rhs->v);
}

void blsPublicKeyAdd(blsPublicKey *pub, const blsPublicKey *rhs)
{
	*cast(&pub->v) += *cast(&rhs->v);
}

void blsSignatureAdd(blsSignature *sig, const blsSignature *rhs)
{
	*cast(&sig->v) += *cast(&rhs->v);
}

void blsSignatureVerifyOrder(int doVerify)
{
#ifdef BLS_SWAP_G
	verifyOrderG2(doVerify != 0);
#else
	verifyOrderG1(doVerify != 0);
#endif
}
void blsPublicKeyVerifyOrder(int doVerify)
{
#ifdef BLS_SWAP_G
	verifyOrderG1(doVerify != 0);
#else
	verifyOrderG2(doVerify != 0);
#endif
}
int blsSignatureIsValidOrder(const blsSignature *sig)
{
	return cast(&sig->v)->isValidOrder();
}
int blsPublicKeyIsValidOrder(const blsPublicKey *pub)
{
	return cast(&pub->v)->isValidOrder();
}

#ifndef BLS_MINIMUM_API
template<class G>
inline bool toG(G& Hm, const void *h, mclSize size)
{
	bool b;
#ifdef BLS_ETH
	if (g_newEth2) {
		BN::hashAndMapToG2(Hm, h, size);
		return true;
	}
	Fp2 t;
	if (t.deserialize(h, size) == 0) return false;
	BN::mapToG2(&b, Hm, t, true);
#elif defined(BLS_SWAP_G)
	Fp t;
	t.setArrayMask((const char *)h, size);
	BN::mapToG2(&b, Hm, Fp2(t, 0));
#else
	Fp t;
	t.setArrayMask((const char *)h, size);
	BN::mapToG1(&b, Hm, t);
#endif
	return b;
}

int blsVerifyAggregatedHashes(const blsSignature *aggSig, const blsPublicKey *pubVec, const void *hVec, size_t sizeofHash, mclSize n)
{
	if (n == 0) return 0;
	GT e1;
	const char *ph = (const char*)hVec;
	const size_t N = 16;
	G1 g1Vec[N];
	G2 g2Vec[N];
#ifdef BLS_SWAP_G
	size_t start = 1; // 1 if first else 0

	g1Vec[0] = getBasePointAdjInv();
	G2::neg(g2Vec[0], *cast(&aggSig->v));
	while (n > 0) {
		size_t m = N - start;
		if (n < m) m = n;
		for (size_t i = 0; i < m; i++) {
			g1Vec[i + start] = *cast(&pubVec[i].v);
			if (!toG(g2Vec[i + start], &ph[i * sizeofHash], sizeofHash)) return 0;
		}
		if (start) {
			millerLoopVec(e1, g1Vec, g2Vec, m + start);
			start = 0;
		} else {
			GT e2;
			millerLoopVec(e2, g1Vec, g2Vec, m);
			e1 *= e2;
		}
		pubVec += m;
		ph += m * sizeofHash;
		n -= m;
	}
#else
	/*
		e(aggSig, Q) = prod_i e(hVec[i], pubVec[i])
		<=> finalExp(ML(-aggSig, Q) * prod_i ML(hVec[i], pubVec[i])) == 1
	*/
	BN::precomputedMillerLoop(e1, -*cast(&aggSig->v), g_Qcoeff.data());
	while (n > 0) {
		size_t m = N;
		if (n < m) m = n;
		for (size_t i = 0; i < m; i++) {
			if (!toG(g1Vec[i], &ph[i * sizeofHash], sizeofHash)) return 0;
			g2Vec[i] = *cast(&pubVec[i].v);
		}
		GT e2;
		millerLoopVec(e2, g1Vec, g2Vec, m);
		e1 *= e2;
		pubVec += m;
		ph += m * sizeofHash;
		n -= m;
	}
#endif
	BN::finalExp(e1, e1);
	return e1.isOne();
}

int blsSignHash(blsSignature *sig, const blsSecretKey *sec, const void *h, mclSize size)
{
	G Hm;
	if (!toG(Hm, h, size)) return -1;
	Fr s = *cast(&sec->v);
#ifdef BLS_ETH
	if (g_curveType == MCL_BLS12_381 && !g_newEth2) {
		s *= mcl::bn::getG2cofactorAdj();
	}
#endif
	GmulCT(*cast(&sig->v), Hm, s);
	return 0;
}

#ifdef BLS_ETH
const uint8_t ZERO_HEADER = 1 << 6;
const mclSize serializedPublicKeySize = 48;
const mclSize serializedSignatureSize = 48 * 2;
inline bool isZeroFormat(const uint8_t *buf, mclSize n)
{
	if (buf[0] != ZERO_HEADER) return false;
	for (mclSize i = 1; i < n; i++) {
		if (buf[i] != 0) return false;
	}
	return true;
}
#endif

mclSize blsPublicKeySerializeUncompressed(void *buf, mclSize maxBufSize, const blsPublicKey *pub)
{
#ifdef BLS_ETH
	if (g_curveType != MCL_BLS12_381) return 0;
	const mclSize retSize = serializedPublicKeySize * 2;
	if (maxBufSize < retSize) return 0;
	uint8_t *dst = (uint8_t*)buf;
	if (cast(&pub->v)->isZero()) {
		dst[0] = ZERO_HEADER;
		memset(dst + 1, 0, retSize - 1);
	} else {
		G1 x;
		G1::normalize(x, *cast(&pub->v));
		if (x.x.serialize(dst, serializedPublicKeySize) == 0) return 0;
		if (x.y.serialize(dst + serializedPublicKeySize, serializedPublicKeySize) == 0) return 0;
	}
	return retSize;
#else
	(void)buf;
	(void)maxBufSize;
	(void)pub;
	return 0;
#endif
}

mclSize blsSignatureSerializeUncompressed(void *buf, mclSize maxBufSize, const blsSignature *sig)
{
#ifdef BLS_ETH
	if (g_curveType != MCL_BLS12_381) return 0;
	const mclSize retSize = serializedSignatureSize * 2;
	if (maxBufSize < retSize) return 0;
	uint8_t *dst = (uint8_t*)buf;
	if (cast(&sig->v)->isZero()) {
		dst[0] = ZERO_HEADER;
		memset(dst + 1, 0, retSize - 1);
	} else {
		G2 x;
		G2::normalize(x, *cast(&sig->v));
		if (x.x.serialize(dst, serializedSignatureSize) == 0) return 0;
		if (x.y.serialize(dst + serializedSignatureSize, serializedSignatureSize) == 0) return 0;
	}
	return retSize;
#else
	(void)buf;
	(void)maxBufSize;
	(void)sig;
	return 0;
#endif
}

mclSize blsPublicKeyDeserializeUncompressed(blsPublicKey *pub, const void *buf, mclSize bufSize)
{
#ifdef BLS_ETH
	if (g_curveType != MCL_BLS12_381) return 0;
	const mclSize retSize = serializedPublicKeySize * 2;
	if (bufSize < retSize) return 0;
	const uint8_t *src = (const uint8_t*)buf;
	G1& x = *cast(&pub->v);
	if (isZeroFormat(src, retSize)) {
		x.clear();
	} else {
		if (x.x.deserialize(src, serializedPublicKeySize) == 0) return 0;
		if (x.y.deserialize(src + serializedPublicKeySize, serializedPublicKeySize) == 0) return 0;
		x.z = 1;
	}
	if (!x.isValid()) return 0;
	return retSize;
#else
	(void)pub;
	(void)buf;
	(void)bufSize;
	return 0;
#endif
}

mclSize blsSignatureDeserializeUncompressed(blsSignature *sig, const void *buf, mclSize bufSize)
{
#ifdef BLS_ETH
	if (g_curveType != MCL_BLS12_381) return 0;
	const mclSize retSize = serializedSignatureSize * 2;
	if (bufSize < retSize) return 0;
	const uint8_t *src = (const uint8_t*)buf;
	G2& x = *cast(&sig->v);
	if (isZeroFormat(src, retSize)) {
		x.clear();
	} else {
		if (x.x.deserialize(src, serializedSignatureSize) == 0) return 0;
		if (x.y.deserialize(src + serializedSignatureSize, serializedSignatureSize) == 0) return 0;
		x.z = 1;
	}
	if (!x.isValid()) return 0;
	return retSize;
#else
	(void)sig;
	(void)buf;
	(void)bufSize;
	return 0;
#endif
}

#ifdef BLS_ETH
/*
	0    16    48   64   96
	0..0 H(im) 0..0 H(re)
*/
inline void blsHashWithDomainToFp2(uint8_t buf[96], const uint8_t hashWithDomain[40])
{
	uint8_t msg[41];
	memcpy(msg, hashWithDomain, 40);
	memset(buf, 0, 16);

	msg[40] = '\x02';
	mcl::fp::sha256(buf + 16, 32, msg, 41); // im part

	memset(buf + 48, 0, 16);
	msg[40] = '\x01';
	mcl::fp::sha256(buf + 64, 32, msg, 41); // re part
}
#endif

int blsSignHashWithDomain(blsSignature *sig, const blsSecretKey *sec, const uint8_t hashWithDomain[40])
{
#ifdef BLS_ETH
	if (g_curveType != MCL_BLS12_381) return -1;
	uint8_t msg[96];
	blsHashWithDomainToFp2(msg, hashWithDomain);
	return blsSignHash(sig, sec, msg, sizeof(msg));
#else
	(void)sig;
	(void)sec;
	(void)hashWithDomain;
	return -1;
#endif
}

int blsVerifyHashWithDomain(const blsSignature *sig, const blsPublicKey *pub, const uint8_t hashWithDomain[40])
{
#ifdef BLS_ETH
	if (g_curveType != MCL_BLS12_381) return 0;
	uint8_t msg[96];
	blsHashWithDomainToFp2(msg, hashWithDomain);
	return blsVerifyHash(sig, pub, msg, sizeof(msg));
#else
	(void)sig;
	(void)pub;
	(void)hashWithDomain;
	return 0;
#endif
}

int blsVerifyAggregatedHashWithDomain(const blsSignature *aggSig, const blsPublicKey *pubVec, const unsigned char hashWithDomain[][40], mclSize n)
{
#ifdef BLS_ETH
	if (g_curveType != MCL_BLS12_381) return 0;
	if (n == 0) return 0;
	GT e1;
	const size_t N = 16;
	G1 g1Vec[N];
	G2 g2Vec[N];

	size_t start = 1; // 1 if first else 0

	g1Vec[0] = getBasePointAdjInv();
	G2::neg(g2Vec[0], *cast(&aggSig->v));
	while (n > 0) {
		size_t m = N - start;
		if (n < m) m = n;
		for (size_t i = 0; i < m; i++) {
			g1Vec[i + start] = *cast(&pubVec[i].v);
			uint8_t buf[96];
			blsHashWithDomainToFp2(buf, hashWithDomain[i]);
			if (!toG(g2Vec[i + start], buf, sizeof(buf))) return 0;
		}
		if (start) {
			millerLoopVec(e1, g1Vec, g2Vec, m + start);
			start = 0;
		} else {
			GT e2;
			millerLoopVec(e2, g1Vec, g2Vec, m);
			e1 *= e2;
		}
		pubVec += m;
		hashWithDomain += m;
		n -= m;
	}
	BN::finalExp(e1, e1);
	return e1.isOne();
#else
	(void)aggSig;
	(void)pubVec;
	(void)hashWithDomain;
	(void)n;
	return 0;
#endif
}

int blsVerifyPairing(const blsSignature *X, const blsSignature *Y, const blsPublicKey *pub)
{
#ifdef BLS_SWAP_G
	return isEqualTwoPairings(*cast(&X->v), *cast(&pub->v), *cast(&Y->v));
#else
	return isEqualTwoPairings(*cast(&X->v), getQcoeff().data(), *cast(&Y->v), *cast(&pub->v));
#endif
}

int blsVerifyHash(const blsSignature *sig, const blsPublicKey *pub, const void *h, mclSize size)
{
	blsSignature Hm;
	if (!toG(*cast(&Hm.v), h, size)) return 0;
	return blsVerifyPairing(sig, &Hm, pub);
}

void blsSecretKeySub(blsSecretKey *sec, const blsSecretKey *rhs)
{
	*cast(&sec->v) -= *cast(&rhs->v);
}

void blsPublicKeySub(blsPublicKey *pub, const blsPublicKey *rhs)
{
	*cast(&pub->v) -= *cast(&rhs->v);
}

void blsSignatureSub(blsSignature *sig, const blsSignature *rhs)
{
	*cast(&sig->v) -= *cast(&rhs->v);
}

mclSize blsGetOpUnitSize() // FpUint64Size
{
	return Fp::getUnitSize() * sizeof(mcl::fp::Unit) / sizeof(uint64_t);
}

int blsGetCurveOrder(char *buf, mclSize maxBufSize)
{
	return (int)Fr::getModulo(buf, maxBufSize);
}

int blsGetFieldOrder(char *buf, mclSize maxBufSize)
{
	return (int)Fp::getModulo(buf, maxBufSize);
}

int blsGetSerializedSecretKeyByteSize()
{
	return blsGetFrByteSize();
}

int blsGetSerializedPublicKeyByteSize()
{
#ifdef BLS_SWAP_G
	return blsGetG1ByteSize();
#else
	return blsGetG1ByteSize() * 2;
#endif
}

int blsGetSerializedSignatureByteSize()
{
#ifdef BLS_SWAP_G
	return blsGetG1ByteSize() * 2;
#else
	return blsGetG1ByteSize();
#endif
}

int blsGetG1ByteSize()
{
	return (int)Fp::getByteSize();
}

int blsGetFrByteSize()
{
	return (int)Fr::getByteSize();
}

void blsGetGeneratorOfPublicKey(blsPublicKey *pub)
{
	*cast(&pub->v) = getBasePoint();
}

int blsIdSetDecStr(blsId *id, const char *buf, mclSize bufSize)
{
	return cast(&id->v)->deserialize(buf, bufSize, 10) > 0 ? 0 : -1;
}
int blsIdSetHexStr(blsId *id, const char *buf, mclSize bufSize)
{
	return cast(&id->v)->deserialize(buf, bufSize, 16) > 0 ? 0 : -1;
}

int blsIdSetLittleEndian(blsId *id, const void *buf, mclSize bufSize)
{
	cast(&id->v)->setArrayMask((const char *)buf, bufSize);
	return 0;
}

mclSize blsIdGetDecStr(char *buf, mclSize maxBufSize, const blsId *id)
{
	return cast(&id->v)->getStr(buf, maxBufSize, 10);
}

mclSize blsIdGetHexStr(char *buf, mclSize maxBufSize, const blsId *id)
{
	return cast(&id->v)->getStr(buf, maxBufSize, 16);
}

int blsHashToSecretKey(blsSecretKey *sec, const void *buf, mclSize bufSize)
{
	cast(&sec->v)->setHashOf(buf, bufSize);
	return 0;
}

#ifndef MCL_DONT_USE_CSPRNG
int blsSecretKeySetByCSPRNG(blsSecretKey *sec)
{
	bool b;
	cast(&sec->v)->setByCSPRNG(&b);
	return b ? 0 : -1;
}
void blsSetRandFunc(void *self, unsigned int (*readFunc)(void *self, void *buf, unsigned int bufSize))
{
	mcl::fp::RandGen::setRandFunc(self, readFunc);
}
#endif

void blsGetPop(blsSignature *sig, const blsSecretKey *sec)
{
	blsPublicKey pub;
	blsGetPublicKey(&pub, sec);
	char buf[1024];
	mclSize n = cast(&pub.v)->serialize(buf, sizeof(buf));
	assert(n);
	blsSign(sig, sec, buf, n);
}

int blsVerifyPop(const blsSignature *sig, const blsPublicKey *pub)
{
	char buf[1024];
	mclSize n = cast(&pub->v)->serialize(buf, sizeof(buf));
	if (n == 0) return 0;
	return blsVerify(sig, pub, buf, n);
}

mclSize blsIdGetLittleEndian(void *buf, mclSize maxBufSize, const blsId *id)
{
	return cast(&id->v)->serialize(buf, maxBufSize);
}
int blsSecretKeySetDecStr(blsSecretKey *sec, const char *buf, mclSize bufSize)
{
	return cast(&sec->v)->deserialize(buf, bufSize, 10) > 0 ? 0 : -1;
}
int blsSecretKeySetHexStr(blsSecretKey *sec, const char *buf, mclSize bufSize)
{
	return cast(&sec->v)->deserialize(buf, bufSize, 16) > 0 ? 0 : -1;
}
mclSize blsSecretKeyGetLittleEndian(void *buf, mclSize maxBufSize, const blsSecretKey *sec)
{
	return cast(&sec->v)->serialize(buf, maxBufSize);
}
mclSize blsSecretKeyGetDecStr(char *buf, mclSize maxBufSize, const blsSecretKey *sec)
{
	return cast(&sec->v)->getStr(buf, maxBufSize, 10);
}
mclSize blsSecretKeyGetHexStr(char *buf, mclSize maxBufSize, const blsSecretKey *sec)
{
	return cast(&sec->v)->getStr(buf, maxBufSize, 16);
}
int blsPublicKeySetHexStr(blsPublicKey *pub, const char *buf, mclSize bufSize)
{
	return cast(&pub->v)->deserialize(buf, bufSize, 16) > 0 ? 0 : -1;
}
mclSize blsPublicKeyGetHexStr(char *buf, mclSize maxBufSize, const blsPublicKey *pub)
{
	return cast(&pub->v)->getStr(buf, maxBufSize, 16);
}
int blsSignatureSetHexStr(blsSignature *sig, const char *buf, mclSize bufSize)
{
	return cast(&sig->v)->deserialize(buf, bufSize, 16) > 0 ? 0 : -1;
}
mclSize blsSignatureGetHexStr(char *buf, mclSize maxBufSize, const blsSignature *sig)
{
	return cast(&sig->v)->getStr(buf, maxBufSize, 16);
}
void blsDHKeyExchange(blsPublicKey *out, const blsSecretKey *sec, const blsPublicKey *pub)
{
	GmulCT(*cast(&out->v), *cast(&pub->v), *cast(&sec->v));
}
int blsPublicKeySetCompressedStr(blsPublicKey *pub, const char *buf, mclSize bufSize)
{
    return cast(&pub->v)->deserialize(buf, bufSize, 2048) > 0 ? 0 : -1;
}
int blsSignatureSetCompressedStr(blsSignature *sig, const char *buf, mclSize bufSize)
{
    return cast(&sig->v)->deserialize(buf, bufSize, 2048) > 0 ? 0 : -1;
}
mclSize blsSignatureGetCompressedStr(char *buf, mclSize maxBufSize, const blsSignature *sig)
{
    return cast(&sig->v)->getStr(buf, maxBufSize, 2048);
}
mclSize blsPublicKeyGetCompressedStr(char *buf, mclSize maxBufSize, const blsPublicKey *pub)
{
    return cast(&pub->v)->getStr(buf, maxBufSize, 2048);
}
#endif

