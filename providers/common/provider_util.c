/*
 * Copyright 2019-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* We need to use some engine deprecated APIs */
#define OPENSSL_SUPPRESS_DEPRECATED

#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include "prov/provider_util.h"
#include "prov/providercommonerr.h"
#include "internal/nelem.h"

void ossl_prov_cipher_reset(PROV_CIPHER *pc)
{
    EVP_CIPHER_free(pc->alloc_cipher);
    pc->alloc_cipher = NULL;
    pc->cipher = NULL;
    pc->engine = NULL;
}

int ossl_prov_cipher_copy(PROV_CIPHER *dst, const PROV_CIPHER *src)
{
    if (src->alloc_cipher != NULL && !EVP_CIPHER_up_ref(src->alloc_cipher))
        return 0;
    dst->engine = src->engine;
    dst->cipher = src->cipher;
    dst->alloc_cipher = src->alloc_cipher;
    return 1;
}

static int load_common(const OSSL_PARAM params[], const char **propquery,
                       ENGINE **engine)
{
    const OSSL_PARAM *p;

    *propquery = NULL;
    p = OSSL_PARAM_locate_const(params, OSSL_ALG_PARAM_PROPERTIES);
    if (p != NULL) {
        if (p->data_type != OSSL_PARAM_UTF8_STRING)
            return 0;
        *propquery = p->data;
    }

    *engine = NULL;
    /* TODO legacy stuff, to be removed */
    /* Inside the FIPS module, we don't support legacy ciphers */
#if !defined(FIPS_MODULE) && !defined(OPENSSL_NO_ENGINE)
    p = OSSL_PARAM_locate_const(params, OSSL_ALG_PARAM_ENGINE);
    if (p != NULL) {
        if (p->data_type != OSSL_PARAM_UTF8_STRING)
            return 0;
        ENGINE_finish(*engine);
        *engine = ENGINE_by_id(p->data);
        if (*engine == NULL)
            return 0;
    }
#endif
    return 1;
}

int ossl_prov_cipher_load_from_params(PROV_CIPHER *pc,
                                      const OSSL_PARAM params[],
                                      OPENSSL_CTX *ctx)
{
    const OSSL_PARAM *p;
    const char *propquery;

    if (!load_common(params, &propquery, &pc->engine))
        return 0;

    p = OSSL_PARAM_locate_const(params, OSSL_ALG_PARAM_CIPHER);
    if (p == NULL)
        return 1;
    if (p->data_type != OSSL_PARAM_UTF8_STRING)
        return 0;

    EVP_CIPHER_free(pc->alloc_cipher);
    ERR_set_mark();
    pc->cipher = pc->alloc_cipher = EVP_CIPHER_fetch(ctx, p->data, propquery);
    /* TODO legacy stuff, to be removed */
#ifndef FIPS_MODULE /* Inside the FIPS module, we don't support legacy ciphers */
    if (pc->cipher == NULL)
        pc->cipher = EVP_get_cipherbyname(p->data);
#endif
    if (pc->cipher != NULL)
        ERR_pop_to_mark();
    else
        ERR_clear_last_mark();
    return pc->cipher != NULL;
}

const EVP_CIPHER *ossl_prov_cipher_cipher(const PROV_CIPHER *pc)
{
    return pc->cipher;
}

ENGINE *ossl_prov_cipher_engine(const PROV_CIPHER *pc)
{
    return pc->engine;
}

void ossl_prov_digest_reset(PROV_DIGEST *pd)
{
    EVP_MD_free(pd->alloc_md);
    pd->alloc_md = NULL;
    pd->md = NULL;
    pd->engine = NULL;
}

int ossl_prov_digest_copy(PROV_DIGEST *dst, const PROV_DIGEST *src)
{
    if (src->alloc_md != NULL && !EVP_MD_up_ref(src->alloc_md))
        return 0;
    dst->engine = src->engine;
    dst->md = src->md;
    dst->alloc_md = src->alloc_md;
    return 1;
}

int ossl_prov_digest_load_from_params(PROV_DIGEST *pd,
                                      const OSSL_PARAM params[],
                                      OPENSSL_CTX *ctx)
{
    const OSSL_PARAM *p;
    const char *propquery;

    if (!load_common(params, &propquery, &pd->engine))
        return 0;


    p = OSSL_PARAM_locate_const(params, OSSL_ALG_PARAM_DIGEST);
    if (p == NULL)
        return 1;
    if (p->data_type != OSSL_PARAM_UTF8_STRING)
        return 0;

    EVP_MD_free(pd->alloc_md);
    ERR_set_mark();
    pd->md = pd->alloc_md = EVP_MD_fetch(ctx, p->data, propquery);
    /* TODO legacy stuff, to be removed */
#ifndef FIPS_MODULE /* Inside the FIPS module, we don't support legacy digests */
    if (pd->md == NULL)
        pd->md = EVP_get_digestbyname(p->data);
#endif
    if (pd->md != NULL)
        ERR_pop_to_mark();
    else
        ERR_clear_last_mark();
    return pd->md != NULL;
}

