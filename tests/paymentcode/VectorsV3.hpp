// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/core/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"

namespace ot = opentxs;

namespace ottest
{
struct VectorV3 {
    ot::UnallocatedCString words_{};
    ot::UnallocatedCString payment_code_{};
    ot::UnallocatedVector<ot::UnallocatedCString> locators_{};
    ot::blockchain::Type receive_chain_{};
    ot::UnallocatedVector<ot::UnallocatedCString> receive_keys_{};
    ot::UnallocatedCString change_key_secret_{};
    ot::UnallocatedCString change_key_public_{};
    ot::UnallocatedCString blinded_payment_code_{};
    ot::UnallocatedCString F_{};
    ot::UnallocatedCString G_{};
};

struct VectorsV3 {
    ot::UnallocatedCString outpoint_{};
    VectorV3 alice_{};
    VectorV3 bob_{};
    VectorV3 chris_{};
    VectorV3 daniel_{};
};

auto GetVectors3() noexcept -> const VectorsV3&;
}  // namespace ottest
