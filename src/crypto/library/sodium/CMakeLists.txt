# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

target_sources(
  opentxs-common
  PRIVATE
    "${opentxs_SOURCE_DIR}/src/internal/crypto/library/Sodium.hpp"
    "Sodium.cpp"
    "Sodium.hpp"
)
target_include_directories(
  opentxs-common SYSTEM PRIVATE "${opentxs_SOURCE_DIR}/deps/argon2"
)
target_link_libraries(
  opentxs-common PRIVATE opentxs-argon2 unofficial-sodium::sodium
)
target_link_libraries(opentxs PUBLIC unofficial-sodium::sodium)
target_include_directories(
  opentxs-common SYSTEM PRIVATE "${opentxs_SOURCE_DIR}/deps/argon2"
)

if(ED25519_EXPORT)
  target_sources(opentxs-common PRIVATE "Ed25519.cpp")
else()
  target_sources(opentxs-common PRIVATE "noEd25519.cpp")
endif()

if(WIN32 AND OT_STATIC_DEPENDENCIES)
  target_compile_definitions(
    opentxs-common PRIVATE SODIUM_STATIC=1 SODIUM_EXPORT=
  )
endif()