const EVP_MD *ossl_prov_digest_md(const PROV_DIGEST *pd)
{
    return pd->md;
}

ENGINE *ossl_prov_digest_engine(const PROV_DIGEST *pd)
{
    return pd->engine;
}

int ossl_prov_set_macctx(EVP_MAC_CTX *macctx,
                         const OSSL_PARAM params[],
                         const char *ciphername,
                         const char *mdname,
                         const char *engine,
                         const char *properties,
                         const unsigned char *key,
                         size_t keylen)
{
    const OSSL_PARAM *p;
    OSSL_PARAM mac_params[6], *mp = mac_params;

    if (params != NULL) {
        if (mdname == NULL) {
            if ((p = OSSL_PARAM_locate_const(params,
                                            OSSL_ALG_PARAM_DIGEST)) != NULL) {
                if (p->data_type != OSSL_PARAM_UTF8_STRING)
                    return 0;
                mdname = p->data;
            }
        }
        if (ciphername == NULL) {
            if ((p = OSSL_PARAM_locate_const(params,
                                            OSSL_ALG_PARAM_CIPHER)) != NULL) {
                if (p->data_type != OSSL_PARAM_UTF8_STRING)
                    return 0;
                ciphername = p->data;
            }
        }
        if (engine == NULL) {
            if ((p = OSSL_PARAM_locate_const(params, OSSL_ALG_PARAM_ENGINE))
                    != NULL) {
                if (p->data_type != OSSL_PARAM_UTF8_STRING)
                    return 0;
                engine = p->data;
            }
        }
    }

    if (mdname != NULL)
        *mp++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 (char *)mdname, 0);
    if (ciphername != NULL)
        *mp++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)ciphername, 0);
    if (properties != NULL)
        *mp++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_PROPERTIES,
                                                 (char *)properties, 0);

#if !defined(OPENSSL_NO_ENGINE) && !defined(FIPS_MODULE)
    if (engine != NULL)
        *mp++ = OSSL_PARAM_construct_utf8_string(OSSL_ALG_PARAM_ENGINE,
                                                 (char *) engine, 0);
#endif

    if (key != NULL)
        *mp++ = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY,
                                                  (unsigned char *)key,
                                                  keylen);

    *mp = OSSL_PARAM_construct_end();

    return EVP_MAC_CTX_set_params(macctx, mac_params);

}

int ossl_prov_macctx_load_from_params(EVP_MAC_CTX **macctx,
                                      const OSSL_PARAM params[],
                                      const char *macname,
                                      const char *ciphername,
                                      const char *mdname,
                                      OPENSSL_CTX *libctx)
{
    const OSSL_PARAM *p;
    const char *properties = NULL;

    if (macname == NULL
        && (p = OSSL_PARAM_locate_const(params, OSSL_ALG_PARAM_MAC)) != NULL) {
        if (p->data_type != OSSL_PARAM_UTF8_STRING)
            return 0;
        macname = p->data;
    }
    if ((p = OSSL_PARAM_locate_const(params,
                                     OSSL_ALG_PARAM_PROPERTIES)) != NULL) {
        if (p->data_type != OSSL_PARAM_UTF8_STRING)
            return 0;
        properties = p->data;
    }

    /* If we got a new mac name, we make a new EVP_MAC_CTX */
    if (macname != NULL) {
        EVP_MAC *mac = EVP_MAC_fetch(libctx, macname, properties);

        EVP_MAC_CTX_free(*macctx);
        *macctx = mac == NULL ? NULL : EVP_MAC_CTX_new(mac);
        /* The context holds on to the MAC */
        EVP_MAC_free(mac);
        if (*macctx == NULL)
            return 0;
    }

    /*
     * If there is no MAC yet (and therefore, no MAC context), we ignore
     * all other parameters.
     */
    if (*macctx == NULL)
        return 1;

    if (ossl_prov_set_macctx(*macctx, params, ciphername, mdname, NULL,
                             properties, NULL, 0))
        return 1;

    EVP_MAC_CTX_free(*macctx);
    *macctx = NULL;
    return 0;
}

