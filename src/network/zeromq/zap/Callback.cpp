// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                     // IWYU pragma: associated
#include "1_Internal.hpp"                   // IWYU pragma: associated
#include "network/zeromq/zap/Callback.hpp"  // IWYU pragma: associated

#include <atomic>
#include <functional>
#include <utility>

#include "internal/network/zeromq/zap/Factory.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/network/zeromq/zap/Reply.hpp"
#include "opentxs/network/zeromq/zap/Request.hpp"
#include "opentxs/network/zeromq/zap/ZAP.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"

template class opentxs::Pimpl<opentxs::network::zeromq::zap::Callback>;

namespace opentxs::network::zeromq::zap
{
auto Callback::Factory(
    const UnallocatedCString& domain,
    const ReceiveCallback& callback) -> OTZMQZAPCallback
{
    auto output = OTZMQZAPCallback(new implementation::Callback());
    output->SetDomain(domain, callback);

    return output;
}

auto Callback::Factory() -> OTZMQZAPCallback
{
    return OTZMQZAPCallback(new implementation::Callback());
}
}  // namespace opentxs::network::zeromq::zap

namespace opentxs::network::zeromq::zap::implementation
{
Callback::Callback()
    : default_callback_(
          std::bind(&Callback::default_callback, this, std::placeholders::_1))
    , domains_()
    , domain_lock_()
    , policy_(Policy::Accept)
{
}

auto Callback::default_callback(const zap::Request& in) const -> Reply
{
    if (Policy::Accept == policy_.load()) {

        return factory::ZAPReply(in, zap::Status::Success, "OK");
    } else {

        return factory::ZAPReply(
            in, zap::Status::AuthFailure, "Unsupported domain");
    }
}

auto Callback::get_domain(const ReadView domain) const
    -> const Callback::Lambda&
{
    const auto key = UnallocatedCString{domain};
    auto lock = Lock{domain_lock_};

    try {

        return domains_.at(key);
    } catch (...) {

        return default_callback_;
    }
}

auto Callback::Process(const zap::Request& request) const -> Reply
{
    auto [valid, error] = request.Validate();

    if (false == valid) {
        LogError()(OT_PRETTY_CLASS())("Rejecting invalid request.").Flush();

        return factory::ZAPReply(request, Status::SystemError, error);
    }

    const auto& domain = get_domain(request.Domain());

    return domain(request);
}

auto Callback::SetDomain(
    const UnallocatedCString& domain,
    const ReceiveCallback& callback) const -> bool
{
    auto lock = Lock{domain_lock_};

    if (domain.empty()) {
        LogError()(OT_PRETTY_CLASS())("Invalid domain.").Flush();

        return false;
    }

    if (0 < domains_.count(domain)) {
        LogError()(OT_PRETTY_CLASS())("Domain ")(domain)(" already registered.")
            .Flush();

        return false;
    }

    return domains_.emplace(domain, callback).second;
}

auto Callback::SetPolicy(const Policy policy) const -> bool
{
    policy_.store(policy);

    return true;
}
}  // namespace opentxs::network::zeromq::zap::implementation
