/**
 * @file openssl_compat.h
 * @brief OpenSSL compatibility and warning suppression
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

// Suppress OpenSSL 3.0 deprecation warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Include OpenSSL headers
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>

#pragma GCC diagnostic pop
