// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/webcrypto_impl.h"

#include "base/logging.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/shared_crypto.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

using webcrypto::Status;

namespace {

void CompleteWithError(const Status& status, blink::WebCryptoResult* result) {
  DCHECK(status.IsError());
  if (status.HasErrorDetails())
    result->completeWithError(blink::WebString::fromUTF8(status.ToString()));
  else
    result->completeWithError();
}

bool IsAlgorithmAsymmetric(const blink::WebCryptoAlgorithm& algorithm) {
  // TODO(padolph): include all other asymmetric algorithms once they are
  // defined, e.g. EC and DH.
  return (algorithm.id() == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 ||
          algorithm.id() == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
          algorithm.id() == blink::WebCryptoAlgorithmIdRsaOaep);
}

}  // namespace

WebCryptoImpl::WebCryptoImpl() { webcrypto::Init(); }

WebCryptoImpl::~WebCryptoImpl() {}

void WebCryptoImpl::encrypt(const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const unsigned char* data,
                            unsigned int data_size,
                            blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  Status status = webcrypto::Encrypt(
      algorithm, key, webcrypto::CryptoData(data, data_size), &buffer);
  if (status.IsError())
    CompleteWithError(status, &result);
  else
    result.completeWithBuffer(buffer);
}

void WebCryptoImpl::decrypt(const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const unsigned char* data,
                            unsigned int data_size,
                            blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  Status status = webcrypto::Decrypt(
      algorithm, key, webcrypto::CryptoData(data, data_size), &buffer);
  if (status.IsError())
    CompleteWithError(status, &result);
  else
    result.completeWithBuffer(buffer);
}

void WebCryptoImpl::digest(const blink::WebCryptoAlgorithm& algorithm,
                           const unsigned char* data,
                           unsigned int data_size,
                           blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  Status status = webcrypto::Digest(
      algorithm, webcrypto::CryptoData(data, data_size), &buffer);
  if (status.IsError())
    CompleteWithError(status, &result);
  else
    result.completeWithBuffer(buffer);
}

void WebCryptoImpl::generateKey(const blink::WebCryptoAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usage_mask,
                                blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  if (IsAlgorithmAsymmetric(algorithm)) {
    blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
    blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
    Status status = webcrypto::GenerateKeyPair(
        algorithm, extractable, usage_mask, &public_key, &private_key);
    if (status.IsError()) {
      CompleteWithError(status, &result);
    } else {
      DCHECK(public_key.handle());
      DCHECK(private_key.handle());
      DCHECK_EQ(algorithm.id(), public_key.algorithm().id());
      DCHECK_EQ(algorithm.id(), private_key.algorithm().id());
      DCHECK_EQ(true, public_key.extractable());
      DCHECK_EQ(extractable, private_key.extractable());
      DCHECK_EQ(usage_mask, public_key.usages());
      DCHECK_EQ(usage_mask, private_key.usages());
      result.completeWithKeyPair(public_key, private_key);
    }
  } else {
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    Status status =
        webcrypto::GenerateSecretKey(algorithm, extractable, usage_mask, &key);
    if (status.IsError()) {
      CompleteWithError(status, &result);
    } else {
      DCHECK(key.handle());
      DCHECK_EQ(algorithm.id(), key.algorithm().id());
      DCHECK_EQ(extractable, key.extractable());
      DCHECK_EQ(usage_mask, key.usages());
      result.completeWithKey(key);
    }
  }
}

void WebCryptoImpl::importKey(blink::WebCryptoKeyFormat format,
                              const unsigned char* key_data,
                              unsigned int key_data_size,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usage_mask,
                              blink::WebCryptoResult result) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  Status status =
      webcrypto::ImportKey(format,
                           webcrypto::CryptoData(key_data, key_data_size),
                           algorithm,
                           extractable,
                           usage_mask,
                           &key);
  if (status.IsError()) {
    CompleteWithError(status, &result);
  } else {
    DCHECK(key.handle());
    DCHECK(!key.algorithm().isNull());
    DCHECK_EQ(extractable, key.extractable());
    result.completeWithKey(key);
  }
}