void ossl_prov_cache_exported_algorithms(const OSSL_ALGORITHM_CAPABLE *in,
                                         OSSL_ALGORITHM *out)
{
    int i, j;

    if (out[0].algorithm_names == NULL) {
        for (i = j = 0; in[i].alg.algorithm_names != NULL; ++i) {
            if (in[i].capable == NULL || in[i].capable())
                out[j++] = in[i].alg;
        }
        out[j++] = in[i].alg;
    }
}

/*
 * Internal library code deals with NIDs, so we need to translate from a name.
 * We do so using EVP_MD_is_a(), and therefore need a name to NID map.
 */
int ossl_prov_digest_md_to_nid(const EVP_MD *md, const OSSL_ITEM *it,
                               size_t it_len)
{
    size_t i;
    int mdnid = NID_undef;

    if (md == NULL)
        goto end;

    for (i = 0; i < it_len; i++) {
        if (EVP_MD_is_a(md, it[i].ptr)) {
            mdnid = (int)it[i].id;
            break;
        }
    }
 end:
    return mdnid;
}

/*
 * Retrieve one of the FIPs approved hash algorithms by nid.
 * See FIPS 180-4 "Secure Hash Standard" and
 * FIPS 202 - SHA-3.
 *
 * NOTE: For some operations SHA1 is not allowed. This check is only enabled
 * for the FIPS_MODULE.
 */
int ossl_prov_digest_get_approved_nid(const EVP_MD *md, int sha1_allowed)
{
    int mdnid;

    static const OSSL_ITEM name_to_nid[] = {
        { NID_sha1,      OSSL_DIGEST_NAME_SHA1      },
        { NID_sha224,    OSSL_DIGEST_NAME_SHA2_224  },
        { NID_sha256,    OSSL_DIGEST_NAME_SHA2_256  },
        { NID_sha384,    OSSL_DIGEST_NAME_SHA2_384  },
        { NID_sha512,    OSSL_DIGEST_NAME_SHA2_512  },
        { NID_sha512_224, OSSL_DIGEST_NAME_SHA2_512_224 },
        { NID_sha512_256, OSSL_DIGEST_NAME_SHA2_512_256 },
        { NID_sha3_224,  OSSL_DIGEST_NAME_SHA3_224  },
        { NID_sha3_256,  OSSL_DIGEST_NAME_SHA3_256  },
        { NID_sha3_384,  OSSL_DIGEST_NAME_SHA3_384  },
        { NID_sha3_512,  OSSL_DIGEST_NAME_SHA3_512  },
    };

    mdnid = ossl_prov_digest_md_to_nid(md, name_to_nid, OSSL_NELEM(name_to_nid));
#ifdef FIPS_MODULE
    if (mdnid == NID_sha1 && !sha1_allowed)
        mdnid = NID_undef;
#endif
    return mdnid;
}

/*
 * In FIPS mode:
 * protect should be 1 for any operations that need 112 bits of security
 * strength (such as signing, and key exchange), or 0 for operations that allow
 * a lower security strength (such as verify).
 *
 * For ECDH key agreement refer to SP800-56A
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-56Ar3.pdf
 * "Appendix D"
 *
 * For ECDSA signatures refer to
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-131Ar2.pdf
 * "Table 2"
 */
int ossl_prov_ec_check(const EC_KEY *ec, int protect)
{
#ifdef FIPS_MODULE
    int nid, strength;
    const char *curve_name;
    const EC_GROUP *group = EC_KEY_get0_group(ec);

    if (group == NULL) {
        ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_CURVE, "No group");
        return 0;
    }
    nid = EC_GROUP_get_curve_name(group);
    if (nid == NID_undef) {
        ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_CURVE,
                       "Explicit curves are not allowed in fips mode");
        return 0;
    }

    curve_name = EC_curve_nid2nist(nid);
    if (curve_name == NULL) {
        ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_CURVE,
                       "Curve %s is not approved in FIPS mode", curve_name);
        return 0;
    }

    /*
     * For EC the security strength is the (order_bits / 2)
     * e.g. P-224 is 112 bits.
     */
    strength = EC_GROUP_order_bits(group) / 2;
    /* The min security strength allowed for legacy verification is 80 bits */
    if (strength < 80) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_CURVE);
        return 0;
    }

    /*
     * For signing/or key agreement only allow curves with at least 112 bits of
     * security strength
     */
    if (protect && strength < 112) {
        ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_CURVE,
                       "Curve %s cannot be used for signing", curve_name);
        return 0;
    }
#endif
    return 1;
}