void WebCryptoImpl::exportKey(blink::WebCryptoKeyFormat format,
                              const blink::WebCryptoKey& key,
                              blink::WebCryptoResult result) {
  blink::WebArrayBuffer buffer;
  Status status = webcrypto::ExportKey(format, key, &buffer);
  if (status.IsError())
    CompleteWithError(status, &result);
  else
    result.completeWithBuffer(buffer);
}

void WebCryptoImpl::sign(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const unsigned char* data,
                         unsigned int data_size,
                         blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  Status status = webcrypto::Sign(
      algorithm, key, webcrypto::CryptoData(data, data_size), &buffer);
  if (status.IsError())
    CompleteWithError(status, &result);
  else
    result.completeWithBuffer(buffer);
}

void WebCryptoImpl::verifySignature(const blink::WebCryptoAlgorithm& algorithm,
                                    const blink::WebCryptoKey& key,
                                    const unsigned char* signature,
                                    unsigned int signature_size,
                                    const unsigned char* data,
                                    unsigned int data_size,
                                    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  bool signature_match = false;
  Status status = webcrypto::VerifySignature(
      algorithm,
      key,
      webcrypto::CryptoData(signature, signature_size),
      webcrypto::CryptoData(data, data_size),
      &signature_match);
  if (status.IsError())
    CompleteWithError(status, &result);
  else
    result.completeWithBoolean(signature_match);
}

void WebCryptoImpl::wrapKey(blink::WebCryptoKeyFormat format,
                            const blink::WebCryptoKey& key,
                            const blink::WebCryptoKey& wrapping_key,
                            const blink::WebCryptoAlgorithm& wrap_algorithm,
                            blink::WebCryptoResult result) {
  blink::WebArrayBuffer buffer;
  // TODO(eroman): Use the same parameter ordering.
  Status status = webcrypto::WrapKey(
      format, wrapping_key, key, wrap_algorithm, &buffer);
  if (status.IsError())
    CompleteWithError(status, &result);
  else
    result.completeWithBuffer(buffer);
}

void WebCryptoImpl::unwrapKey(
    blink::WebCryptoKeyFormat format,
    const unsigned char* wrapped_key,
    unsigned wrapped_key_size,
    const blink::WebCryptoKey& wrapping_key,
    const blink::WebCryptoAlgorithm& unwrap_algorithm,
    const blink::WebCryptoAlgorithm& unwrapped_key_algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoResult result) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  Status status =
      webcrypto::UnwrapKey(format,
                           webcrypto::CryptoData(wrapped_key, wrapped_key_size),
                           wrapping_key,
                           unwrap_algorithm,
                           unwrapped_key_algorithm,
                           extractable,
                           usages,
                           &key);
  if (status.IsError())
    CompleteWithError(status, &result);
  else
    result.completeWithKey(key);
}

bool WebCryptoImpl::digestSynchronous(
    const blink::WebCryptoAlgorithmId algorithm_id,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer& result) {
  blink::WebCryptoAlgorithm algorithm =
      blink::WebCryptoAlgorithm::adoptParamsAndCreate(algorithm_id, NULL);
  return (webcrypto::Digest(
              algorithm, webcrypto::CryptoData(data, data_size), &result))
      .IsSuccess();
}

bool WebCryptoImpl::deserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const unsigned char* key_data,
    unsigned key_data_size,
    blink::WebCryptoKey& key) {
  Status status = webcrypto::DeserializeKeyForClone(
      algorithm,
      type,
      extractable,
      usages,
      webcrypto::CryptoData(key_data, key_data_size),
      &key);
  return status.IsSuccess();
}

bool WebCryptoImpl::serializeKeyForClone(
    const blink::WebCryptoKey& key,
    blink::WebVector<unsigned char>& key_data) {
  Status status = webcrypto::SerializeKeyForClone(key, &key_data);
  return status.IsSuccess();
}

}  // namespace content
